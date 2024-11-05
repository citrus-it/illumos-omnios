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
 * Copyright 2024 Oxide Computer Co.
 */

#ifndef _SYS_IO_FCH_ESPI_H
#define	_SYS_IO_FCH_ESPI_H

/*
 * FCH::ITF::ESPI ...
 */

#ifndef	_ASM
#include <sys/bitext.h>
#include <sys/types.h>
#include <sys/amdzen/smn.h>
#include <sys/amdzen/mmioreg.h>
#endif	/* !_ASM */

#include <sys/amdzen/fch.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * The SPI region is controlled by FCH::LPCPCICFG::SPI_BASE_ADDR, a
 * non-standard BAR in the LPC controller's PCI config space. The reset value
 * of this register is FEC1_0000 and ESPI is always SPI + 0x10_000, with
 * eSPI1 another 0x10_000 beyond that. Note that the terminology in the PPRs is
 * 'ESPI' and 'ESPI1' and we mirror that here.
 */
#define	FCH_SPI_PHYS_BASE		0xfec10000
#define	FCH_ESPI_PHYS_BASE		FCH_SPI_PHYS_BASE + 0x10000
#define	FCH_ESPI1_PHYS_BASE		FCH_SPI_PHYS_BASE + 0x20000

#define	FCH_ESPI_SMN_BASE		0x02dc5000
#define	FCH_ESPI1_SMN_BASE		0x02dca000

#define	FCH_ESPI_SIZE			0x170

/*
 * Not all registers are included here; there are far more in the PPRs.  These
 * are the ones we use or have used in the past. More can be added as
 * required.
 */

#ifndef	_ASM

#if 0
static inline paddr_t
fch_espi_mmio_block(const uint8_t unit)
{
	paddr_t base;

	switch (unit) {
	case 0:
		base = FCH_ESPI_PHYS_BASE;
		break;
	case 1:
		base = FCH_ESPI1_PHYS_BASE;
		break;
	default:
		panic("unreachable code: invalid ESPI unit %u", unit);
	}

	const mmio_reg_block_phys_t phys = {
		.mrbp_base = base,
		.mrbp_size = FCH_ESPI_SIZE
	};

	return (mmio_reg_block_map(SMN_UNIT_FCH_ESPI, phys));
}
#endif

MAKE_MMIO_FCH_REG_BLOCK_FN(ESPI, espi, FCH_ESPI_PHYS_BASE, FCH_ESPI_SIZE);
MAKE_MMIO_FCH_REG_FN(ESPI, espi, 4);
MAKE_SMN_FCH_REG_FN(ESPI, espi, FCH_ESPI_SMN_BASE, FCH_ESPI_SIZE, 4);

/*
 * FCH::ITF::ESPI::DN_TXHDR_0th
 */
#define	FCH_ESPI_DN_TXHDR0		0x0
#define	D_FCH_ESPI_DN_TXHDR0					\
	(const smn_reg_def_t) {					\
		.srd_unit = SMN_UNIT_FCH_ESPI,			\
		.srd_reg = FCH_ESPI_DN_TXHDR0			\
	}
#define	FCH_ESPI_DN_TXHDR0_MMIO(b)		\
    fch_espi_mmio_reg((b), D_FCH_ESPI_DN_TXHDR0, 0)

#define	FCH_ESPI_DN_TXHDR0_GET_HDATA2(r)		bitx32(r, 31, 24)
#define	FCH_ESPI_DN_TXHDR0_SET_HDATA2(r, v)		bitset32(r, 31, 24, v)
#define	FCH_ESPI_DN_TXHDR0_SET_LENL(r, v)		bitset32(r, 31, 24, v)

#define	FCH_ESPI_DN_TXHDR0_GET_HDATA1(r)		bitx32(r, 23, 16)
#define	FCH_ESPI_DN_TXHDR0_SET_HDATA1(r, v)		bitset32(r, 23, 16, v)
#define	FCH_ESPI_DN_TXHDR0_SET_TAG(r, v)		bitset32(r, 23, 20, v)
#define	FCH_ESPI_DN_TXHDR0_SET_LENH(r, v)		bitset32(r, 19, 16, v)

#define	FCH_ESPI_DN_TXHDR0_GET_HDATA0(r)		bitx32(r, 15, 8)
#define	FCH_ESPI_DN_TXHDR0_SET_HDATA0(r, v)		bitset32(r, 15, 8, v)
#define	FCH_ESPI_DN_TXHDR0_SET_CYCLE(r, v)		bitset32(r, 15, 8, v)

#define	FCH_ESPI_DN_TXHDR0_GET_DNCMD_STATUS(r)		bitx32(r, 3, 3)
#define	FCH_ESPI_DN_TXHDR0_SET_DNCMD_STATUS(r, v)	bitset32(r, 3, 3, v)

#define	FCH_ESPI_DN_TXHDR0_GET_DNCMD_TYPE(r)		bitx32(r, 2, 0)
#define	FCH_ESPI_DN_TXHDR0_SET_DNCMD_TYPE(r, v)		bitset32(r, 2, 0, v)
#define	FCH_ESPI_DN_TXHDR0_TYPE_SETCONF			0
#define	FCH_ESPI_DN_TXHDR0_TYPE_GETCONF			1
#define	FCH_ESPI_DN_TXHDR0_TYPE_RESET			2
#define	FCH_ESPI_DN_TXHDR0_TYPE_PERIPH			4
#define	FCH_ESPI_DN_TXHDR0_TYPE_VW			5
#define	FCH_ESPI_DN_TXHDR0_TYPE_OOB			6
#define	FCH_ESPI_DN_TXHDR0_TYPE_FLASH			7

/*
 * FCH::ITF::ESPI::DN_TXHDR_0th - modeled as four separate 8-bit registers as
 * we are sometimes required to do byte writes.
 */
#define	FCH_ESPI_DN_TXHDR0_TYPE			0x0
#define	D_FCH_ESPI_DN_TXHDR0_TYPE				\
	(const smn_reg_def_t) {					\
		.srd_unit = SMN_UNIT_FCH_ESPI,			\
		.srd_reg = FCH_ESPI_DN_TXHDR0_TYPE,		\
		.srd_size = 1					\
	}
#define	FCH_ESPI_DN_TXHDR0_TYPE_MMIO(b)		\
    fch_espi_mmio_reg((b), D_FCH_ESPI_DN_TXHDR0_TYPE, 0)

#define	FCH_ESPI_DN_TXHDR0_HDATA0		0x1
#define	D_FCH_ESPI_DN_TXHDR0_HDATA0				\
	(const smn_reg_def_t) {					\
		.srd_unit = SMN_UNIT_FCH_ESPI,			\
		.srd_reg = FCH_ESPI_DN_TXHDR0_HDATA0,		\
		.srd_size = 1					\
	}
#define	FCH_ESPI_DN_TXHDR0_HDATA0_MMIO(b)		\
    fch_espi_mmio_reg((b), D_FCH_ESPI_DN_TXHDR0_HDATA0, 0)

#define	FCH_ESPI_DN_TXHDR0_HDATA1		0x2
#define	D_FCH_ESPI_DN_TXHDR0_HDATA1				\
	(const smn_reg_def_t) {					\
		.srd_unit = SMN_UNIT_FCH_ESPI,			\
		.srd_reg = FCH_ESPI_DN_TXHDR0_HDATA1,		\
		.srd_size = 1					\
	}
#define	FCH_ESPI_DN_TXHDR0_HDATA1_MMIO(b)		\
    fch_espi_mmio_reg((b), D_FCH_ESPI_DN_TXHDR0_HDATA1, 0)

#define	FCH_ESPI_DN_TXHDR0_HDATA2		0x3
#define	D_FCH_ESPI_DN_TXHDR0_HDATA2				\
	(const smn_reg_def_t) {					\
		.srd_unit = SMN_UNIT_FCH_ESPI,			\
		.srd_reg = FCH_ESPI_DN_TXHDR0_HDATA2,		\
		.srd_size = 1					\
	}
#define	FCH_ESPI_DN_TXHDR0_HDATA2_MMIO(b)		\
    fch_espi_mmio_reg((b), D_FCH_ESPI_DN_TXHDR0_HDATA2, 0)

/*
 * FCH::ITF::ESPI::DN_TXHDR_1
 */
#define	FCH_ESPI_DN_TXHDR1		0x4
#define	D_FCH_ESPI_DN_TXHDR1					\
	(const smn_reg_def_t) {					\
		.srd_unit = SMN_UNIT_FCH_ESPI,			\
		.srd_reg = FCH_ESPI_DN_TXHDR1			\
	}
#define	FCH_ESPI_DN_TXHDR1_MMIO(b)		\
    fch_espi_mmio_reg((b), D_FCH_ESPI_DN_TXHDR1, 0)

#define	FCH_ESPI_DN_TXHDR1_GET_HDATA6(r)		bitx32(r, 31, 24)
#define	FCH_ESPI_DN_TXHDR1_SET_HDATA6(r, v)		bitset32(r, 31, 24, v)
#define	FCH_ESPI_DN_TXHDR1_GET_HDATA5(r)		bitx32(r, 23, 16)
#define	FCH_ESPI_DN_TXHDR1_SET_HDATA5(r, v)		bitset32(r, 23, 16, v)
#define	FCH_ESPI_DN_TXHDR1_GET_HDATA4(r)		bitx32(r, 15, 8)
#define	FCH_ESPI_DN_TXHDR1_SET_HDATA4(r, v)		bitset32(r, 15, 8, v)
#define	FCH_ESPI_DN_TXHDR1_GET_HDATA3(r)		bitx32(r, 7, 0)
#define	FCH_ESPI_DN_TXHDR1_SET_HDATA3(r, v)		bitset32(r, 7, 0, v)

/*
 * FCH::ITF::ESPI::DN_TXHDR_2
 */
#define	FCH_ESPI_DN_TXHDR2		0x8
#define	D_FCH_ESPI_DN_TXHDR2					\
	(const smn_reg_def_t) {					\
		.srd_unit = SMN_UNIT_FCH_ESPI,			\
		.srd_reg = FCH_ESPI_DN_TXHDR2			\
	}
#define	FCH_ESPI_DN_TXHDR2_MMIO(b)		\
    fch_espi_mmio_reg((b), D_FCH_ESPI_DN_TXHDR2, 0)

#define	FCH_ESPI_DN_TXHDR2_GET_HDATA7(r)		bitx32(r, 7, 0)
#define	FCH_ESPI_DN_TXHDR2_SET_HDATA7(r, v)		bitset32(r, 7, 0, v)

/*
 * FCH::ITF::ESPI::DN_TXDATA_PORT
 */
#define	FCH_ESPI_DN_TXDATA_PORT		0xc
#define	D_FCH_ESPI_DN_TXDATA_PORT					\
	(const smn_reg_def_t) {					\
		.srd_unit = SMN_UNIT_FCH_ESPI,			\
		.srd_reg = FCH_ESPI_DN_TXDATA_PORT			\
	}
#define	FCH_ESPI_DN_TXDATA_PORT_MMIO(b)		\
    fch_espi_mmio_reg((b), D_FCH_ESPI_DN_TXDATA_PORT, 0)

/*
 * FCH::ITF::ESPI::UP_RXHDR_0
 */
#define	FCH_ESPI_UP_RXHDR0		0x10
#define	D_FCH_ESPI_UP_RXHDR0				\
	(const smn_reg_def_t) {					\
		.srd_unit = SMN_UNIT_FCH_ESPI,			\
		.srd_reg = FCH_ESPI_UP_RXHDR0		\
	}
#define	FCH_ESPI_UP_RXHDR0_MMIO(b)		\
    fch_espi_mmio_reg((b), D_FCH_ESPI_UP_RXHDR0, 0)
#define	FCH_ESPI_UP_RXHDR0_GET_LENL(v)			bitx32(v, 31, 24)
#define	FCH_ESPI_UP_RXHDR0_GET_TAG(v)			bitx32(v, 23, 20)
#define	FCH_ESPI_UP_RXHDR0_GET_LENH(v)			bitx32(v, 19, 16)
#define	FCH_ESPI_UP_RXHDR0_GET_CYCLE(v)			bitx32(v, 15, 8)
#define	FCH_ESPI_UP_RXHDR0_GET_UPCMD_STAT(v)		bitx32(v, 3, 3)
#define	FCH_ESPI_UP_RXHDR0_CLEAR_UPCMD_STAT(v)		bitset32(v, 3, 3, 1)
#define	FCH_ESPI_UP_RXHDR0_GET_UPCMD_TYPE(v)		bitx32(v, 2, 0)
#define	FCH_ESPI_UP_RXHDR0_GET_UPCMD_TYPE_OOB	1

/*
 * FCH::ITF::ESPI::UP_RXHDR_1
 */
#define	FCH_ESPI_UP_RXHDR1		0x14
#define	D_FCH_ESPI_UP_RXHDR1				\
	(const smn_reg_def_t) {					\
		.srd_unit = SMN_UNIT_FCH_ESPI,			\
		.srd_reg = FCH_ESPI_UP_RXHDR1		\
	}
#define	FCH_ESPI_UP_RXHDR1_MMIO(b)		\
    fch_espi_mmio_reg((b), D_FCH_ESPI_UP_RXHDR1, 0)
#define	FCH_ESPI_UP_RXHDR1_GET_HDATA6(v)		bitx32(v, 31, 24)
#define	FCH_ESPI_UP_RXHDR1_GET_HDATA5(v)		bitx32(v, 23, 16)
#define	FCH_ESPI_UP_RXHDR1_GET_HDATA4(v)		bitx32(v, 15, 8)
#define	FCH_ESPI_UP_RXHDR1_GET_HDATA3(v)		bitx32(v, 7, 0)

/*
 * FCH::ITF::ESPI::UP_RXDATA_PORT
 */
#define	FCH_ESPI_UP_RXDATA_PORT	0x18
#define	D_FCH_ESPI_UP_RXDATA_PORT				\
	(const smn_reg_def_t) {					\
		.srd_unit = SMN_UNIT_FCH_ESPI,			\
		.srd_reg = FCH_ESPI_UP_RXDATA_PORT		\
	}
#define	FCH_ESPI_UP_RXDATA_PORT_MMIO(b)		\
    fch_espi_mmio_reg((b), D_FCH_ESPI_UP_RXDATA_PORT, 0)

/*
 * FCH::ITF::ESPI::RESERVED_REG0
 */
#define	FCH_ESPI_RESERVED_REG0		0x1c
#define	D_FCH_ESPI_RESERVED_REG0				\
	(const smn_reg_def_t) {					\
		.srd_unit = SMN_UNIT_FCH_ESPI,			\
		.srd_reg = FCH_ESPI_RESERVED_REG0		\
	}
#define	FCH_ESPI_RESERVED_REG0_MMIO(b)		\
    fch_espi_mmio_reg((b), D_FCH_ESPI_RESERVED_REG0, 0)

#define	FCH_ESPI_RESERVED_REG0_INIT_STAT(r)		bitx32(r, 19, 16)
#define	FCH_ESPI_RESERVED_REG0_INIT_STAT_SUCCESS	7

/*
 * FCH::ITF::ESPI::ESPI_MISC_CONTROL_REG0
 */
#define	FCH_ESPI_MISC_CTL0		0x20
#define	D_FCH_ESPI_MISC_CTL0				\
	(const smn_reg_def_t) {					\
		.srd_unit = SMN_UNIT_FCH_ESPI,			\
		.srd_reg = FCH_ESPI_MISC_CTL0		\
	}
#define	FCH_ESPI_MISC_CTL0_MMIO(b)		\
    fch_espi_mmio_reg((b), D_FCH_ESPI_MISC_CTL0, 0)

#define	FCH_ESPI_MISC_CTL0_OOB_AVAIL(r)			bitx32(r, 23, 23)
#define	FCH_ESPI_MISC_CTL0_OOB_FREE(r)			bitx32(r, 19, 19)

/*
 * FCH::ITF::ESPI::MASTER_CAP
 */
#define	FCH_ESPI_MASTER_CAP		0x2c
#define	D_FCH_ESPI_MASTER_CAP				\
	(const smn_reg_def_t) {					\
		.srd_unit = SMN_UNIT_FCH_ESPI,			\
		.srd_reg = FCH_ESPI_MASTER_CAP		\
	}
#define	FCH_ESPI_MASTER_CAP_MMIO(b)		\
    fch_espi_mmio_reg((b), D_FCH_ESPI_MASTER_CAP, 0)

#define	FCH_ESPI_MASTER_CAP_PR_MAXSZ(r)			bitx32(r, 21, 19)

/*
 * FCH::ITF::ESPI::SEMAPHORE_MISC_CONTROL_REG0. Semaphore register used to
 * co-ordinate access authority.
 */
#define	FCH_ESPI_SEM_MISC_CTL_REG0	0x38
#define	D_FCH_ESPI_SEM_MISC_CTL_REG0				\
	(const smn_reg_def_t) {					\
		.srd_unit = SMN_UNIT_FCH_ESPI,			\
		.srd_reg = FCH_ESPI_SEM_MISC_CTL_REG0		\
	}
#define	FCH_ESPI_SEM_MISC_CTL_REG0_MMIO(b)		\
    fch_espi_mmio_reg((b), D_FCH_ESPI_SEM_MISC_CTL_REG0, 0)

#define	FCH_ESPI_SEM_MISC_CTL_REG0_GET_SW2_OWN_CLR(r)	 bitx32(r, 26, 26)
#define	FCH_ESPI_SEM_MISC_CTL_REG0_SET_SW2_OWN_CLR(r, v) bitset32(r, 26, 26, v)
#define	FCH_ESPI_SEM_MISC_CTL_REG0_GET_SW2_OWN_SET(r)	 bitx32(r, 25, 25)
#define	FCH_ESPI_SEM_MISC_CTL_REG0_SET_SW2_OWN_SET(r, v) bitset32(r, 25, 25, v)
#define	FCH_ESPI_SEM_MISC_CTL_REG0_GET_SW3_OWN_STAT(r)	 bitx32(r, 28, 28)
#define	FCH_ESPI_SEM_MISC_CTL_REG0_GET_SW2_OWN_STAT(r)	 bitx32(r, 24, 24)
#define	FCH_ESPI_SEM_MISC_CTL_REG0_GET_SW1_OWN_STAT(r)	 bitx32(r, 20, 20)
#define	FCH_ESPI_SEM_MISC_CTL_REG0_GET_SW0_OWN_STAT(r)	 bitx32(r, 16, 16)
#define	FCH_ESPI_SEM_MISC_CTL_REG0_GET_SW4_USER_ID(r)	 bitx32(r, 15, 8)

/*
 * FCH::ITF::ESPI::SLAVE0_INT_STS.
 */
#define	FCH_ESPI_S0_INT_STS		0x70
#define	D_FCH_ESPI_S0_INT_STS				\
	(const smn_reg_def_t) {					\
		.srd_unit = SMN_UNIT_FCH_ESPI,			\
		.srd_reg = FCH_ESPI_S0_INT_STS		\
	}
#define	FCH_ESPI_S0_INT_STS_MMIO(b)		\
    fch_espi_mmio_reg((b), D_FCH_ESPI_S0_INT_STS, 0)

#define	FCH_ESPI_S0_INT_STS_GET_RXOOB(r)		bitx32(r, 30, 30)
#define	FCH_ESPI_S0_INT_STS_CLEAR_RXOOB(r)		bitset32(r, 30, 30, 1)
#define	FCH_ESPI_S0_INT_STS_GET_RXMSG(r)		bitx32(r, 29, 29)
#define	FCH_ESPI_S0_INT_STS_CLEAR_RXMSG(r)		bitset32(r, 29, 29, 1)
#define	FCH_ESPI_S0_INT_STS_GET_DNCMD(r)		bitx32(r, 28, 28)
#define	FCH_ESPI_S0_INT_STS_CLEAR_DNCMD(r)		bitset32(r, 28, 28, 1)
#define	FCH_ESPI_S0_INT_STS_GET_PROTOERR(r)		bitx32(r, 15, 15)
#define	FCH_ESPI_S0_INT_STS_CLEAR_PROTOERR(r)		bitset32(r, 15, 15, 1)
#define	FCH_ESPI_S0_INT_STS_GET_RXMSG_OFLOW(r)		bitx32(r, 13, 13)
#define	FCH_ESPI_S0_INT_STS_CLEAR_RXMSG_OFLOW(r)	bitset32(r, 13, 13, 1)
#define	FCH_ESPI_S0_INT_STS_GET_ILL_LEN(r)		bitx32(r, 11, 11)
#define	FCH_ESPI_S0_INT_STS_CLEAR_ILL_LEN(r)		bitset32(r, 11, 11, 1)
#define	FCH_ESPI_S0_INT_STS_GET_ILL_TAG(r)		bitx32(r, 10, 10)
#define	FCH_ESPI_S0_INT_STS_CLEAR_ILL_TAG(r)		bitset32(r, 10, 10, 1)
#define	FCH_ESPI_S0_INT_STS_GET_USF_CPL(r)		bitx32(r, 9, 9)
#define	FCH_ESPI_S0_INT_STS_CLEAR_USF_CPL(r)		bitset32(r, 9, 9, 1)
#define	FCH_ESPI_S0_INT_STS_GET_UNK_CYC(r)		bitx32(r, 8, 8)
#define	FCH_ESPI_S0_INT_STS_CLEAR_UNK_CYC(r)		bitset32(r, 8, 8, 1)
#define	FCH_ESPI_S0_INT_STS_GET_UNK_RSP(r)		bitx32(r, 7, 7)
#define	FCH_ESPI_S0_INT_STS_CLEAR_UNK_RSP(r)		bitset32(r, 7, 7, 1)
#define	FCH_ESPI_S0_INT_STS_GET_NFATAL_ERR(r)		bitx32(r, 6, 6)
#define	FCH_ESPI_S0_INT_STS_CLEAR_NFATAL_ERR(r)		bitset32(r, 6, 6, 1)
#define	FCH_ESPI_S0_INT_STS_GET_FATAL_ERR(r)		bitx32(r, 5, 5)
#define	FCH_ESPI_S0_INT_STS_CLEAR_FATAL_ERR(r)		bitset32(r, 5, 5, 1)
#define	FCH_ESPI_S0_INT_STS_GET_NO_RSP(r)		bitx32(r, 4, 4)
#define	FCH_ESPI_S0_INT_STS_CLEAR_NO_RSP(r)		bitset32(r, 4, 4, 1)
#define	FCH_ESPI_S0_INT_STS_GET_CRC_ERR(r)		bitx32(r, 2, 2)
#define	FCH_ESPI_S0_INT_STS_CLEAR_CRC_ERR(r)		bitset32(r, 2, 2, 1)
#define	FCH_ESPI_S0_INT_STS_GET_WAIT_TMT(r)		bitx32(r, 1, 1)
#define	FCH_ESPI_S0_INT_STS_CLEAR_WAIT_TMT(r)		bitset32(r, 1, 1, 1)
#define	FCH_ESPI_S0_INT_STS_GET_BUS_ERR(r)		bitx32(r, 0, 0)
#define	FCH_ESPI_S0_INT_STS_CLEAR_BUS_ERR(r)		bitset32(r, 0, 0, 1)

/*
 * FCH::ITF::ESPI::SLAVE0_RXMSG_HDR0.
 */
#define	FCH_ESPI_S0_RXMSG_HDR0	0x74
#define	D_FCH_ESPI_S0_RXMSG_HDR0				\
	(const smn_reg_def_t) {					\
		.srd_unit = SMN_UNIT_FCH_ESPI,			\
		.srd_reg = FCH_ESPI_S0_RXMSG_HDR0		\
	}
#define	FCH_ESPI_S0_RXMSG_HDR0_MMIO(b)		\
    fch_espi_mmio_reg((b), D_FCH_ESPI_S0_RXMSG_HDR0, 0)

#define	FCH_ESPI_S0_RXMSG_HDR0_CODE(v)			bitx32(v, 31, 24)
#define	FCH_ESPI_S0_RXMSG_HDR0_LENL(v)			bitx32(v, 23, 16)
#define	FCH_ESPI_S0_RXMSG_HDR0_TAG(v)			bitx32(v, 15, 12)
#define	FCH_ESPI_S0_RXMSG_HDR0_LENH(v)			bitx32(v, 11, 8)
#define	FCH_ESPI_S0_RXMSG_HDR0_CYCLE(v)			bitx32(v, 7, 0)

/*
 * FCH::ITF::ESPI::SLAVE0_RXMSG_HDR1.
 */
#define	FCH_ESPI_S0_RXMSG_HDR1	0x78
#define	D_FCH_ESPI_S0_RXMSG_HDR1				\
	(const smn_reg_def_t) {					\
		.srd_unit = SMN_UNIT_FCH_ESPI,			\
		.srd_reg = FCH_ESPI_S0_RXMSG_HDR1		\
	}
#define	FCH_ESPI_S0_RXMSG_HDR1_MMIO(b)		\
    fch_espi_mmio_reg((b), D_FCH_ESPI_S0_RXMSG_HDR1, 0)

/*
 * FCH::ITF::ESPI::SLAVE0_RXMSG_DATA_PORT.
 */
#define	FCH_ESPI_S0_RXMSG_DATA_PORT	0x7c
#define	D_FCH_ESPI_S0_RXMSG_DATA_PORT				\
	(const smn_reg_def_t) {					\
		.srd_unit = SMN_UNIT_FCH_ESPI,			\
		.srd_reg = FCH_ESPI_S0_RXMSG_DATA_PORT		\
	}
#define	FCH_ESPI_S0_RXMSG_DATA_PORT_MMIO(b)		\
    fch_espi_mmio_reg((b), D_FCH_ESPI_S0_RXMSG_DATA_PORT, 0)

#endif	/* !_ASM */

#ifdef __cplusplus
}
#endif

#endif /* _SYS_IO_FCH_ESPI_H */
