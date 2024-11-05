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
 * Copyright 2024 Oxide Computer Company
 */

/*
 * XXX
 *
 * This code is executed very early in unix before a lot of niceties are
 * available. Avoid using ASSERT/VERIFY, DTRACE_PROBExx, cmn_err and things
 * from genunix such as mutexes without checking that we are far enough along
 * via the global `standalone` variable being 0.
 */

#include <sys/stdbool.h>
#include <sys/types.h>
#include <sys/archsystm.h>
#include <sys/cpu.h>
#include <sys/prom_debug.h>
#include <sys/boot_data.h>
#include <sys/boot_debug.h>
#include <sys/bootconf.h>
#include <sys/sdt.h>
#include <sys/ipcc_proto.h>
#include <sys/platform_detect.h>
#include <sys/espi.h>
#include <sys/io/fch/espi.h>
#include <vm/kboot_mmu.h>

extern int standalone;

typedef struct espi_data {
	bool		ep_active;
	uint32_t	ep_reg_gencap;
	uint32_t	ep_reg_oobcap;
	size_t		ep_hostmaxpayload;
	size_t		ep_selpayload;
	size_t		ep_maxpayload;


} espi_data_t;

static espi_data_t espi_data;

// XXX - needs experimentation - is 32 the limit or can we use
// ep_selpayload, possibly -overhead?
// Experimentation shows we're ok up to 256 bytes on the peripheral channel,
// above that the SP5 aborts the transaction. Currently unknown for OOB but
// expect it will also be 256.
// NB: FCH::ITF::ESPI::ESPI_MISC_CONTROL_REG0[OOB_LENGTH_LIMIT_EN] allows
// relaxation of the limit beyond that permitted by the eSPI spec.
uint_t ESPI_MAX_PERIPH_DATA = 32;

// XXX - in ms
#define	ESPI_DELAY	10
#define	ESPI_RETRIES	100

static inline void
espi_pause(uint64_t delay_ms)
{
	const hrtime_t delay_ns = MSEC2NSEC(delay_ms);
	extern int gethrtime_hires;

	if (gethrtime_hires != 0) {
		/* The TSC is calibrated, we can use drv_usecwait() */
		drv_usecwait(NSEC2USEC(delay_ns));
	} else {
		/*
		 * The TSC has not yet been calibrated so assume its frequency
		 * is 2GHz (2 ticks per nanosecond). This is approximately
		 * correct for Gimlet and should be the right order of
		 * magnitude for future platforms. This delay does not have be
		 * accurate.
		 */
		const hrtime_t start = tsc_read();
		while (tsc_read() < start + (delay_ns << 1))
			SMT_PAUSE();
	}
}

int
espi_init(mmio_reg_block_t block) {
	const char *freq, *mode;
	mmio_reg_t reg;
	uint32_t val;
	int ret;

	reg = FCH_ESPI_RESERVED_REG0_MMIO(block);
	val = mmio_reg_read(reg);
	if (FCH_ESPI_RESERVED_REG0_INIT_STAT(val) !=
	    FCH_ESPI_RESERVED_REG0_INIT_STAT_SUCCESS) {
		EB_DBGMSG("eSPI hardware not successfully initialised\n");
		return (ENXIO);
	}

	espi_data.ep_active = true;
	ret = espi_acquire(block);
	espi_data.ep_active = false;

	if (ret != 0) {
		EB_DBGMSG("Could not acquire eSPI semaphore\n");
		return (ret);
	}

	ret = ENXIO;

	val = espi_get_configuration(block, ESPI_REG_IDENT);
	if (ESPI_REG_IDENT_VERSION(val) != 1) {
		EB_DBGMSG("Unsupported eSPI version %u\n",
		    ESPI_REG_IDENT_VERSION(val));
		goto out;
	}

	val = espi_get_configuration(block, ESPI_REG_GEN_CAP);
	espi_data.ep_reg_gencap = val;
	if (ESPI_REG_GEN_CAP_OOB(val) == 0) {
		EB_DBGMSG("OOB channel is not supported\n");
		goto out;
	}

	switch (ESPI_REG_GEN_CAP_IOMODE(val)) {
	case ESPI_REG_GEN_CAP_IOMODE_SINGLE:
		mode = "x1";
		break;
	case ESPI_REG_GEN_CAP_IOMODE_DUAL:
		mode = "x2";
		break;
	case ESPI_REG_GEN_CAP_IOMODE_QUAD:
		mode = "x4";
		break;
	default:
		mode = "??";
	}

	switch (ESPI_REG_GEN_CAP_FREQ(val)) {
	case ESPI_REG_GEN_CAP_FREQ_20MHZ:
		freq = "20MHz";
		break;
	case ESPI_REG_GEN_CAP_FREQ_25MHZ:
		freq = "25MHz";
		break;
	case ESPI_REG_GEN_CAP_FREQ_35MHZ:
		freq = "35MHz";
		break;
	case ESPI_REG_GEN_CAP_FREQ_50MHZ:
		freq = "50MHz";
		break;
	case ESPI_REG_GEN_CAP_FREQ_66MHZ:
		freq = "66MHz";
		break;
	default:
		freq = "?MHz";
	}

	val = espi_get_configuration(block, ESPI_REG_CHAN2_CAP);
	espi_data.ep_reg_oobcap = val;
	if (ESPI_REG_CHAN2_CAP_EN(val) == 0) {
		EB_DBGMSG("OOB channel not enabled\n");
		goto out;
	}
	if (ESPI_REG_CHAN2_CAP_READY(val) == 0) {
		EB_DBGMSG("OOB channel not ready\n");
		goto out;
	}

	espi_data.ep_selpayload = ESPI_SIZE_TO_BYTES(
	    ESPI_REG_CHAN2_CAP_SELPAYLOAD(val));
	espi_data.ep_maxpayload = ESPI_SIZE_TO_BYTES(
	    ESPI_REG_CHAN2_CAP_MAXPAYLOAD(val));

	reg = FCH_ESPI_MASTER_CAP_MMIO(block);
	val = mmio_reg_read(reg);
	espi_data.ep_hostmaxpayload = ESPI_SIZE_TO_BYTES(
	    FCH_ESPI_MASTER_CAP_PR_MAXSZ(val));

	reg = FCH_ESPI_MISC_CTL0_MMIO(block);
	val = mmio_reg_read(reg);

	if (FCH_ESPI_MISC_CTL0_OOB_FREE(val) != 1) {
		EB_DBGMSG("OOB channel is not free\n");
		goto out;
	}

	EB_DBGMSG("Successfully initialised eSPI -- %s %s\n", freq, mode);

	ret = 0;
	espi_data.ep_active = true;

out:
	espi_release(block);
	return (ret);
}

int
espi_acquire(mmio_reg_block_t block)
{
	mmio_reg_t reg = FCH_ESPI_SEM_MISC_CTL_REG0_MMIO(block);
	uint32_t val;
	int ret = ETIMEDOUT;

	if (!espi_data.ep_active)
		return (ENXIO);

	val = mmio_reg_read(reg);

	if (standalone == 0) {
		VERIFY0(FCH_ESPI_SEM_MISC_CTL_REG0_GET_SW2_OWN_STAT(val));
	}

	for (uint_t i = 0; i < ESPI_RETRIES; i++) {
		/* Poll for idle */
		if (FCH_ESPI_SEM_MISC_CTL_REG0_GET_SW4_USER_ID(val) != 0 ||
		    FCH_ESPI_SEM_MISC_CTL_REG0_GET_SW0_OWN_STAT(val) != 0 ||
		    FCH_ESPI_SEM_MISC_CTL_REG0_GET_SW1_OWN_STAT(val) != 0 ||
		    FCH_ESPI_SEM_MISC_CTL_REG0_GET_SW2_OWN_STAT(val) != 0 ||
		    FCH_ESPI_SEM_MISC_CTL_REG0_GET_SW3_OWN_STAT(val) != 0) {
			if (standalone == 0) {
				DTRACE_PROBE1(espi__acquire__locked,
				    uint32_t, val);
			}
			espi_pause(ESPI_DELAY);
			continue;
		}

		/*
		 * Attempt to acquire the semaphore as owner 2
		 * (reserved for x86).
		 */
		val = FCH_ESPI_SEM_MISC_CTL_REG0_SET_SW2_OWN_SET(val, 1);
		val = FCH_ESPI_SEM_MISC_CTL_REG0_SET_SW2_OWN_CLR(val, 0);

		mmio_reg_write(reg, val);
		val = mmio_reg_read(reg);

		/* Confirm semaphore acquisition */
		if (FCH_ESPI_SEM_MISC_CTL_REG0_GET_SW2_OWN_STAT(val) == 1) {
			/* Success */
			ret = 0;
			break;
		}

		if (standalone == 0) {
			DTRACE_PROBE1(espi__acquire__failed, uint32_t, val);
		}

		espi_pause(ESPI_DELAY);
	}

	return (ret);
}

void
espi_release(mmio_reg_block_t block)
{
	mmio_reg_t reg = FCH_ESPI_SEM_MISC_CTL_REG0_MMIO(block);

	uint32_t val = mmio_reg_read(reg);

	if (standalone == 0) {
		VERIFY(espi_data.ep_active);
		VERIFY(FCH_ESPI_SEM_MISC_CTL_REG0_GET_SW2_OWN_STAT(val));
	}

	/* Release semaphore */
	val = FCH_ESPI_SEM_MISC_CTL_REG0_SET_SW2_OWN_CLR(val, 1);
	mmio_reg_write(reg, val);
	val = mmio_reg_read(reg);

	/* Wait for ownership status to change */
	while (FCH_ESPI_SEM_MISC_CTL_REG0_GET_SW2_OWN_STAT(val) != 0) {
		if (standalone == 0) {
			DTRACE_PROBE1(espi__release__wait, uint32_t, val);
		}
		espi_pause(ESPI_DELAY);
		val = mmio_reg_read(reg);
	}

	/* Complete release operation */
	val = FCH_ESPI_SEM_MISC_CTL_REG0_SET_SW2_OWN_CLR(val, 0);
	val = FCH_ESPI_SEM_MISC_CTL_REG0_SET_SW2_OWN_SET(val, 0);
	mmio_reg_write(reg, val);

}

uint32_t
espi_intstatus(mmio_reg_block_t block)
{
	mmio_reg_t reg = FCH_ESPI_S0_INT_STS_MMIO(block);

	return (mmio_reg_read(reg));
}

bool
espi_readable(mmio_reg_block_t block)
{
	mmio_reg_t reg = FCH_ESPI_S0_INT_STS_MMIO(block);
	uint32_t val;

	val = mmio_reg_read(reg);
	return (FCH_ESPI_S0_INT_STS_GET_RXOOB(val) == 1);
}

bool
espi_writable(mmio_reg_block_t block)
{
	mmio_reg_t reg = FCH_ESPI_DN_TXHDR0_MMIO(block);
	uint32_t val;

	val = mmio_reg_read(reg);
	return (FCH_ESPI_DN_TXHDR0_GET_DNCMD_STATUS(val) == 0);
}

void
espi_flush(mmio_reg_block_t block)
{
	/* Drain the input buffer */
	while (espi_readable(block))
		(void) espi_rx(block, NULL, NULL);

	mmio_reg_t intsts = FCH_ESPI_S0_INT_STS_MMIO(block);
	mmio_reg_write(intsts, FCH_ESPI_S0_INT_STS_CLEAR_RXMSG_OFLOW(0));
}

static int
espi_wait_idle(mmio_reg_block_t block)
{
	mmio_reg_t hdr0 = FCH_ESPI_DN_TXHDR0_MMIO(block);
	mmio_reg_t ctl0 = FCH_ESPI_MISC_CTL0_MMIO(block);
	uint32_t val;
	int ret = ETIMEDOUT;

	val = mmio_reg_read(ctl0);
	if (FCH_ESPI_MISC_CTL0_OOB_FREE(val) != 1)
		return (ENXIO);

	/* Wait until the hardware is ready */
	for (uint_t i = 0; i < ESPI_RETRIES; i++) {
		val = mmio_reg_read(hdr0);
		if (FCH_ESPI_DN_TXHDR0_GET_DNCMD_STATUS(val) == 0) {
			ret = 0;
			break;
		}
		if (standalone == 0) {
			DTRACE_PROBE1(espi__wait__idle, uint32_t, val);
		}
		espi_pause(ESPI_DELAY);
	}

	return (ret);
}

static int
espi_submit(mmio_reg_block_t block)
{
	mmio_reg_t hdr0_type = FCH_ESPI_DN_TXHDR0_TYPE_MMIO(block);
	mmio_reg_t intsts = FCH_ESPI_S0_INT_STS_MMIO(block);
	uint32_t val;
	int ret = ETIMEDOUT;

	/* XXX Clear status interrupt flags */
	val = 0;
	val = FCH_ESPI_S0_INT_STS_CLEAR_PROTOERR(val);
	val = FCH_ESPI_S0_INT_STS_CLEAR_RXMSG_OFLOW(val);
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
	mmio_reg_write(intsts, val);

	/* Clear the "Downstream Register Command Complete" interrupt status */
	mmio_reg_write(intsts, FCH_ESPI_S0_INT_STS_CLEAR_DNCMD(0));

	/* Mark ready to send */
	val = mmio_reg_read(hdr0_type);
	val = FCH_ESPI_DN_TXHDR0_SET_DNCMD_STATUS(val, 1);
	mmio_reg_write(hdr0_type, val);

	/* Poll for interrupt */
	for (uint_t i = 0; i < ESPI_RETRIES; i++) {
		val = mmio_reg_read(intsts);
		if (FCH_ESPI_S0_INT_STS_GET_DNCMD(val) == 1) {
			ret = 0;
			break;
		}
		if (standalone == 0) {
			DTRACE_PROBE1(espi__submit__waitintr, uint32_t, val);
		}
		espi_pause(ESPI_DELAY);
	}
	if (ret != 0)
		return (ret);

	/* Clear the "Downstream Register Command Complete" interrupt status */
	mmio_reg_write(intsts, FCH_ESPI_S0_INT_STS_CLEAR_DNCMD(0));

	/* Poll for completion */
	for (uint_t i = 0; i < ESPI_RETRIES; i++) {
		val = mmio_reg_read(hdr0_type);
		if (FCH_ESPI_DN_TXHDR0_GET_DNCMD_STATUS(val) == 0) {
			ret = 0;
			break;
		}
		if (standalone == 0) {
			DTRACE_PROBE1(espi__submit__wait, uint32_t, val);
		}
		espi_pause(ESPI_DELAY);
	}

	return (ret);
}

uint32_t
espi_get_configuration(mmio_reg_block_t block, uint16_t reg)
{
	mmio_reg_t hdr0 = FCH_ESPI_DN_TXHDR0_MMIO(block);
	mmio_reg_t hdr1 = FCH_ESPI_DN_TXHDR1_MMIO(block);
	uint32_t val;

	if ((reg & 0x3) != 0 || (reg >> 12) != 0)
		return (UINT32_MAX);

	if (espi_wait_idle(block) != 0)
		return (UINT32_MAX);

	/* Set the command type - GET_CONFIGURATION */
	val = FCH_ESPI_DN_TXHDR0_SET_DNCMD_TYPE(0,
	    FCH_ESPI_DN_TXHDR0_TYPE_GETCONF);
	/* Set the requested address (register) */
	val = FCH_ESPI_DN_TXHDR0_SET_HDATA2(val, 0);	/* Reserved */
	/* Address[7:0] */
	val = FCH_ESPI_DN_TXHDR0_SET_HDATA1(val, reg & 0xff);
	/* 0000_Address[11:8] */
	val = FCH_ESPI_DN_TXHDR0_SET_HDATA0(val, (reg >> 8) & 0xf);
	mmio_reg_write(hdr0, val);

	/* It is recommended to set this to 0 to clear any residual value */
	mmio_reg_write(hdr1, 0);

	if (espi_submit(block) != 0)
		return (UINT32_MAX);

	val = mmio_reg_read(hdr1);

	if (standalone == 0) {
		DTRACE_PROBE2(espi__cfg, uint16_t, reg, uint32_t, val);
	}

	return (val);
}

int
espi_tx(mmio_reg_block_t block, uint8_t *buf, size_t *lenp)
{
	static uint8_t tag = 0;
	uint32_t val0, val1;
	size_t len = *lenp;
	size_t written = 0;
	int ret = 0;

	mmio_reg_t hdr0_type = FCH_ESPI_DN_TXHDR0_TYPE_MMIO(block);
	mmio_reg_t hdr0_hdata0 = FCH_ESPI_DN_TXHDR0_HDATA0_MMIO(block);
	mmio_reg_t hdr0_hdata1 = FCH_ESPI_DN_TXHDR0_HDATA1_MMIO(block);
	mmio_reg_t hdr0_hdata2 = FCH_ESPI_DN_TXHDR0_HDATA2_MMIO(block);
	mmio_reg_t hdr1 = FCH_ESPI_DN_TXHDR1_MMIO(block);

	while (len > 0) {
		const size_t sendlen = MIN(len, ESPI_MAX_PERIPH_DATA);

		if ((ret = espi_wait_idle(block)) != 0)
			break;

		/* Command type - OOB Channel Message Downstream */
		val0 = FCH_ESPI_DN_TXHDR0_SET_DNCMD_TYPE(0,
		    FCH_ESPI_DN_TXHDR0_TYPE_OOB);

		/* Set the cycle type, tag and length */
		val0 = FCH_ESPI_DN_TXHDR0_SET_CYCLE(val0, 0x21); // MACRO
		val0 = FCH_ESPI_DN_TXHDR0_SET_TAG(val0, (tag++ & 0xf));

		/*
		 * The OOB message packet length is the size of the embedded
		 * data plus three extra bytes to account for the SMBus
		 * header (target, opcode, count). We don't add a PEC byte.
		 */
		const size_t pktlen = sendlen + 3;
		val0 = FCH_ESPI_DN_TXHDR0_SET_LENH(val0, pktlen >> 8);
		val0 = FCH_ESPI_DN_TXHDR0_SET_LENL(val0, pktlen & 0xff);

		/*
		 * AMD sources state that the first two of these must be done
		 * by "byte-write" operations so we use four separate 8-bit
		 * registers.
		 */
		mmio_reg_write(hdr0_type,
		    FCH_ESPI_DN_TXHDR0_GET_DNCMD_TYPE(val0));
		mmio_reg_write(hdr0_hdata0,
		    FCH_ESPI_DN_TXHDR0_GET_HDATA0(val0));
		mmio_reg_write(hdr0_hdata1,
		    FCH_ESPI_DN_TXHDR0_GET_HDATA1(val0));
		mmio_reg_write(hdr0_hdata2,
		    FCH_ESPI_DN_TXHDR0_GET_HDATA2(val0));

		/*
		 * Additional header data.
		 */
		val1 = 0;
		val1 = FCH_ESPI_DN_TXHDR1_SET_HDATA6(val1, 0x0); /* RSVD, 0 */
		val1 = FCH_ESPI_DN_TXHDR1_SET_HDATA5(val1, sendlen);
		val1 = FCH_ESPI_DN_TXHDR1_SET_HDATA4(val1, 0x1); /* Opcode */
		val1 = FCH_ESPI_DN_TXHDR1_SET_HDATA3(val1, 0x1); /* Slave add */

		mmio_reg_write(hdr1, val1);

		if (standalone == 0) {
			DTRACE_PROBE2(espi__tx, uint32_t, val0, uint32_t, val1);
		}

		/* Submit data, in blocks of four bytes */

		mmio_reg_t data = FCH_ESPI_DN_TXDATA_PORT_MMIO(block);
		size_t towrite = sendlen;

		while (towrite > 0) {
			uint32_t val = 0;
			for (uint_t i = 0; i < 4; i++) {
				if (towrite == 0)
					break;
				val |= (buf[0] << (8 * i));
				buf++;
				towrite--;
			}
			mmio_reg_write(data, val);
		}

		if ((ret = espi_submit(block)) != 0)
			break;

		len -= sendlen;
		written += sendlen;
	}

	*lenp = written;

	return (ret);
}

int
espi_rx(mmio_reg_block_t block, uint8_t *buf, size_t *buflen)
{
	size_t accum, space;

	accum = 0;
	space = buf == NULL ? 0 : *buflen;

	for (;;) {
		mmio_reg_t intsts = FCH_ESPI_S0_INT_STS_MMIO(block);
		uint8_t cycle, tag;
		uint32_t val, val0, val1;
		size_t len;

		/* If there is nothing to read, we're done */
		val = mmio_reg_read(intsts);
		if (FCH_ESPI_S0_INT_STS_GET_RXOOB(val) == 0)
			break;

		/* Clear the interrupt status */
		mmio_reg_write(intsts, FCH_ESPI_S0_INT_STS_CLEAR_RXOOB(0));

		mmio_reg_t hdr0 = FCH_ESPI_UP_RXHDR0_MMIO(block);
		mmio_reg_t hdr1 = FCH_ESPI_UP_RXHDR1_MMIO(block);

		/* Check UPCMD_STATUS and UPCMD_TYPE */

		val0 = mmio_reg_read(hdr0);
		cycle = FCH_ESPI_UP_RXHDR0_GET_CYCLE(val0);
		tag = FCH_ESPI_UP_RXHDR0_GET_TAG(val0);

		val1 = mmio_reg_read(hdr1);
		len = FCH_ESPI_UP_RXHDR1_GET_HDATA5(val1);

		if (standalone == 0) {
			DTRACE_PROBE3(espi__rx, size_t, len, uint32_t, val0,
			    uint32_t, val1);
		} else {
			EB_DBGMSG(
			    "RX cycle %x tag %x len %zx val0 %x val1 %x\n",
			    cycle, tag, len, val0, val1);
		}

		/*
		 * If we were called with a NULL buf just throw the data
		 * away to drain the FIFO.
		 */
		if (buf == NULL)
			goto next;

		if (len > space)
			return (EOVERFLOW);

		accum += len;
		space -= len;

		mmio_reg_t data = FCH_ESPI_UP_RXDATA_PORT_MMIO(block);
		while (len > 0) {
			val = mmio_reg_read(data);
			for (uint_t i = 0; i < 4; i++) {
				if (len == 0)
					break;
				*buf = bitx32(val, (i + 1) * 8 - 1, i * 8);
				buf++;
				len--;
			}
		}

next:
		/* Let the hardware know we've retrieved the message */
		mmio_reg_write(hdr0, FCH_ESPI_UP_RXHDR0_CLEAR_UPCMD_STAT(0));
	}

	if (buf != NULL)
		*buflen = accum;

	return (0);
}
