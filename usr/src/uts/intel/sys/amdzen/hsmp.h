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
 * Copyright 2025 Oxide Computer Company
 */

#ifndef _SYS_AMDZEN_HSMP_H
#define	_SYS_AMDZEN_HSMP_H

#include <sys/bitext.h>
#include <sys/amdzen/smn.h>

/*
 * This header covers the SMN Mailbox Registers for the HSMP (Host System
 * Management Port).
 */

#ifdef __cplusplus
extern "C" {
#endif

#define	SMN_HSMP_APERTURE_MASK	0xffffffffffffff00
AMDZEN_MAKE_SMN_REG_FN(amdzen_smn_hsmp_reg, HSMP, 0x3b10900,
    SMN_HSMP_APERTURE_MASK, 1, 0);

/*
 * HSMP Message ID.
 * The address of the message ID register changed in Turin to something in the
 * same range as the others.
 */
/*CSTYLED*/
#define	D_SMN_HSMP_MSGID	(const smn_reg_def_t){	\
	.srd_unit = SMN_UNIT_HSMP,	\
	.srd_reg = 0x34,		\
}
#define	HSMP_LEGACY_ID_REG	SMN_MAKE_REG(0x3b10534, SMN_UNIT_HSMP)
static inline smn_reg_t
SMN_HSMP_MSGID(x86_processor_family_t fam)
{
	switch (fam) {
	case X86_PF_AMD_MILAN:
	case X86_PF_AMD_GENOA:
	case X86_PF_AMD_BERGAMO:
		return (HSMP_LEGACY_ID_REG);
	default:
		break;
	}
	return (amdzen_smn_hsmp_reg(0, D_SMN_HSMP_MSGID, 0));
}

/*
 * HSMP Response Status.
 */
/*CSTYLED*/
#define	D_SMN_HSMP_RESP	(const smn_reg_def_t){	\
	.srd_unit = SMN_UNIT_HSMP,	\
	.srd_reg = 0x80,		\
}
#define	SMN_HSMP_RESP	\
    amdzen_smn_hsmp_reg(0, D_SMN_HSMP_RESP, 0)

/*
 * HSMP Arguments.
 */
/*CSTYLED*/
#define	D_SMN_HSMP_ARG	(const smn_reg_def_t){	\
	.srd_unit = SMN_UNIT_HSMP,	\
	.srd_reg = 0xe0,		\
	.srd_stride = 0x4,		\
	.srd_nents = 8			\
}
#define	SMN_HSMP_ARG(n)	\
    amdzen_smn_hsmp_reg(0, D_SMN_HSMP_ARG, n)

#ifdef __cplusplus
}
#endif

#endif /* _SYS_AMDZEN_HSMP_H */
