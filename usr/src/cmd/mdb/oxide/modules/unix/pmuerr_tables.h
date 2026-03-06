/*
 * This file and its contents are supplied under the terms of the
 * Common Development and Distribution License ("CDDL"), version 1.0.
 * You may only use this file in accordance with the terms of version
 * 1.0 of the CDDL.
 *
 * A full copy of the text of the CDDL should have accompanied this
 * source. A copy of the CDDL is also available via the Internet at
 * http://www.illumos.org/license/CDDL.
 */

/*
 * Copyright 2026 Oxide Computer Company
 */

#ifndef _PMUERR_TABLES_H
#define	_PMUERR_TABLES_H

#include <sys/types.h>
#include <sys/x86_archext.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Training phase flags. An entry may apply to training, post-training, or
 * both.
 */
#define	PMUERR_PH_TRAIN		(1U << 0)
#define	PMUERR_PH_POST		(1U << 1)
#define	PMUERR_PH_BOTH		(PMUERR_PH_TRAIN | PMUERR_PH_POST)

typedef struct pmuerr_entry {
	uint32_t pme_phases;	/* bitmask of PMUERR_PH_* */
	uint32_t pme_code;	/* full 32-bit error code (MSG_ID | NARGS) */
	const char *pme_msg;	/* PMU message string */
} pmuerr_entry_t;

/*
 * A PMU error table is specific to a (platform, SMU firmware version)
 * combination. The SMU version serves as a proxy for the PMU firmware version.
 * Tables are grouped by processor family; within a family, the table with the
 * highest SMU version that does not exceed the system's version is selected.
 * If no table's version matches, the latest available table is used and a
 * warning is emitted.
 */
typedef struct pmuerr_table {
	x86_processor_family_t pmt_family;
	uint8_t pmt_smu_maj;
	uint8_t pmt_smu_min;
	const char *pmt_desc;
	const pmuerr_entry_t *pmt_entries;
	size_t pmt_nentries;
} pmuerr_table_t;

extern void pmuerr_init(x86_processor_family_t, const uint32_t [3]);
extern void pmuerr_init_family(x86_processor_family_t);
extern const pmuerr_entry_t *pmuerr_lookup(uint32_t, uint32_t);

#ifdef __cplusplus
}
#endif

#endif /* _PMUERR_TABLES_H */
