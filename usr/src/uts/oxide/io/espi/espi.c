/*
 * This file and its contents are supplied under the terms of the
 * Common Development and Distribution License ("CDDL"), version 1.0.
 * You may only use this file in accordance with the terms of version
 * 1.0 of the CDDL.
 *
 * A full copy of the text of the CDDL should have accompanied this
 * source.  A copy of the CDDL is also available via the Internet at
 * http://www.illumos.org/license/CDDL.
 */

/*
 * Copyright 2026 Oxide Computer Company
 */

/*
 * eSPI Nexus Driver
 *
 * This driver attaches to the eSPI controllers in the FCH, which appear as
 * children of fch(4D) named espi@<unit>. An eSPI controller talks to a single
 * target over one of up to four channels, and the children of this nexus are
 * the drivers for those channels. This file is the controller side of that.
 *
 * Controller Roles
 *
 * Not every controller has a target behind it, and one of them is normally
 * used by the kernel itself to talk to the service processor (IPCC), without
 * going through the DDI at all. The board data (oxide_board_data->obd_espi)
 * says which is which. A controller with no target does not attach at all. We
 * decline in probe(9E) so that the node remains in the tree without an
 * instance. A controller used for IPCC attaches in passive mode, in which the
 * driver maps the registers and reports on the state of the controller but
 * never issues a command and creates no children. A controller marked for this
 * driver attaches in owner mode and is fully configured and driven from here.
 *
 * Register Access
 *
 * The registers are mapped through the DDI using the reg property that fch(4D)
 * places on our node, and are accessed with x_ddi_reg_get() and
 * x_ddi_reg_put(). The polled eSPI routines in unix (os/kespi.c) operate only
 * on blocks mapped outside the DDI, as required early in boot, and so are not
 * used here.
 *
 * Commands
 *
 * Every command to the target goes through one set of downstream header
 * registers (FCH::ITF::ESPI::DN_TXHDR_*) and data FIFO, whatever channel it
 * belongs to. The header registers are programmed, the DNCMD_STATUS bit is set
 * to send the command, and the controller clears that bit and raises the DNCMD
 * interrupt status bit when the command has completed. Errors are reported
 * through other bits of the same interrupt status register, which are cleared
 * before each command so that whatever is found afterwards belongs to it. The
 * command path is serialised by en_lock and, for the moment, polled.
 *
 * Link Initialisation
 *
 * For the controller that the system booted through, the PSP and ABL
 * initialise the link before the OS runs. Nothing does that for any other
 * controller, so in owner mode this driver performs the sequence that the
 * PPR's eSPI software programming guide describes for a controller that is not
 * the boot path (PPR Vol 7, 14.1.10.1.1.3 and 14.1.10.1.2). In outline, the
 * controller's own watchdog and wait state settings are programmed, the target
 * is reset in band, its capabilities are read with GET_CONFIGURATION, the link
 * parameters (I/O mode, frequency, CRC) are negotiated by writing the target's
 * general configuration register and mirroring the result into the
 * controller's SLAVE0_CONFIG register, and then the channels we use are enabled
 * one by one, each by enabling it in the target, waiting for it to report
 * ready, and enabling it in the controller. A target that gives no response to
 * GET_CONFIGURATION is taken to be absent, which the specification says is not
 * an error during initialisation.
 */

#include <sys/types.h>
#include <sys/stdbool.h>
#include <sys/conf.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/sunndi.h>
#include <sys/modctl.h>
#include <sys/devops.h>
#include <sys/cmn_err.h>
#include <sys/systm.h>
#include <sys/ksynch.h>
#include <sys/sysmacros.h>
#include <sys/cpuvar.h>
#include <sys/x86_archext.h>
#include <sys/platform_detect.h>
#include <sys/espi.h>
#include <sys/amdzen/mmioreg.h>
#include <sys/amdzen/fch/iomux.h>
#include <sys/io/turin/iomux.h>
#include <sys/io/fch/espi.h>

typedef enum espi_mode {
	ESPI_M_PASSIVE,
	ESPI_M_OWNER
} espi_mode_t;

/*
 * The state of the controller's pins, as last established by espi_iomux().
 */
typedef enum espi_pins {
	ESPI_PINS_UNKNOWN,	/* no pin data for this processor */
	ESPI_PINS_OK,
	ESPI_PINS_WRONG
} espi_pins_t;

/*
 * Flash access channel parameters negotiated with the target.
 */
typedef struct espi_flash {
	bool		ef_enabled;
	uint32_t	ef_payload;	/* bytes, per write or completion */
	uint32_t	ef_maxread;	/* bytes, per read request */
	uint32_t	ef_erase_sizes;	/* bit n set: 2^n KiB erase supported */
} espi_flash_t;

typedef struct espi {
	dev_info_t		*en_dip;
	uint_t			en_unit;
	oxide_espi_role_t	en_role;
	espi_mode_t		en_mode;
	espi_pins_t		en_pins;
	mmio_reg_block_t	en_regs;
	/*
	 * en_lock serialises use of the downstream command path and the
	 * controller configuration that goes with it.
	 */
	kmutex_t		en_lock;
	uint32_t		en_master_cap;
	/*
	 * The remaining members are only valid in owner mode. en_target is set
	 * once a target has answered GET_CONFIGURATION. The register copies
	 * are the target's configuration as last read after initialisation.
	 */
	bool			en_target;
	uint32_t		en_ident;
	uint32_t		en_gen_cap;
	uint32_t		en_chan_cap[4];
	espi_flash_t		en_flash;
} espi_t;

static void *espi_state;

/*
 * Tunables. espi_force_owner and espi_force_passive are debugging aids. A bit
 * set in the first makes the corresponding controller behave as though the
 * board data marked it for this driver. A bit set in the second makes a
 * controller that the board data gives no role attach passively, so that its
 * state can be examined. The remainder bound how long we wait for the
 * controller, and set the fastest link parameters we will negotiate. The PPR
 * notes that 66 MHz operation depends heavily on board loading and recommends
 * 33 MHz.
 */
uint_t espi_force_owner = 0;
uint_t espi_force_passive = 0;
uint_t espi_cmd_spins = 20;
uint_t espi_cmd_retries = 100;
uint_t espi_cmd_delay_us = 1000;
uint_t espi_max_freq = ESPI_REG_GEN_CAP_FREQ_33MHZ;
uint_t espi_max_iomode = ESPI_REG_GEN_CAP_IOMODE_QUAD;

/*
 * The I/O mux pins that each controller uses on the SP5 socket, and the pin
 * function that selects the eSPI signal on each. The pin assignments are those
 * of Turin, which Genoa shares. The alternate clock pins (ESPI_CLK2 and so on)
 * are not listed because our boards do not use them.
 */
typedef struct espi_pin {
	uint32_t	ep_pin;
	uint32_t	ep_func;
} espi_pin_t;

static const espi_pin_t espi_sp5_pins[FCH_MAX_ESPI][7] = {
	{
		{ 117, TURIN_FCH_IOMUX_117_ESPI_CLK0 },
		{ 108, TURIN_FCH_IOMUX_108_ESPI0_ALERT_L },
		{ 124, TURIN_FCH_IOMUX_124_ESPI_CS0_L },
		{ 120, TURIN_FCH_IOMUX_120_ESPI0_DAT0 },
		{ 121, TURIN_FCH_IOMUX_121_ESPI0_DAT1 },
		{ 122, TURIN_FCH_IOMUX_122_ESPI0_DAT2 },
		{ 123, TURIN_FCH_IOMUX_123_ESPI0_DAT3 }
	},
	{
		{ 75, TURIN_FCH_IOMUX_75_ESPI_CLK1 },
		{ 110, TURIN_FCH_IOMUX_110_ESPI1_ALERT_L },
		{ 125, TURIN_FCH_IOMUX_125_ESPI_CS1_L },
		{ 131, TURIN_FCH_IOMUX_131_ESPI1_DAT0 },
		{ 132, TURIN_FCH_IOMUX_132_ESPI1_DAT1 },
		{ 133, TURIN_FCH_IOMUX_133_ESPI1_DAT2 },
		{ 134, TURIN_FCH_IOMUX_134_ESPI1_DAT3 }
	}
};

static uint32_t
espi_read(const mmio_reg_t reg)
{
	return ((uint32_t)x_ddi_reg_get(reg));
}

static void
espi_write(const mmio_reg_t reg, uint32_t val)
{
	x_ddi_reg_put(reg, val);
}

/*
 * Wait between polls of the controller. The first few waits are short spins,
 * as most operations complete quickly, and the rest are sleeps.
 */
static void
espi_pause(uint_t iter)
{
	if (iter < espi_cmd_spins)
		drv_usecwait(10);
	else
		delay(drv_usectohz(espi_cmd_delay_us));
}

/*
 * The size encodings used for payload and read request sizes throughout the
 * target's channel capability registers. 1 is 64 bytes and each step doubles.
 * 0 is only valid for the target's maximum read request size, where it also
 * means 64 bytes.
 */
static uint32_t
espi_size_bytes(uint32_t code)
{
	return (code == 0 ? 64 : 32U << code);
}

/*
 * Determine which controller this node represents from its unit address,
 * which fch(4D) assigns from its own definition of the child.
 */
static bool
espi_unit(dev_info_t *dip, uint_t *unitp)
{
	const char *ua = ddi_get_name_addr(dip);
	unsigned long val;

	if (ua == NULL || ddi_strtoul(ua, NULL, 16, &val) != 0 ||
	    val >= FCH_MAX_ESPI) {
		return (false);
	}

	*unitp = (uint_t)val;
	return (true);
}

static oxide_espi_role_t
espi_role(uint_t unit)
{
	if ((espi_force_owner & (1U << unit)) != 0)
		return (OXIDE_ESPI_ROLE_NEXUS);

	if (unit >= OXIDE_BOARD_ESPI)
		return (OXIDE_ESPI_ROLE_NONE);

	return (oxide_board_data->obd_espi[unit]);
}

/*
 * Pin Muxing
 *
 * Each controller's signals are multiplexed onto FCH GPIO pins, which must be
 * set to their eSPI function before the controller can be used. For the
 * controller that carries IPCC the firmware does this, as the system boots
 * through it. For a controller that this driver owns it does so itself here,
 * since the board's platform data confines itself to what is needed to identify
 * the board and to reach the service processor. In passive mode the pins are
 * only checked. We know the pin assignments for SP5 processors only. The I/O
 * mux registers belong to another part of the FCH, so this uses the kernel's
 * own mapping of them rather than a DDI mapping.
 */
static espi_pins_t
espi_iomux(espi_t *en, bool set)
{
	mmio_reg_block_t block;
	espi_pins_t res = ESPI_PINS_OK;

	switch (chiprev_family(cpuid_getchiprev(CPU))) {
	case X86_PF_AMD_GENOA:
	case X86_PF_AMD_TURIN:
	case X86_PF_AMD_DENSE_TURIN:
		break;
	default:
		dev_err(en->en_dip, CE_WARN, "!no I/O mux pin data for this "
		    "processor family, assuming the pins are muxed for eSPI");
		en->en_pins = ESPI_PINS_UNKNOWN;
		return (en->en_pins);
	}

	block = fch_iomux_mmio_block();

	for (uint_t i = 0; i < ARRAY_SIZE(espi_sp5_pins[en->en_unit]); i++) {
		const espi_pin_t *ep = &espi_sp5_pins[en->en_unit][i];
		const mmio_reg_t reg = FCH_IOMUX_IOMUX_MMIO(block, ep->ep_pin);
		uint32_t val = mmio_reg_read(reg);

		if (FCH_IOMUX_IOMUX_GET_AF(val) == ep->ep_func)
			continue;

		if (set) {
			dev_err(en->en_dip, CE_NOTE, "?AGPIO%u was muxed to "
			    "function %u, selecting eSPI (%u)", ep->ep_pin,
			    FCH_IOMUX_IOMUX_GET_AF(val), ep->ep_func);
			mmio_reg_write(reg,
			    FCH_IOMUX_IOMUX_SET_AF(val, ep->ep_func));
			val = mmio_reg_read(reg);
			if (FCH_IOMUX_IOMUX_GET_AF(val) == ep->ep_func)
				continue;
		}

		dev_err(en->en_dip, CE_WARN, "!AGPIO%u is muxed to function "
		    "%u, not eSPI (%u)", ep->ep_pin,
		    FCH_IOMUX_IOMUX_GET_AF(val), ep->ep_func);
		res = ESPI_PINS_WRONG;
	}

	mmio_reg_block_unmap(&block);
	en->en_pins = res;
	return (res);
}

/*
 * Semaphore
 *
 * FCH::ITF::ESPI::SEMAPHORE_MISC_CONTROL_REG0 arbitrates use of the controller
 * between the PSP, the SMU firmware and software on the host. We take owner 2,
 * one of the two reserved for the host, around each sequence of commands.
 */
static int
espi_sem_acquire(espi_t *en)
{
	const mmio_reg_t reg = FCH_ESPI_SEM_MISC_CTL_REG0_MMIO(en->en_regs);

	ASSERT(MUTEX_HELD(&en->en_lock));

	for (uint_t i = 0; i < espi_cmd_retries; i++) {
		uint32_t val = espi_read(reg);

		if (FCH_ESPI_SEM_MISC_CTL_REG0_GET_SW4_USER_ID(val) == 0 &&
		    FCH_ESPI_SEM_MISC_CTL_REG0_GET_SW0_OWN_STAT(val) == 0 &&
		    FCH_ESPI_SEM_MISC_CTL_REG0_GET_SW1_OWN_STAT(val) == 0 &&
		    FCH_ESPI_SEM_MISC_CTL_REG0_GET_SW2_OWN_STAT(val) == 0 &&
		    FCH_ESPI_SEM_MISC_CTL_REG0_GET_SW3_OWN_STAT(val) == 0) {
			val = FCH_ESPI_SEM_MISC_CTL_REG0_SET_SW2_OWN_SET(val,
			    1);
			val = FCH_ESPI_SEM_MISC_CTL_REG0_SET_SW2_OWN_CLR(val,
			    0);
			espi_write(reg, val);

			val = espi_read(reg);
			if (FCH_ESPI_SEM_MISC_CTL_REG0_GET_SW2_OWN_STAT(val) ==
			    1) {
				return (0);
			}
		}

		espi_pause(i);
	}

	return (EBUSY);
}

static void
espi_sem_release(espi_t *en)
{
	const mmio_reg_t reg = FCH_ESPI_SEM_MISC_CTL_REG0_MMIO(en->en_regs);
	uint32_t val = espi_read(reg);

	ASSERT(MUTEX_HELD(&en->en_lock));
	VERIFY3U(FCH_ESPI_SEM_MISC_CTL_REG0_GET_SW2_OWN_STAT(val), ==, 1);

	val = FCH_ESPI_SEM_MISC_CTL_REG0_SET_SW2_OWN_CLR(val, 1);
	espi_write(reg, val);

	for (uint_t i = 0; i < espi_cmd_retries; i++) {
		val = espi_read(reg);
		if (FCH_ESPI_SEM_MISC_CTL_REG0_GET_SW2_OWN_STAT(val) == 0)
			break;
		espi_pause(i);
	}

	if (FCH_ESPI_SEM_MISC_CTL_REG0_GET_SW2_OWN_STAT(val) != 0) {
		dev_err(en->en_dip, CE_WARN, "semaphore did not release");
		return;
	}

	val = FCH_ESPI_SEM_MISC_CTL_REG0_SET_SW2_OWN_CLR(val, 0);
	val = FCH_ESPI_SEM_MISC_CTL_REG0_SET_SW2_OWN_SET(val, 0);
	espi_write(reg, val);
}

/*
 * Command Path
 */

/*
 * Convert the interrupt status register into an error for the command that was
 * just issued. The entries are in descending order of significance and the
 * first one that applies wins. A target that does not respond at all is
 * reported as ENODEV, which the initialisation code uses to tell an absent
 * target from a broken one. Bits that indicate completions rather than errors
 * are not included.
 */
static int
espi_intsts_error(uint32_t r)
{
	const struct {
		bool set;
		int errnum;
	} map[] = {
		{ FCH_ESPI_S0_INT_STS_GET_WDG_TO(r),		ETIMEDOUT },
		{ FCH_ESPI_S0_INT_STS_GET_MST_ABORT(r),		ECONNABORTED },
		{ FCH_ESPI_S0_INT_STS_GET_UPFIFO_WDG_TO(r),	ETIMEDOUT },
		{ FCH_ESPI_S0_INT_STS_GET_PROTOERR(r),		EPROTO },
		{ FCH_ESPI_S0_INT_STS_GET_ILL_LEN(r),		EPROTO },
		{ FCH_ESPI_S0_INT_STS_GET_ILL_TAG(r),		EPROTO },
		{ FCH_ESPI_S0_INT_STS_GET_USF_CPL(r),		EPROTO },
		{ FCH_ESPI_S0_INT_STS_GET_UNK_CYC(r),		EPROTO },
		{ FCH_ESPI_S0_INT_STS_GET_UNK_RSP(r),		EPROTO },
		{ FCH_ESPI_S0_INT_STS_GET_CRC_ERR(r),		EPROTO },
		{ FCH_ESPI_S0_INT_STS_GET_WAIT_TMT(r),		EPROTO },
		{ FCH_ESPI_S0_INT_STS_GET_BUS_ERR(r),		EPROTO },
		{ FCH_ESPI_S0_INT_STS_GET_RXFLASH_OFLOW(r),	EOVERFLOW },
		{ FCH_ESPI_S0_INT_STS_GET_RXMSG_OFLOW(r),	EOVERFLOW },
		{ FCH_ESPI_S0_INT_STS_GET_RXOOB_OFLOW(r),	EOVERFLOW },
		{ FCH_ESPI_S0_INT_STS_GET_NO_RSP(r),		ENODEV },
		{ FCH_ESPI_S0_INT_STS_GET_FATAL_ERR(r),		EPROTO },
	};

	for (uint_t i = 0; i < ARRAY_SIZE(map); i++) {
		if (map[i].set)
			return (map[i].errnum);
	}

	return (0);
}

/*
 * Clear the error and command completion bits of the interrupt status register
 * so that what we find after the next command belongs to it. The bits that
 * announce received data (RXOOB, RXMSG, RXVW, FLASHREQ) are left alone, as
 * they belong to whichever channel they are for.
 */
static void
espi_intsts_clear(espi_t *en)
{
	uint32_t val = 0;

	val = FCH_ESPI_S0_INT_STS_CLEAR_DNCMD(val);
	val = FCH_ESPI_S0_INT_STS_CLEAR_WDG_TO(val);
	val = FCH_ESPI_S0_INT_STS_CLEAR_MST_ABORT(val);
	val = FCH_ESPI_S0_INT_STS_CLEAR_UPFIFO_WDG_TO(val);
	val = FCH_ESPI_S0_INT_STS_CLEAR_PROTOERR(val);
	val = FCH_ESPI_S0_INT_STS_CLEAR_RXFLASH_OFLOW(val);
	val = FCH_ESPI_S0_INT_STS_CLEAR_RXMSG_OFLOW(val);
	val = FCH_ESPI_S0_INT_STS_CLEAR_RXOOB_OFLOW(val);
	val = FCH_ESPI_S0_INT_STS_CLEAR_ILL_LEN(val);
	val = FCH_ESPI_S0_INT_STS_CLEAR_ILL_TAG(val);
	val = FCH_ESPI_S0_INT_STS_CLEAR_USF_CPL(val);
	val = FCH_ESPI_S0_INT_STS_CLEAR_UNK_CYC(val);
	val = FCH_ESPI_S0_INT_STS_CLEAR_UNK_RSP(val);
	val = FCH_ESPI_S0_INT_STS_CLEAR_NFATAL_ERR(val);
	val = FCH_ESPI_S0_INT_STS_CLEAR_FATAL_ERR(val);
	val = FCH_ESPI_S0_INT_STS_CLEAR_NO_RSP(val);
	val = FCH_ESPI_S0_INT_STS_CLEAR_CRC_ERR(val);
	val = FCH_ESPI_S0_INT_STS_CLEAR_WAIT_TMT(val);
	val = FCH_ESPI_S0_INT_STS_CLEAR_BUS_ERR(val);

	espi_write(FCH_ESPI_S0_INT_STS_MMIO(en->en_regs), val);
}

/*
 * Wait for the downstream command path to be free, as indicated by
 * DN_TXHDR_0th[DNCMD_STATUS] being clear.
 */
static int
espi_cmd_wait_idle(espi_t *en)
{
	const mmio_reg_t hdr0 = FCH_ESPI_DN_TXHDR0_MMIO(en->en_regs);

	ASSERT(MUTEX_HELD(&en->en_lock));

	for (uint_t i = 0; i < espi_cmd_retries; i++) {
		const uint32_t val = espi_read(hdr0);

		if (FCH_ESPI_DN_TXHDR0_GET_DNCMD_STATUS(val) == 0)
			return (0);
		espi_pause(i);
	}

	return (ETIMEDOUT);
}

/*
 * Send a command whose header registers and data have been programmed, and
 * wait for it to complete. hdr0 is the value already written to DN_TXHDR_0th,
 * which is written again with DNCMD_STATUS set to start the command. An
 * in-band reset has no response phase, so for it the no-response indication
 * is not treated as an error.
 */
static int
espi_cmd_submit(espi_t *en, uint32_t hdr0, bool response)
{
	const mmio_reg_t hdr0r = FCH_ESPI_DN_TXHDR0_MMIO(en->en_regs);
	const mmio_reg_t ists = FCH_ESPI_S0_INT_STS_MMIO(en->en_regs);
	uint32_t val = 0;
	uint_t i;

	ASSERT(MUTEX_HELD(&en->en_lock));

	espi_write(hdr0r, FCH_ESPI_DN_TXHDR0_SET_DNCMD_STATUS(hdr0, 1));

	for (i = 0; i < espi_cmd_retries; i++) {
		int err;

		val = espi_read(ists);
		if (FCH_ESPI_S0_INT_STS_GET_DNCMD(val) == 1)
			break;

		err = espi_intsts_error(val);
		if (err == ENODEV && !response)
			err = 0;
		if (err != 0)
			return (err);

		espi_pause(i);
	}

	if (FCH_ESPI_S0_INT_STS_GET_DNCMD(val) != 1)
		return (ETIMEDOUT);

	/*
	 * An error may have been recorded in the same pass as the completion.
	 */
	if (response) {
		int err = espi_intsts_error(val);

		if (err != 0)
			return (err);
	}

	for (i = 0; i < espi_cmd_retries; i++) {
		val = espi_read(hdr0r);
		if (FCH_ESPI_DN_TXHDR0_GET_DNCMD_STATUS(val) == 0)
			return (0);
		espi_pause(i);
	}

	return (ETIMEDOUT);
}

/*
 * Build DN_TXHDR_0th for a GET_CONFIGURATION or SET_CONFIGURATION of the given
 * target register. The register address is split across HDATA0 and HDATA1 and
 * HDATA2 is reserved.
 */
static uint32_t
espi_cfg_hdr0(uint32_t type, uint16_t reg)
{
	uint32_t val = 0;

	val = FCH_ESPI_DN_TXHDR0_SET_DNCMD_TYPE(val, type);
	val = FCH_ESPI_DN_TXHDR0_SET_HDATA0(val, bitx16(reg, 11, 8));
	val = FCH_ESPI_DN_TXHDR0_SET_HDATA1(val, bitx16(reg, 7, 0));
	val = FCH_ESPI_DN_TXHDR0_SET_HDATA2(val, 0);

	return (val);
}

static bool
espi_cfg_reg_valid(uint16_t reg)
{
	/*
	 * Configuration registers are 32 bits wide and the address space is 12
	 * bits.
	 */
	return ((reg & 0x3) == 0 && (reg >> 12) == 0);
}

static int
espi_cfg_get(espi_t *en, uint16_t reg, uint32_t *valp)
{
	const mmio_reg_t hdr0 = FCH_ESPI_DN_TXHDR0_MMIO(en->en_regs);
	const mmio_reg_t hdr1 = FCH_ESPI_DN_TXHDR1_MMIO(en->en_regs);
	uint32_t val0;
	int ret;

	if (!espi_cfg_reg_valid(reg))
		return (EINVAL);

	if ((ret = espi_cmd_wait_idle(en)) != 0)
		return (ret);

	espi_intsts_clear(en);
	val0 = espi_cfg_hdr0(FCH_ESPI_DN_TXHDR0_TYPE_GETCONF, reg);
	espi_write(hdr0, val0);
	/*
	 * The PPR recommends clearing the register that will receive the value
	 * before each GET_CONFIGURATION.
	 */
	espi_write(hdr1, 0);

	if ((ret = espi_cmd_submit(en, val0, true)) != 0)
		return (ret);

	*valp = espi_read(hdr1);
	return (0);
}

static int
espi_cfg_set(espi_t *en, uint16_t reg, uint32_t val)
{
	const mmio_reg_t hdr0 = FCH_ESPI_DN_TXHDR0_MMIO(en->en_regs);
	const mmio_reg_t hdr1 = FCH_ESPI_DN_TXHDR1_MMIO(en->en_regs);
	uint32_t val0;
	int ret;

	if (!espi_cfg_reg_valid(reg))
		return (EINVAL);

	if ((ret = espi_cmd_wait_idle(en)) != 0)
		return (ret);

	espi_intsts_clear(en);
	val0 = espi_cfg_hdr0(FCH_ESPI_DN_TXHDR0_TYPE_SETCONF, reg);
	espi_write(hdr0, val0);
	espi_write(hdr1, val);

	return (espi_cmd_submit(en, val0, true));
}

/*
 * Read a target configuration register until the predicate is satisfied by its
 * value. This is how a channel's ready bit is awaited after enabling it.
 */
typedef bool (*espi_cfg_pred_f)(uint32_t);

static int
espi_cfg_wait(espi_t *en, uint16_t reg, espi_cfg_pred_f pred,
    uint32_t *valp)
{
	for (uint_t i = 0; i < espi_cmd_retries; i++) {
		int ret;

		if ((ret = espi_cfg_get(en, reg, valp)) != 0)
			return (ret);
		if (pred(*valp))
			return (0);
		espi_pause(i);
	}

	return (ETIMEDOUT);
}

static bool
espi_chan1_ready(uint32_t val)
{
	return (ESPI_REG_CHAN1_CAP_GET_EN(val) == 1 &&
	    ESPI_REG_CHAN1_CAP_GET_READY(val) == 1);
}

static bool
espi_chan3_ready(uint32_t val)
{
	return (ESPI_REG_CHAN3_CAP_GET_EN(val) == 1 &&
	    ESPI_REG_CHAN3_CAP_GET_READY(val) == 1);
}

/*
 * Send a single virtual wire group to the target. The data FIFO receives the
 * index byte followed by the data byte, and HDATA0 carries the number of
 * groups less one.
 */
static int
espi_vw_put(espi_t *en, uint8_t index, uint8_t data)
{
	const mmio_reg_t hdr0 = FCH_ESPI_DN_TXHDR0_MMIO(en->en_regs);
	const mmio_reg_t hdr1 = FCH_ESPI_DN_TXHDR1_MMIO(en->en_regs);
	const mmio_reg_t hdr2 = FCH_ESPI_DN_TXHDR2_MMIO(en->en_regs);
	const mmio_reg_t txd = FCH_ESPI_DN_TXDATA_PORT_MMIO(en->en_regs);
	uint32_t val0;
	int ret;

	if ((ret = espi_cmd_wait_idle(en)) != 0)
		return (ret);

	espi_intsts_clear(en);
	val0 = FCH_ESPI_DN_TXHDR0_SET_DNCMD_TYPE(0, FCH_ESPI_DN_TXHDR0_TYPE_VW);
	val0 = FCH_ESPI_DN_TXHDR0_SET_HDATA0(val0, 0);
	espi_write(hdr0, val0);
	espi_write(hdr1, 0);
	espi_write(hdr2, 0);
	espi_write(txd, (uint32_t)index | ((uint32_t)data << 8));

	return (espi_cmd_submit(en, val0, true));
}

/*
 * Reset the target in band (PPR 14.1.10.1.2.3). The reset is sent at the
 * lowest clock rate and afterwards both ends are back at the link defaults of
 * single I/O at 20 MHz without CRC, so the controller's configuration is set
 * to match. The target's general capabilities register is the only one that
 * the reset affects.
 */
static int
espi_inband_reset(espi_t *en)
{
	const mmio_reg_t cfgr = FCH_ESPI_S0_CONFIG_MMIO(en->en_regs);
	const mmio_reg_t hdr0 = FCH_ESPI_DN_TXHDR0_MMIO(en->en_regs);
	const mmio_reg_t hdr1 = FCH_ESPI_DN_TXHDR1_MMIO(en->en_regs);
	const mmio_reg_t hdr2 = FCH_ESPI_DN_TXHDR2_MMIO(en->en_regs);
	uint32_t cfg, val0;
	int ret;

	if ((ret = espi_cmd_wait_idle(en)) != 0)
		return (ret);

	cfg = espi_read(cfgr);
	cfg = FCH_ESPI_S0_CONFIG_SET_CLK_FREQ_SEL(cfg,
	    FCH_ESPI_S0_CONFIG_CLK_FREQ_16MHZ);
	espi_write(cfgr, cfg);

	espi_intsts_clear(en);
	val0 = FCH_ESPI_DN_TXHDR0_SET_DNCMD_TYPE(0,
	    FCH_ESPI_DN_TXHDR0_TYPE_RESET);
	espi_write(hdr0, val0);
	espi_write(hdr1, 0);
	espi_write(hdr2, 0);

	ret = espi_cmd_submit(en, val0, false);

	cfg = espi_read(cfgr);
	cfg = FCH_ESPI_S0_CONFIG_SET_CLK_FREQ_SEL(cfg,
	    FCH_ESPI_S0_CONFIG_CLK_FREQ_20MHZ);
	cfg = FCH_ESPI_S0_CONFIG_SET_IO_MODE_SEL(cfg,
	    FCH_ESPI_S0_CONFIG_IO_MODE_SINGLE);
	cfg = FCH_ESPI_S0_CONFIG_SET_CRC_CHECK_EN(cfg, 0);
	espi_write(cfgr, cfg);

	return (ret);
}

/*
 * Link Initialisation
 */

/*
 * Clear the system event virtual wire state held in
 * FCH::ITF::ESPI::SLAVE0_RXVW. The PPR's initialisation sequence reads the
 * register and then writes FFFF6F00h to it, which sets every write-1-to-clear
 * event bit other than the two boot load bits, writes a 1 to each of the level
 * wires to return it to its inactive state, and also sets the reserved bits
 * 29:20 and 9. The reserved bits are included here so that the write matches
 * the PPR exactly.
 */
static void
espi_rxvw_clear(espi_t *en)
{
	const mmio_reg_t reg = FCH_ESPI_S0_RXVW_MMIO(en->en_regs);
	uint32_t val = 0;

	ASSERT(MUTEX_HELD(&en->en_lock));

	(void) espi_read(reg);

	val = FCH_ESPI_S0_RXVW_CLEAR_CPUTEMP_REQ(val);
	val = FCH_ESPI_S0_RXVW_CLEAR_RTCTIME_REQ(val);
	val = FCH_ESPI_S0_RXVW_CLEAR_HOST_RST_ACK(val);
	val = FCH_ESPI_S0_RXVW_SET_RCIN_B(val, 1);
	val = FCH_ESPI_S0_RXVW_SET_SMI_B(val, 1);
	val = FCH_ESPI_S0_RXVW_SET_SCI_B(val, 1);
	val = FCH_ESPI_S0_RXVW_CLEAR_ERROR_NONFATAL(val);
	val = FCH_ESPI_S0_RXVW_CLEAR_ERROR_FATAL(val);
	val = FCH_ESPI_S0_RXVW_SET_PME_B(val, 1);
	val = FCH_ESPI_S0_RXVW_SET_WAKE_B(val, 1);
	val = FCH_ESPI_S0_RXVW_CLEAR_OOB_RST_ACK(val);
	val = bitset32(val, 29, 20, 0x3ff);
	val = bitset32(val, 9, 9, 1);

	espi_write(reg, val);
}

/*
 * Program the controller's own settings, which do not involve the target
 * (PPR 14.1.10.1.1.3.1 and parts of 14.1.10.1.2.1).
 */
static void
espi_ctrl_init(espi_t *en)
{
	mmio_reg_t reg;
	uint32_t val;

	ASSERT(MUTEX_HELD(&en->en_lock));

	/*
	 * Clear any virtual wire state left from before we were here.
	 */
	espi_rxvw_clear(en);

	/*
	 * Enable the watchdog on downstream transactions and the wait state
	 * limit. The watchdog count is set to its maximum, as the PPR
	 * recommends when flash erases will be issued.
	 */
	reg = FCH_ESPI_GLOBAL_CTL0_MMIO(en->en_regs);
	val = espi_read(reg);
	val = FCH_ESPI_GLOBAL_CTL0_SET_WDG_EN(val, 1);
	val = FCH_ESPI_GLOBAL_CTL0_SET_WDG_CNT(val, 0xffff);
	val = FCH_ESPI_GLOBAL_CTL0_SET_WAIT_CHKEN(val, 1);
	val = FCH_ESPI_GLOBAL_CTL0_SET_WAIT_CNT(val, 0x3f);
	espi_write(reg, val);

	/*
	 * Errors are routed to SMI by default. We poll for them and do not
	 * want SMIs, so route them to the same IRQ line as command completion.
	 * The eSPI IRQ lines are masked in FCH::PM::ESPIINTRCTRL until we
	 * come to use interrupts.
	 */
	reg = FCH_ESPI_GLOBAL_CTL1_MMIO(en->en_regs);
	val = espi_read(reg);
	val = FCH_ESPI_GLOBAL_CTL1_SET_ERR_INT_MAP(val,
	    FCH_ESPI_GLOBAL_CTL1_INT_MAP_IRQ(23));
	val = FCH_ESPI_GLOBAL_CTL1_SET_RGCMD_INT_MAP(val,
	    FCH_ESPI_GLOBAL_CTL1_INT_MAP_IRQ(23));
	val = FCH_ESPI_GLOBAL_CTL1_SET_ALERT_ENABLE(val, 0);
	val = FCH_ESPI_GLOBAL_CTL1_SET_BUS_MASTER_EN(val, 0);
	espi_write(reg, val);

	/*
	 * Capture the four groups of system event virtual wires that the target
	 * sends so that their state can be read from SLAVE0_RXVW_DATA.
	 */
	reg = FCH_ESPI_S0_RXVW_INDEX_MMIO(en->en_regs);
	val = 0;
	val = FCH_ESPI_S0_RXVW_INDEX_SET_GRP(val, 0, ESPI_VW_IDX_SYSEV_WAKE);
	val = FCH_ESPI_S0_RXVW_INDEX_SET_GRP(val, 1, ESPI_VW_IDX_SYSEV_BOOT);
	val = FCH_ESPI_S0_RXVW_INDEX_SET_GRP(val, 2, ESPI_VW_IDX_SYSEV_SCI);
	val = FCH_ESPI_S0_RXVW_INDEX_SET_GRP(val, 3,
	    ESPI_VW_IDX_SYSEV_HOST_RST);
	espi_write(reg, val);

	reg = FCH_ESPI_S0_RXVW_MISC_CNTL_MMIO(en->en_regs);
	val = espi_read(reg);
	for (uint_t i = 0; i < 4; i++)
		val = FCH_ESPI_S0_RXVW_MISC_CNTL_SET_GRP_EN(val, i, 1);
	espi_write(reg, val);

	/*
	 * Our targets signal alerts in band on I/O[1], with no channel enabled
	 * until the link has been configured. The peripheral channel is not
	 * used from the host at all.
	 */
	reg = FCH_ESPI_S0_CONFIG_MMIO(en->en_regs);
	val = espi_read(reg);
	val = FCH_ESPI_S0_CONFIG_SET_ALERT_MODE_SEL(val,
	    FCH_ESPI_S0_CONFIG_ALERT_MODE_IO1);
	val = FCH_ESPI_S0_CONFIG_SET_PR_EN(val, 0);
	val = FCH_ESPI_S0_CONFIG_SET_VW_EN(val, 0);
	val = FCH_ESPI_S0_CONFIG_SET_OOB_EN(val, 0);
	val = FCH_ESPI_S0_CONFIG_SET_FLASH_EN(val, 0);
	espi_write(reg, val);
}

/*
 * Find out whether there is a target and that it implements a version of the
 * specification we understand.
 */
static int
espi_target_identify(espi_t *en)
{
	int ret;

	ret = espi_cfg_get(en, ESPI_REG_IDENT, &en->en_ident);
	if (ret != 0)
		return (ret);

	if (ESPI_REG_IDENT_GET_VERSION(en->en_ident) !=
	    ESPI_REG_IDENT_VERSION_1X) {
		dev_err(en->en_dip, CE_WARN, "target implements eSPI version "
		    "%u which is not supported",
		    ESPI_REG_IDENT_GET_VERSION(en->en_ident));
		return (ENOTSUP);
	}

	en->en_target = true;
	return (0);
}

/*
 * Negotiate the I/O mode, clock frequency and CRC use. The target's general
 * capabilities register is written first and the controller's SLAVE0_CONFIG
 * register is then set to match, as the new target settings only take effect
 * once its response to the SET_CONFIGURATION has been sent. The encodings of
 * the I/O mode and frequency fields are the same on both sides. The
 * controller's I/O mode support field is taken to use the target's encoding
 * too, which is consistent with the value documented for Turin.
 */
static int
espi_link_configure(espi_t *en)
{
	const mmio_reg_t cfgr = FCH_ESPI_S0_CONFIG_MMIO(en->en_regs);
	uint32_t cap, tsup, csup, iomode, freq, crc, cfg;
	int ret;

	if ((ret = espi_cfg_get(en, ESPI_REG_GEN_CAP, &cap)) != 0)
		return (ret);

	tsup = ESPI_REG_GEN_CAP_GET_IOMODE_SUP(cap);
	csup = FCH_ESPI_MASTER_CAP_GET_IOMODE(en->en_master_cap);
	iomode = ESPI_REG_GEN_CAP_IOMODE_SINGLE;
	if (espi_max_iomode >= ESPI_REG_GEN_CAP_IOMODE_QUAD &&
	    (tsup & 0x2) != 0 && (csup & 0x2) != 0) {
		iomode = ESPI_REG_GEN_CAP_IOMODE_QUAD;
	} else if (espi_max_iomode >= ESPI_REG_GEN_CAP_IOMODE_DUAL &&
	    (tsup & 0x1) != 0 && (csup & 0x1) != 0) {
		iomode = ESPI_REG_GEN_CAP_IOMODE_DUAL;
	}

	freq = MIN(ESPI_REG_GEN_CAP_GET_FREQ_SUP(cap),
	    FCH_ESPI_MASTER_CAP_GET_CLKFREQ(en->en_master_cap));
	freq = MIN(freq, espi_max_freq);

	crc = FCH_ESPI_MASTER_CAP_GET_CRC(en->en_master_cap);

	cap = ESPI_REG_GEN_CAP_SET_IOMODE(cap, iomode);
	cap = ESPI_REG_GEN_CAP_SET_FREQ(cap, freq);
	cap = ESPI_REG_GEN_CAP_SET_CRC_EN(cap, crc);
	cap = ESPI_REG_GEN_CAP_SET_ALERTMODE(cap,
	    ESPI_REG_GEN_CAP_ALERTMODE_IO);

	if ((ret = espi_cfg_set(en, ESPI_REG_GEN_CAP, cap)) != 0)
		return (ret);

	cfg = espi_read(cfgr);
	cfg = FCH_ESPI_S0_CONFIG_SET_IO_MODE_SEL(cfg, iomode);
	cfg = FCH_ESPI_S0_CONFIG_SET_CLK_FREQ_SEL(cfg, freq);
	cfg = FCH_ESPI_S0_CONFIG_SET_CRC_CHECK_EN(cfg, crc);
	espi_write(cfgr, cfg);

	/*
	 * Read the register back over the newly configured link. That both
	 * proves the link works and shows that the target took the settings.
	 */
	if ((ret = espi_cfg_get(en, ESPI_REG_GEN_CAP, &en->en_gen_cap)) != 0)
		return (ret);

	if (ESPI_REG_GEN_CAP_GET_IOMODE(en->en_gen_cap) != iomode ||
	    ESPI_REG_GEN_CAP_GET_FREQ(en->en_gen_cap) != freq ||
	    ESPI_REG_GEN_CAP_GET_CRC_EN(en->en_gen_cap) != crc) {
		dev_err(en->en_dip, CE_WARN, "target did not accept link "
		    "configuration, general capabilities are 0x%x",
		    en->en_gen_cap);
		return (EIO);
	}

	return (0);
}

/*
 * Enable the virtual wire channel and tell the target that the platform is
 * out of reset. The PPR's sequence for the boot controller also sends
 * SUS_STAT# deasserted here, which does no harm for a target that ignores it.
 */
static int
espi_vw_enable(espi_t *en)
{
	mmio_reg_t reg;
	uint32_t val, data;
	int ret;

	if (ESPI_REG_GEN_CAP_GET_VWIRE(en->en_gen_cap) == 0)
		return (ENOTSUP);

	if ((ret = espi_cfg_get(en, ESPI_REG_CHAN1_CAP, &val)) != 0)
		return (ret);
	val = ESPI_REG_CHAN1_CAP_SET_EN(val, 1);
	if ((ret = espi_cfg_set(en, ESPI_REG_CHAN1_CAP, val)) != 0)
		return (ret);
	if ((ret = espi_cfg_wait(en, ESPI_REG_CHAN1_CAP, espi_chan1_ready,
	    &en->en_chan_cap[1])) != 0) {
		return (ret);
	}

	reg = FCH_ESPI_S0_CONFIG_MMIO(en->en_regs);
	val = espi_read(reg);
	val = FCH_ESPI_S0_CONFIG_SET_VW_EN(val, 1);
	espi_write(reg, val);

	reg = FCH_ESPI_GLOBAL_CTL1_MMIO(en->en_regs);
	val = espi_read(reg);
	val = FCH_ESPI_GLOBAL_CTL1_SET_ALERT_ENABLE(val, 1);
	espi_write(reg, val);

	data = 0;
	data = ESPI_VW_DATA_SET_VALID(data,
	    (1 << ESPI_VW_SYSEV_SUS_STAT) | (1 << ESPI_VW_SYSEV_PLTRST));
	data = ESPI_VW_DATA_SET_LEVEL(data,
	    (1 << ESPI_VW_SYSEV_SUS_STAT) | (1 << ESPI_VW_SYSEV_PLTRST));
	if ((ret = espi_vw_put(en, ESPI_VW_IDX_SYSEV_PLTRST,
	    (uint8_t)data)) != 0) {
		return (ret);
	}

	/*
	 * Enabling the channel may have prompted the target to send its boot
	 * load status. Clear everything else that it may have raised.
	 */
	espi_rxvw_clear(en);

	return (0);
}

/*
 * Enable the flash access channel with the target's flash attached to the
 * target (SAFS), which is the only sharing scheme we support. Read requests
 * are limited to the payload size so that each is answered by a single
 * completion.
 */
static int
espi_flash_enable(espi_t *en)
{
	espi_flash_t *ef = &en->en_flash;
	mmio_reg_t reg;
	uint32_t cap, cap2, sup, payload, maxread, val;
	int ret;

	if (ESPI_REG_GEN_CAP_GET_FLASH(en->en_gen_cap) == 0)
		return (ENOTSUP);

	if ((ret = espi_cfg_get(en, ESPI_REG_CHAN3_CAP, &cap)) != 0)
		return (ret);
	if ((ret = espi_cfg_get(en, ESPI_REG_CHAN3_CAP2, &cap2)) != 0)
		return (ret);

	sup = ESPI_REG_CHAN3_CAP_GET_SHARE_SUP(cap);
	if (sup != ESPI_REG_CHAN3_CAP_SHARE_SUP_TAFS &&
	    sup != ESPI_REG_CHAN3_CAP_SHARE_SUP_BOTH) {
		dev_err(en->en_dip, CE_WARN, "!target does not support target "
		    "attached flash sharing (0x%x)", sup);
		return (ENOTSUP);
	}

	payload = MIN(ESPI_REG_CHAN3_CAP_GET_MAXPAYLOAD(cap),
	    FCH_ESPI_MASTER_CAP_GET_FLASH_MAXSZ(en->en_master_cap));
	maxread = ESPI_REG_CHAN3_CAP_GET2_MAXREAD(cap2);
	if (maxread == 0)
		maxread = 1;
	maxread = MIN(maxread, payload);

	cap = ESPI_REG_CHAN3_CAP_SET_SHARE(cap, 1);
	cap = ESPI_REG_CHAN3_CAP_SET_SELPAYLOAD(cap, payload);
	cap = ESPI_REG_CHAN3_CAP_SET_MAXREAD(cap, maxread);
	cap = ESPI_REG_CHAN3_CAP_SET_EN(cap, 1);
	if ((ret = espi_cfg_set(en, ESPI_REG_CHAN3_CAP, cap)) != 0)
		return (ret);
	if ((ret = espi_cfg_wait(en, ESPI_REG_CHAN3_CAP, espi_chan3_ready,
	    &en->en_chan_cap[3])) != 0) {
		return (ret);
	}

	reg = FCH_ESPI_S0_CONFIG_MMIO(en->en_regs);
	val = espi_read(reg);
	val = FCH_ESPI_S0_CONFIG_SET_FLASH_SHARING_MODE(val,
	    FCH_ESPI_S0_CONFIG_FLASH_SHARING_SAFS);
	val = FCH_ESPI_S0_CONFIG_SET_FLASH_MPS(val, payload);
	val = FCH_ESPI_S0_CONFIG_SET_FLASH_EN(val, 1);
	espi_write(reg, val);

	ef->ef_enabled = true;
	ef->ef_payload = espi_size_bytes(payload);
	ef->ef_maxread = espi_size_bytes(maxread);
	ef->ef_erase_sizes = ESPI_REG_CHAN3_CAP_GET2_ERASE_SZ(cap2);

	return (0);
}

/*
 * Bring up the link to the target in owner mode. Failure to find a target is
 * not a failure to attach. Failure beyond that point leaves the controller
 * attached with whatever was achieved, so that its state can be inspected.
 */
static void
espi_owner_init(espi_t *en)
{
	int ret;

	mutex_enter(&en->en_lock);

	espi_ctrl_init(en);

	if ((ret = espi_sem_acquire(en)) != 0) {
		dev_err(en->en_dip, CE_WARN, "could not acquire the eSPI "
		    "semaphore: %d", ret);
		mutex_exit(&en->en_lock);
		return;
	}

	if ((ret = espi_inband_reset(en)) != 0) {
		dev_err(en->en_dip, CE_WARN, "in-band reset failed: %d", ret);
		goto out;
	}

	ret = espi_target_identify(en);
	if (ret == ENODEV) {
		dev_err(en->en_dip, CE_NOTE, "?no eSPI target present");
		goto out;
	} else if (ret != 0) {
		dev_err(en->en_dip, CE_WARN, "failed to identify eSPI target: "
		    "%d", ret);
		goto out;
	}

	if ((ret = espi_link_configure(en)) != 0) {
		dev_err(en->en_dip, CE_WARN, "failed to configure link: %d",
		    ret);
		goto out;
	}

	if ((ret = espi_vw_enable(en)) != 0) {
		dev_err(en->en_dip, CE_WARN, "failed to enable the virtual "
		    "wire channel: %d", ret);
		goto out;
	}

	ret = espi_flash_enable(en);
	if (ret != 0 && ret != ENOTSUP) {
		dev_err(en->en_dip, CE_WARN, "failed to enable the flash "
		    "channel: %d", ret);
	}

out:
	espi_sem_release(en);
	mutex_exit(&en->en_lock);
}

/*
 * Decode the controller version and the link parameters for the attach report.
 * The I/O mode and clock frequency encodings are shared by the target's general
 * capabilities register and the controller's SLAVE0_CONFIG register.
 */
static const char *
espi_ctrl_ver_str(uint32_t cap)
{
	switch (FCH_ESPI_MASTER_CAP_GET_VER(cap)) {
	case FCH_ESPI_MASTER_CAP_VER_0_7:
		return ("0.7");
	case FCH_ESPI_MASTER_CAP_VER_0_75:
		return ("0.75");
	case FCH_ESPI_MASTER_CAP_VER_1_0:
		return ("1.0");
	default:
		return ("unknown");
	}
}

static const char *
espi_iomode_str(uint32_t mode)
{
	switch (mode) {
	case ESPI_REG_GEN_CAP_IOMODE_SINGLE:
		return ("single");
	case ESPI_REG_GEN_CAP_IOMODE_DUAL:
		return ("dual");
	case ESPI_REG_GEN_CAP_IOMODE_QUAD:
		return ("quad");
	default:
		return ("unknown");
	}
}

static const char *
espi_freq_str(uint32_t freq)
{
	switch (freq) {
	case FCH_ESPI_S0_CONFIG_CLK_FREQ_16MHZ:
		return ("16 MHz");
	case FCH_ESPI_S0_CONFIG_CLK_FREQ_20MHZ:
		return ("20 MHz");
	case FCH_ESPI_S0_CONFIG_CLK_FREQ_25MHZ:
		return ("25 MHz");
	case FCH_ESPI_S0_CONFIG_CLK_FREQ_33MHZ:
		return ("33 MHz");
	case FCH_ESPI_S0_CONFIG_CLK_FREQ_50MHZ:
		return ("50 MHz");
	case FCH_ESPI_S0_CONFIG_CLK_FREQ_66MHZ:
		return ("66 MHz");
	default:
		return ("unknown");
	}
}

/*
 * The hardware initialisation status (FCH::ITF::ESPI::ESPI_RESERVED_REG0
 * [SAFS_HW_INI_STS]) records how far the controller's own initialisation
 * sequence got for a controller that the system booted through.
 */
static const char *
espi_init_stat_str(uint32_t rst)
{
	switch (FCH_ESPI_RESERVED_REG0_INIT_STAT(rst)) {
	case FCH_ESPI_RESERVED_REG0_INIT_STAT_IDLE:
		return ("idle");
	case FCH_ESPI_RESERVED_REG0_INIT_STAT_SETCONF_VW:
		return ("enabling VW channel");
	case FCH_ESPI_RESERVED_REG0_INIT_STAT_GETCONF_VW:
		return ("waiting for VW channel");
	case FCH_ESPI_RESERVED_REG0_INIT_STAT_ACPI_RSTB:
		return ("waiting for PLTRST# deassertion");
	case FCH_ESPI_RESERVED_REG0_INIT_STAT_SETCONF_SAFS:
		return ("enabling flash channel");
	case FCH_ESPI_RESERVED_REG0_INIT_STAT_GETCONF_FLASH:
		return ("waiting for flash channel");
	case FCH_ESPI_RESERVED_REG0_INIT_STAT_GETCONF_PERIPH:
		return ("waiting for peripheral channel");
	case FCH_ESPI_RESERVED_REG0_INIT_STAT_SUCCESS:
		return ("complete");
	case FCH_ESPI_RESERVED_REG0_INIT_STAT_RESETTING:
		return ("in-band reset");
	default:
		return ("unknown");
	}
}

static const char *
espi_role_str(oxide_espi_role_t role)
{
	switch (role) {
	case OXIDE_ESPI_ROLE_IPCC:
		return ("ipcc");
	case OXIDE_ESPI_ROLE_NEXUS:
		return ("nexus");
	default:
		return ("none");
	}
}

static int
espi_freq_hz(uint32_t freq)
{
	switch (freq) {
	case FCH_ESPI_S0_CONFIG_CLK_FREQ_16MHZ:
		return (16000000);
	case FCH_ESPI_S0_CONFIG_CLK_FREQ_20MHZ:
		return (20000000);
	case FCH_ESPI_S0_CONFIG_CLK_FREQ_25MHZ:
		return (25000000);
	case FCH_ESPI_S0_CONFIG_CLK_FREQ_33MHZ:
		return (33000000);
	case FCH_ESPI_S0_CONFIG_CLK_FREQ_50MHZ:
		return (50000000);
	case FCH_ESPI_S0_CONFIG_CLK_FREQ_66MHZ:
		return (66000000);
	default:
		return (0);
	}
}

/*
 * Device Properties
 *
 * What the driver finds out about the controller, the link and the target is
 * published as properties on its node so that it can be read with prtconf(8)
 * or from mdb. The properties are informational and a failure to create one is
 * not treated as an error.
 */

static void
espi_prop_channels(const espi_t *en, char *name, bool pr, bool vw, bool oob,
    bool flash)
{
	char *names[4];
	uint_t n = 0;

	if (pr)
		names[n++] = "peripheral";
	if (vw)
		names[n++] = "vwire";
	if (oob)
		names[n++] = "oob";
	if (flash)
		names[n++] = "flash";

	if (n == 0) {
		(void) ddi_prop_remove(DDI_DEV_T_NONE, en->en_dip, name);
		return;
	}

	(void) ddi_prop_update_string_array(DDI_DEV_T_NONE, en->en_dip, name,
	    names, n);
}

static void
espi_set_props(const espi_t *en)
{
	dev_info_t *dip = en->en_dip;
	const uint32_t cap = en->en_master_cap;
	const uint32_t rst = espi_read(
	    FCH_ESPI_RESERVED_REG0_MMIO(en->en_regs));
	const uint32_t cfg = espi_read(FCH_ESPI_S0_CONFIG_MMIO(en->en_regs));

	(void) ddi_prop_update_string(DDI_DEV_T_NONE, dip, "espi-role",
	    (char *)espi_role_str(en->en_role));
	(void) ddi_prop_update_string(DDI_DEV_T_NONE, dip, "espi-mode",
	    en->en_mode == ESPI_M_OWNER ? "owner" : "passive");

	(void) ddi_prop_update_string(DDI_DEV_T_NONE, dip,
	    "controller-version", (char *)espi_ctrl_ver_str(cap));
	espi_prop_channels(en, "controller-channels",
	    FCH_ESPI_MASTER_CAP_GET_PR(cap) != 0,
	    FCH_ESPI_MASTER_CAP_GET_VW(cap) != 0,
	    FCH_ESPI_MASTER_CAP_GET_OOB(cap) != 0,
	    FCH_ESPI_MASTER_CAP_GET_FLASH(cap) != 0);
	(void) ddi_prop_update_string(DDI_DEV_T_NONE, dip, "init-status",
	    (char *)espi_init_stat_str(rst));

	(void) ddi_prop_update_string(DDI_DEV_T_NONE, dip, "io-mode",
	    (char *)espi_iomode_str(FCH_ESPI_S0_CONFIG_GET_IO_MODE_SEL(cfg)));
	(void) ddi_prop_update_int(DDI_DEV_T_NONE, dip, "clock-frequency",
	    espi_freq_hz(FCH_ESPI_S0_CONFIG_GET_CLK_FREQ_SEL(cfg)));
	(void) ddi_prop_update_int(DDI_DEV_T_NONE, dip, "crc-enabled",
	    (int)FCH_ESPI_S0_CONFIG_GET_CRC_CHECK_EN(cfg));
	(void) ddi_prop_update_string(DDI_DEV_T_NONE, dip, "alert-mode",
	    FCH_ESPI_S0_CONFIG_GET_ALERT_MODE_SEL(cfg) ==
	    FCH_ESPI_S0_CONFIG_ALERT_MODE_PIN ? "pin" : "in-band");
	espi_prop_channels(en, "enabled-channels",
	    FCH_ESPI_S0_CONFIG_GET_PR_EN(cfg) != 0,
	    FCH_ESPI_S0_CONFIG_GET_VW_EN(cfg) != 0,
	    FCH_ESPI_S0_CONFIG_GET_OOB_EN(cfg) != 0,
	    FCH_ESPI_S0_CONFIG_GET_FLASH_EN(cfg) != 0);

	if (en->en_pins != ESPI_PINS_UNKNOWN) {
		(void) ddi_prop_update_int(DDI_DEV_T_NONE, dip, "pins-muxed",
		    en->en_pins == ESPI_PINS_OK ? 1 : 0);
	}

	if (!en->en_target)
		return;

	(void) ddi_prop_update_string(DDI_DEV_T_NONE, dip, "target-version",
	    ESPI_REG_IDENT_GET_VERSION(en->en_ident) ==
	    ESPI_REG_IDENT_VERSION_1X ? "1.x" : "unknown");
	espi_prop_channels(en, "target-channels",
	    ESPI_REG_GEN_CAP_GET_PERIPH(en->en_gen_cap) != 0,
	    ESPI_REG_GEN_CAP_GET_VWIRE(en->en_gen_cap) != 0,
	    ESPI_REG_GEN_CAP_GET_OOB(en->en_gen_cap) != 0,
	    ESPI_REG_GEN_CAP_GET_FLASH(en->en_gen_cap) != 0);

	if (!en->en_flash.ef_enabled)
		return;

	(void) ddi_prop_update_int(DDI_DEV_T_NONE, dip, "flash-payload",
	    (int)en->en_flash.ef_payload);
	(void) ddi_prop_update_int(DDI_DEV_T_NONE, dip, "flash-max-read",
	    (int)en->en_flash.ef_maxread);
	(void) ddi_prop_update_int(DDI_DEV_T_NONE, dip, "flash-erase-sizes",
	    (int)en->en_flash.ef_erase_sizes);
}

/*
 * Report the state of the controller, and in owner mode of its link to the
 * target, as it stands after attach.
 */
static void
espi_report(const espi_t *en)
{
	const uint32_t cap = en->en_master_cap;
	const uint32_t rst = espi_read(
	    FCH_ESPI_RESERVED_REG0_MMIO(en->en_regs));
	const uint32_t cfg = espi_read(FCH_ESPI_S0_CONFIG_MMIO(en->en_regs));
	const char *mode = en->en_mode == ESPI_M_OWNER ? "owner" : "passive";

	dev_err(en->en_dip, CE_CONT, "?eSPI controller %u (%s): version %s, "
	    "channels%s%s%s%s, hardware initialisation %s\n", en->en_unit,
	    mode, espi_ctrl_ver_str(cap),
	    FCH_ESPI_MASTER_CAP_GET_PR(cap) != 0 ? " PR" : "",
	    FCH_ESPI_MASTER_CAP_GET_VW(cap) != 0 ? " VW" : "",
	    FCH_ESPI_MASTER_CAP_GET_OOB(cap) != 0 ? " OOB" : "",
	    FCH_ESPI_MASTER_CAP_GET_FLASH(cap) != 0 ? " FLASH" : "",
	    espi_init_stat_str(rst));
	dev_err(en->en_dip, CE_CONT, "?link: %s I/O, %s, CRC %s, %s alert, "
	    "enabled channels%s%s%s%s%s\n",
	    espi_iomode_str(FCH_ESPI_S0_CONFIG_GET_IO_MODE_SEL(cfg)),
	    espi_freq_str(FCH_ESPI_S0_CONFIG_GET_CLK_FREQ_SEL(cfg)),
	    FCH_ESPI_S0_CONFIG_GET_CRC_CHECK_EN(cfg) != 0 ? "on" : "off",
	    FCH_ESPI_S0_CONFIG_GET_ALERT_MODE_SEL(cfg) ==
	    FCH_ESPI_S0_CONFIG_ALERT_MODE_PIN ? "pin" : "in-band",
	    FCH_ESPI_S0_CONFIG_GET_PR_EN(cfg) != 0 ? " PR" : "",
	    FCH_ESPI_S0_CONFIG_GET_VW_EN(cfg) != 0 ? " VW" : "",
	    FCH_ESPI_S0_CONFIG_GET_OOB_EN(cfg) != 0 ? " OOB" : "",
	    FCH_ESPI_S0_CONFIG_GET_FLASH_EN(cfg) != 0 ? " FLASH" : "",
	    (FCH_ESPI_S0_CONFIG_GET_PR_EN(cfg) |
	    FCH_ESPI_S0_CONFIG_GET_VW_EN(cfg) |
	    FCH_ESPI_S0_CONFIG_GET_OOB_EN(cfg) |
	    FCH_ESPI_S0_CONFIG_GET_FLASH_EN(cfg)) == 0 ? " none" : "");

	if (!en->en_target)
		return;

	dev_err(en->en_dip, CE_CONT, "?target: eSPI %s, general "
	    "capabilities 0x%x, channels%s%s%s%s\n",
	    ESPI_REG_IDENT_GET_VERSION(en->en_ident) ==
	    ESPI_REG_IDENT_VERSION_1X ? "1.x" : "unknown version",
	    en->en_gen_cap,
	    ESPI_REG_GEN_CAP_GET_PERIPH(en->en_gen_cap) != 0 ? " PR" : "",
	    ESPI_REG_GEN_CAP_GET_VWIRE(en->en_gen_cap) != 0 ? " VW" : "",
	    ESPI_REG_GEN_CAP_GET_OOB(en->en_gen_cap) != 0 ? " OOB" : "",
	    ESPI_REG_GEN_CAP_GET_FLASH(en->en_gen_cap) != 0 ? " FLASH" : "");

	if (en->en_flash.ef_enabled) {
		dev_err(en->en_dip, CE_CONT, "?flash: SAFS, payload %u bytes, "
		    "max read %u bytes, erase sizes 0x%x\n",
		    en->en_flash.ef_payload, en->en_flash.ef_maxread,
		    en->en_flash.ef_erase_sizes);
	}
}

static int
espi_probe(dev_info_t *dip)
{
	uint_t unit;

	if (!espi_unit(dip, &unit))
		return (DDI_PROBE_FAILURE);

	switch (espi_role(unit)) {
	case OXIDE_ESPI_ROLE_IPCC:
	case OXIDE_ESPI_ROLE_NEXUS:
		return (DDI_PROBE_SUCCESS);
	default:
		return ((espi_force_passive & (1U << unit)) != 0 ?
		    DDI_PROBE_SUCCESS : DDI_PROBE_FAILURE);
	}
}

static int
espi_attach(dev_info_t *dip, ddi_attach_cmd_t cmd)
{
	ddi_device_acc_attr_t attr = {
		.devacc_attr_version = DDI_DEVICE_ATTR_V1,
		.devacc_attr_endian_flags = DDI_NEVERSWAP_ACC,
		.devacc_attr_dataorder = DDI_STRICTORDER_ACC,
		.devacc_attr_access = DDI_DEFAULT_ACC
	};
	const int inst = ddi_get_instance(dip);
	espi_t *en;
	uint_t unit;

	switch (cmd) {
	case DDI_ATTACH:
		break;
	case DDI_RESUME:
		return (DDI_SUCCESS);
	default:
		return (DDI_FAILURE);
	}

	if (!espi_unit(dip, &unit)) {
		dev_err(dip, CE_WARN, "cannot determine controller from unit "
		    "address");
		return (DDI_FAILURE);
	}

	if (ddi_soft_state_zalloc(espi_state, inst) != DDI_SUCCESS) {
		dev_err(dip, CE_WARN, "failed to allocate soft state");
		return (DDI_FAILURE);
	}
	en = ddi_get_soft_state(espi_state, inst);
	en->en_dip = dip;
	en->en_unit = unit;
	en->en_role = espi_role(unit);
	mutex_init(&en->en_lock, NULL, MUTEX_DRIVER, NULL);

	if (x_ddi_reg_block_setup(dip, 0, &attr, &en->en_regs) !=
	    DDI_SUCCESS) {
		dev_err(dip, CE_WARN, "failed to map registers");
		goto err;
	}

	en->en_master_cap = espi_read(FCH_ESPI_MASTER_CAP_MMIO(en->en_regs));
	if (FCH_ESPI_MASTER_CAP_GET_VER(en->en_master_cap) !=
	    FCH_ESPI_MASTER_CAP_VER_1_0) {
		dev_err(dip, CE_WARN, "unsupported eSPI controller version %u",
		    FCH_ESPI_MASTER_CAP_GET_VER(en->en_master_cap));
		goto err;
	}

	switch (en->en_role) {
	case OXIDE_ESPI_ROLE_NEXUS:
		/*
		 * The pins are ours to configure. Refusing to drive a
		 * controller whose pins could not be muxed is safer than
		 * failing to attach, as the node then still shows what we
		 * found.
		 */
		if (espi_iomux(en, true) != ESPI_PINS_WRONG) {
			en->en_mode = ESPI_M_OWNER;
			espi_owner_init(en);
		} else {
			dev_err(dip, CE_WARN, "pins could not be muxed for "
			    "eSPI, attaching in passive mode");
			en->en_mode = ESPI_M_PASSIVE;
		}
		break;
	case OXIDE_ESPI_ROLE_IPCC:
		en->en_mode = ESPI_M_PASSIVE;
		(void) espi_iomux(en, false);
		break;
	default:
		if ((espi_force_passive & (1U << unit)) != 0) {
			en->en_mode = ESPI_M_PASSIVE;
			(void) espi_iomux(en, false);
			break;
		}
		/* probe(9E) does not let us get here. */
		dev_err(dip, CE_WARN, "controller %u has no role", unit);
		goto err;
	}

	espi_set_props(en);
	espi_report(en);
	ddi_report_dev(dip);
	return (DDI_SUCCESS);

err:
	x_ddi_reg_block_free(&en->en_regs);
	mutex_destroy(&en->en_lock);
	ddi_soft_state_free(espi_state, inst);
	return (DDI_FAILURE);
}

static int
espi_detach(dev_info_t *dip, ddi_detach_cmd_t cmd)
{
	const int inst = ddi_get_instance(dip);
	espi_t *en;

	switch (cmd) {
	case DDI_DETACH:
		break;
	case DDI_SUSPEND:
		return (DDI_SUCCESS);
	default:
		return (DDI_FAILURE);
	}

	en = ddi_get_soft_state(espi_state, inst);
	if (en == NULL || en->en_dip != dip) {
		dev_err(dip, CE_WARN, "asked to detach instance with no state");
		return (DDI_FAILURE);
	}

	/*
	 * The link to the target is left as it is. A later attach starts again
	 * with an in-band reset.
	 */
	ddi_prop_remove_all(dip);
	x_ddi_reg_block_free(&en->en_regs);
	mutex_destroy(&en->en_lock);
	ddi_soft_state_free(espi_state, inst);
	return (DDI_SUCCESS);
}

static struct dev_ops espi_dev_ops = {
	.devo_rev = DEVO_REV,
	.devo_refcnt = 0,
	.devo_getinfo = nodev,
	.devo_identify = nulldev,
	.devo_probe = espi_probe,
	.devo_attach = espi_attach,
	.devo_detach = espi_detach,
	.devo_reset = nodev,
	.devo_quiesce = ddi_quiesce_not_needed
};

static struct modldrv espi_modldrv = {
	.drv_modops = &mod_driverops,
	.drv_linkinfo = "eSPI Nexus Driver",
	.drv_dev_ops = &espi_dev_ops
};

static struct modlinkage espi_modlinkage = {
	.ml_rev = MODREV_1,
	.ml_linkage = { &espi_modldrv, NULL }
};

int
_init(void)
{
	int ret;

	ret = ddi_soft_state_init(&espi_state, sizeof (espi_t),
	    FCH_MAX_ESPI);
	if (ret != 0)
		return (ret);

	if ((ret = mod_install(&espi_modlinkage)) != 0)
		ddi_soft_state_fini(&espi_state);

	return (ret);
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&espi_modlinkage, modinfop));
}

int
_fini(void)
{
	int ret;

	if ((ret = mod_remove(&espi_modlinkage)) == 0)
		ddi_soft_state_fini(&espi_state);

	return (ret);
}
