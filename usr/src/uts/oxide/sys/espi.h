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

#ifndef _SYS_ESPI_H
#define	_SYS_ESPI_H

#include <sys/bitext.h>
#include <sys/stdbool.h>
#include <sys/types.h>
#include <sys/uart.h>
#include <sys/amdzen/mmioreg.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ESPI_CYCLE_MESSAGE_WITH_DATA 0x11

/*
 * Convert from the size values found in various register fields to a number of
 * bytes.
 */
#define	ESPI_SIZE_TO_BYTES(r)		(1 << (5 + (r)))

/*
 * eSPI Capabilities and Configuration Registers as defined by the
 * specification.
 */
#define	ESPI_REG_IDENT		0x4
#define	ESPI_REG_IDENT_VERSION(r)		bitx32(r, 7, 0)
#define	ESPI_REG_GEN_CAP	0x8
#define	ESPI_REG_GEN_CAP_IOMODE(r)		bitx32(r, 27, 26)
#define	ESPI_REG_GEN_CAP_IOMODE_SINGLE		0
#define	ESPI_REG_GEN_CAP_IOMODE_DUAL		1
#define	ESPI_REG_GEN_CAP_IOMODE_QUAD		2
#define	ESPI_REG_GEN_CAP_FREQ(r)		bitx32(r, 22, 20)
#define	ESPI_REG_GEN_CAP_FREQ_20MHZ		0
#define	ESPI_REG_GEN_CAP_FREQ_25MHZ		1
#define	ESPI_REG_GEN_CAP_FREQ_35MHZ		2
#define	ESPI_REG_GEN_CAP_FREQ_50MHZ		3
#define	ESPI_REG_GEN_CAP_FREQ_66MHZ		4
#define	ESPI_REG_GEN_CAP_FLASH(r)		bitx32(r, 3, 3)
#define	ESPI_REG_GEN_CAP_OOB(r)			bitx32(r, 2, 2)
#define	ESPI_REG_GEN_CAP_VWIRE(r)		bitx32(r, 1, 1)
#define	ESPI_REG_GEN_CAP_PERIPH(r)		bitx32(r, 0, 0)
/* Channel 0 - Peripheral channel */
#define	ESPI_REG_CHAN0_CAP	0x10
#define	ESPI_REG_CHAN0_CAP_MAXREAD(r)		bitx32(r, 14, 12)
#define	ESPI_REG_CHAN0_CAP_SELPAYLOAD(r)	bitx32(r, 10, 8)
#define	ESPI_REG_CHAN0_CAP_MAXPAYLOAD(r)	bitx32(r, 6, 4)
#define	ESPI_REG_CHAN0_CAP_BUSMASTER_EN(r)	bitx32(r, 2, 2)
#define	ESPI_REG_CHAN0_CAP_READY(r)		bitx32(r, 1, 1)
#define	ESPI_REG_CHAN0_CAP_EN(r)		bitx32(r, 0, 0)
/* Channel 1 - Virtual wire */
#define	ESPI_REG_CHAN1_CAP	0x20
/* Channel 2 - OOB */
#define	ESPI_REG_CHAN2_CAP	0x30
#define	ESPI_REG_CHAN2_CAP_SELPAYLOAD(r)	bitx32(r, 10, 8)
#define	ESPI_REG_CHAN2_CAP_MAXPAYLOAD(r)	bitx32(r, 6, 4)
#define	ESPI_REG_CHAN2_CAP_READY(r)		bitx32(r, 1, 1)
#define	ESPI_REG_CHAN2_CAP_EN(r)		bitx32(r, 0, 0)
/* Channel 3 - Flash */
#define	ESPI_REG_CHAN3_CAP	0x40
#define	ESPI_REG_CHAN3_CAP2	0x44

extern int espi_init(mmio_reg_block_t);
extern uint32_t espi_intstatus(mmio_reg_block_t);
extern int espi_acquire(mmio_reg_block_t);
extern void espi_release(mmio_reg_block_t);
extern bool espi_readable(mmio_reg_block_t);
extern bool espi_writable(mmio_reg_block_t);
extern void espi_flush(mmio_reg_block_t);
extern int espi_tx(mmio_reg_block_t, uint8_t *, size_t *);
extern int espi_rx(mmio_reg_block_t, uint8_t *, size_t *);
extern uint32_t espi_get_configuration(mmio_reg_block_t, uint16_t);

#ifdef __cplusplus
}
#endif

#endif /* _SYS_ESPI_H */
