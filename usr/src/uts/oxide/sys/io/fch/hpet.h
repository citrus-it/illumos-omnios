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

#ifndef _SYS_IO_FCH_HPET_H
#define	_SYS_IO_FCH_HPET_H

/*
 * FCH::TMR::HPET is a standard IA-PC HPET at a fixed physical address. Its
 * decoding is controlled by FCH::PM::DECODEEN[hpeten]. We access it only via
 * MMIO and only with 32-bit accesses, which every supported FCH permits
 * regardless of FCH::PM::DECODEEN[hpetwidthsel].
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

#define	FCH_HPET_PHYS_BASE	0xfed00000
#define	FCH_HPET_SIZE		0x400

#ifndef	_ASM

MAKE_MMIO_FCH_REG_BLOCK_FN(HPET, hpet, FCH_HPET_PHYS_BASE, FCH_HPET_SIZE);
MAKE_MMIO_FCH_REG_FN(HPET, hpet, 4);

/*
 * FCH::TMR::HPET::GCAP_ID, the low half of the general capabilities and ID
 * register.
 */
/*CSTYLED*/
#define	D_FCH_HPET_GCAP_ID	(const smn_reg_def_t){	\
	.srd_unit = SMN_UNIT_FCH_HPET,	\
	.srd_reg = 0x00	\
}
#define	FCH_HPET_GCAP_ID_MMIO(b)	\
    fch_hpet_mmio_reg((b), D_FCH_HPET_GCAP_ID, 0)
#define	FCH_HPET_GCAP_ID_GET_VENDOR(r)		bitx32(r, 31, 16)
#define	FCH_HPET_GCAP_ID_GET_LEGRTCAP(r)	bitx32(r, 15, 15)
#define	FCH_HPET_GCAP_ID_GET_COUNTSIZECAP(r)	bitx32(r, 13, 13)
#define	FCH_HPET_GCAP_ID_GET_NUMTIMCAP(r)	bitx32(r, 12, 8)
#define	FCH_HPET_GCAP_ID_GET_REVID(r)		bitx32(r, 7, 0)

/*
 * FCH::TMR::HPET::CLK_PERIOD, the high half of the general capabilities and
 * ID register. This is the period of the main counter in femtoseconds.
 */
/*CSTYLED*/
#define	D_FCH_HPET_CLK_PERIOD	(const smn_reg_def_t){	\
	.srd_unit = SMN_UNIT_FCH_HPET,	\
	.srd_reg = 0x04	\
}
#define	FCH_HPET_CLK_PERIOD_MMIO(b)	\
    fch_hpet_mmio_reg((b), D_FCH_HPET_CLK_PERIOD, 0)

/*
 * The HPET specification requires a period no greater than 100ns.
 */
#define	FCH_HPET_CLK_PERIOD_MAX_FS	100000000U

/*
 * FCH::TMR::HPET::GEN_CONF
 */
/*CSTYLED*/
#define	D_FCH_HPET_GEN_CONF	(const smn_reg_def_t){	\
	.srd_unit = SMN_UNIT_FCH_HPET,	\
	.srd_reg = 0x10	\
}
#define	FCH_HPET_GEN_CONF_MMIO(b)	\
    fch_hpet_mmio_reg((b), D_FCH_HPET_GEN_CONF, 0)
#define	FCH_HPET_GEN_CONF_GET_LEG_RT_CNF(r)	bitx32(r, 1, 1)
#define	FCH_HPET_GEN_CONF_SET_LEG_RT_CNF(r, v)	bitset32(r, 1, 1, v)
#define	FCH_HPET_GEN_CONF_GET_ENABLE_CNF(r)	bitx32(r, 0, 0)
#define	FCH_HPET_GEN_CONF_SET_ENABLE_CNF(r, v)	bitset32(r, 0, 0, v)

/*
 * FCH::TMR::HPET::MAIN_COUNTER, the low 32 bits of the main counter.
 */
/*CSTYLED*/
#define	D_FCH_HPET_MAIN_COUNTER	(const smn_reg_def_t){	\
	.srd_unit = SMN_UNIT_FCH_HPET,	\
	.srd_reg = 0xf0	\
}
#define	FCH_HPET_MAIN_COUNTER_MMIO(b)	\
    fch_hpet_mmio_reg((b), D_FCH_HPET_MAIN_COUNTER, 0)

#endif	/* !_ASM */

#ifdef __cplusplus
}
#endif

#endif /* _SYS_IO_FCH_HPET_H */
