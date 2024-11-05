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
 */

#include <sys/stdbool.h>
#include <sys/types.h>
#include <sys/cmn_err.h>
#include <sys/archsystm.h>
#include <sys/cpu.h>
#include <sys/prom_debug.h>
#include <sys/boot_data.h>
#include <sys/boot_debug.h>
#include <sys/bootconf.h>
#include <sys/ipcc_proto.h>
#include <sys/platform_detect.h>
#include <sys/espi.h>
#include <sys/io/fch/espi.h>
#include <vm/kboot_mmu.h>

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
espi_acquire(mmio_reg_block_t block)
{
	mmio_reg_t reg = FCH_ESPI_SEM_MISC_CTL_REG0_MMIO(block);

	for (;;) {
		/*
		 * Poll for idle
		 */
		uint32_t val = mmio_reg_read(reg);

		if (FCH_ESPI_SEM_MISC_CTL_REG0_GET_SW4_USER_ID(val) != 0 ||
		    FCH_ESPI_SEM_MISC_CTL_REG0_GET_SW0_OWN_STAT(val) != 0 ||
		    FCH_ESPI_SEM_MISC_CTL_REG0_GET_SW1_OWN_STAT(val) != 0 ||
		    FCH_ESPI_SEM_MISC_CTL_REG0_GET_SW2_OWN_STAT(val) != 0 ||
		    FCH_ESPI_SEM_MISC_CTL_REG0_GET_SW3_OWN_STAT(val) != 0) {
			DTRACE_PROBE1(espi__idle__poll, uint32_t, val);
			// XXX Milan and Genoa define these bits as DBGSEL_LOW,
			// and default to 0x31. Short circuit during
			// development.
			if (oxide_board_data->obd_cpuinfo.obc_fchkind <
			    FK_KUNLUN &&
			    FCH_ESPI_SEM_MISC_CTL_REG0_GET_SW4_USER_ID(val) ==
			    0x31) {
				return (0);
			}
			espi_pause(100);
			continue;
		}

		FCH_ESPI_SEM_MISC_CTL_REG0_SET_SW3_OWN_SET(val, 1);
		FCH_ESPI_SEM_MISC_CTL_REG0_SET_SW3_OWN_CLR(val, 0);

		mmio_reg_write(reg, val);
		val = mmio_reg_read(reg);

		if (FCH_ESPI_SEM_MISC_CTL_REG0_GET_SW3_OWN_STAT(val) == 1) {
			DTRACE_PROBE1(espi__sem__acquired, uint32_t, val);
			break;
		}
		DTRACE_PROBE1(espi__sem__failed, uint32_t, val);
		espi_pause(100);
	}

	return (0);
}

void
espi_release(mmio_reg_block_t block)
{
	mmio_reg_t reg = FCH_ESPI_SEM_MISC_CTL_REG0_MMIO(block);

	uint32_t val = mmio_reg_read(reg);
	FCH_ESPI_SEM_MISC_CTL_REG0_SET_SW3_OWN_CLR(val, 1);
	mmio_reg_write(reg, val);
	val = mmio_reg_read(reg);
	while (FCH_ESPI_SEM_MISC_CTL_REG0_GET_SW0_OWN_STAT(val) != 0) {
		DTRACE_PROBE1(espi__sem__releasing, uint32_t, val);
		espi_pause(100);
		val = mmio_reg_read(reg);
	}
	FCH_ESPI_SEM_MISC_CTL_REG0_SET_SW3_OWN_CLR(val, 0);
	FCH_ESPI_SEM_MISC_CTL_REG0_SET_SW3_OWN_SET(val, 0);
	mmio_reg_write(reg, val);
	DTRACE_PROBE1(espi__sem__released, uint32_t, val);
}

bool
espi_readable(mmio_reg_block_t block)
{
	mmio_reg_t reg = FCH_ESPI_S0_INT_STS_MMIO(block);
	uint32_t val;

	val = mmio_reg_read(reg);
	return (FCH_ESPI_S0_INT_STS_GET_RXMSG(val) == 1);
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
	//TBD
}

int
espi_tx(mmio_reg_block_t block, uint8_t tag, uint8_t *buf, size_t *lenp)
{
	mmio_reg_t hdr0 = FCH_ESPI_DN_TXHDR0_MMIO(block);
	uint32_t val;
	size_t len = *lenp;

	/* Wait until the hardware is ready */
	for (;;) {
		val = mmio_reg_read(hdr0);
		if (FCH_ESPI_DN_TXHDR0_GET_DNCMD_STATUS(val) == 0)
			break;
		espi_pause(100);
	}

	/* Set the command type - Peripheral Channel message downstream */
	val = FCH_ESPI_DN_TXHDR0_SET_DNCMD_TYPE(val,
	    FCH_ESPI_DN_TXHDR0_TYPE_PERIPH);
	mmio_reg_write(hdr0, val);

	/* Set the cycle type, tag and length */
	val = FCH_ESPI_DN_TXHDR0_SET_CYCLE(val, 0);
	val = FCH_ESPI_DN_TXHDR0_SET_TAG(val, tag);
	val = FCH_ESPI_DN_TXHDR0_SET_LENH(val, bitx32(len, 11, 8));
	val = FCH_ESPI_DN_TXHDR0_SET_LENL(val, bitx32(len, 7, 0));
	mmio_reg_write(hdr0, val);

	/* Additional header data */
	mmio_reg_t hdr1 = FCH_ESPI_DN_TXHDR1_MMIO(block);
	mmio_reg_t hdr2 = FCH_ESPI_DN_TXHDR2_MMIO(block);
	uint32_t val1 = 0, val2 = 0;
	val1 = FCH_ESPI_DN_TXHDR1_SET_HDATA3(val1, 0);	/* Message code */
	val1 = FCH_ESPI_DN_TXHDR1_SET_HDATA4(val1, bitx32(IPCC_MAGIC, 31, 24));
	val1 = FCH_ESPI_DN_TXHDR1_SET_HDATA5(val1, bitx32(IPCC_MAGIC, 23, 16));
	val1 = FCH_ESPI_DN_TXHDR1_SET_HDATA6(val1, bitx32(IPCC_MAGIC, 15, 8));
	val2 = FCH_ESPI_DN_TXHDR2_SET_HDATA7(val2, bitx32(IPCC_MAGIC, 7, 0));
	mmio_reg_write(hdr1, val1);
	mmio_reg_write(hdr2, val2);

	/* Submit data, in blocks of four bytes - XXX TBD 32 byte limit? */
	mmio_reg_t data = FCH_ESPI_DN_TXDATA_PORT_MMIO(block);
	while (len > 0) {
		val = 0;
		for (uint_t i = 0; i < 4; i++) {
			if (len == 0)
				break;
			val |= (buf[0] << (8 * i));
			len--;
			buf++;
		}
		mmio_reg_write(data, val);
	}
	/* We wrote it all so we leave *lenp unchanged */

	/* Mark ready to send */
	val = mmio_reg_read(hdr0);
	val = FCH_ESPI_DN_TXHDR0_CLEAR_DNCMD_STATUS(val);	//XXX RW0C
	mmio_reg_write(hdr0, val);

	//Poll for now
	for (;;) {
		val = mmio_reg_read(hdr0);
		if (FCH_ESPI_DN_TXHDR0_GET_DNCMD_STATUS(val) == 0)
			return (0);
		espi_pause(100);
	}
}

int
espi_rx(mmio_reg_block_t block, uint8_t etag, uint8_t *buf, size_t *buflen)
{
	mmio_reg_t intsts = FCH_ESPI_S0_INT_STS_MMIO(block);
	uint32_t val;
	uint8_t tag;
	size_t len;

	for (;;) {
		//Poll for now
		val = mmio_reg_read(intsts);
		if (FCH_ESPI_S0_INT_STS_GET_RXMSG(val) == 1)
			break;
		DTRACE_PROBE1(espi__rx__intr, uint32_t, val);
		espi_pause(100);
	}

	/* Clear the interrupt status */
	mmio_reg_write(intsts, FCH_ESPI_S0_INT_STS_CLEAR_RXMSG(0));

	mmio_reg_t hdr0 = FCH_ESPI_S0_RXMSG_HDR0_MMIO(block);
	//mmio_reg_t hdr1 = FCH_ESPI_S0_RXMSG_HDR1_MMIO(block);

	val = mmio_reg_read(hdr0);

	tag = FCH_ESPI_S0_RXMSG_HDR0_TAG(val);
	if (tag != etag) {
		bop_panic("Unexpected eSPI tag; got 0x%x, expected 0x%x",
		    tag, etag);
	}

	len = bitset32(0, 11, 8, FCH_ESPI_S0_RXMSG_HDR0_LENH(val));
	len = bitset32(len, 7, 0, FCH_ESPI_S0_RXMSG_HDR0_LENL(val));

	if (len > *buflen)
		return (EOVERFLOW);

	*buflen = len;

	mmio_reg_t data = FCH_ESPI_S0_RXMSG_DATA_PORT_MMIO(block);
	while (len > 0) {
		val = mmio_reg_read(data);
		for (uint_t i = 0; i < 4; i++) {
			if (len == 0)
				break;
			buf[0] = bitx32(val, (i + 1) * 8 - 1, i * 8);
			len--;
			buf++;
		}
	}

	return (0);
}

uint32_t
espi_get_configuration(mmio_reg_block_t block)
{
	mmio_reg_t hdr0 = FCH_ESPI_DN_TXHDR0_MMIO(block);
	mmio_reg_t hdr1 = FCH_ESPI_DN_TXHDR1_MMIO(block);
	mmio_reg_t intsts = FCH_ESPI_S0_INT_STS_MMIO(block);
	uint32_t val;

	(void) espi_acquire(block);

	/* Wait until the hardware is ready */
	for (;;) {
		val = mmio_reg_read(hdr0);
		if (FCH_ESPI_DN_TXHDR0_GET_DNCMD_STATUS(val) == 0)
			break;
		espi_pause(100);
	}

	/* Set the command type - GET_CONFIGURATION */
	val = FCH_ESPI_DN_TXHDR0_SET_DNCMD_TYPE(val,
	    FCH_ESPI_DN_TXHDR0_TYPE_GETCONF);
	mmio_reg_write(hdr0, val);

	/* Mark ready to send */
	val = mmio_reg_read(hdr0);
	val = FCH_ESPI_DN_TXHDR0_CLEAR_DNCMD_STATUS(val);	//XXX RW0C
	mmio_reg_write(hdr0, val);

	for (;;) {
		val = mmio_reg_read(hdr0);
		if (FCH_ESPI_DN_TXHDR0_GET_DNCMD_STATUS(val) == 0)
			break;
		espi_pause(100);
	}

	for (;;) {
		val = mmio_reg_read(intsts);
		if (FCH_ESPI_S0_INT_STS_GET_DNCMD(val) == 1)
			break;
		DTRACE_PROBE1(espi__getconf__intr, uint32_t, val);
		espi_pause(100);
	}

	/* Clear the interrupt status */
	mmio_reg_write(intsts, FCH_ESPI_S0_INT_STS_CLEAR_DNCMD(0));

	val = mmio_reg_read(hdr1);

	(void) espi_release(block);

	return (val);
}
