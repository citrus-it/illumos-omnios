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
 * Copyright 2026 OmniOS Community Edition (OmniOSce) Association.
 */

/*
 * TSC calibration for Xen HVM guests, using the TSC frequency that the
 * hypervisor reports through CPUID.
 */

#include <sys/systm.h>
#include <sys/x86_archext.h>
#include <sys/tsc.h>

/*
 * Xen's CPUID leaves start at the first multiple of 0x100 from 0x40000000
 * that carries its signature. When Xen also presents another hypervisor's
 * interface, such as Hyper-V, that interface occupies 0x40000000 and the Xen
 * leaves are found further up.
 */
#define	XEN_CPUID_BASE_FIRST	0x40000000
#define	XEN_CPUID_BASE_LAST	0x40010000
#define	XEN_CPUID_BASE_STEP	0x100

/*
 * Offset from the base of the leaf that describes the guest's time sources.
 * Sub-leaf 0 of this leaf reports the TSC frequency in kHz in ECX.
 */
#define	XEN_CPUID_LEAF_TIME	3

static boolean_t
tsc_calibrate_xen(uint64_t *freqp)
{
	/*
	 * Don't require HW_XEN_HVM here. When Xen also presents the Hyper-V
	 * interface, determine_platform() finds that first and reports
	 * HW_MICROSOFT, but the Xen leaves are still present further up.
	 */
	if (get_hwenv() == HW_NATIVE)
		return (B_FALSE);

	for (uint32_t base = XEN_CPUID_BASE_FIRST; base < XEN_CPUID_BASE_LAST;
	    base += XEN_CPUID_BASE_STEP) {
		struct cpuid_regs regs = { .cp_eax = base };
		uint32_t sig[4];

		(void) __cpuid_insn(&regs);
		sig[0] = regs.cp_ebx;
		sig[1] = regs.cp_ecx;
		sig[2] = regs.cp_edx;
		sig[3] = 0;
		if (strcmp((const char *)sig, HVSIG_XEN_HVM) != 0)
			continue;

		/* EAX holds the highest leaf that this hypervisor supports */
		if (regs.cp_eax < base + XEN_CPUID_LEAF_TIME)
			return (B_FALSE);

		struct cpuid_regs tregs = {
			.cp_eax = base + XEN_CPUID_LEAF_TIME,
			.cp_ecx = 0
		};

		(void) __cpuid_insn(&tregs);

		/* A value of 0 means that no frequency was supplied */
		if (tregs.cp_ecx == 0)
			return (B_FALSE);

		*freqp = (uint64_t)tregs.cp_ecx * 1000;
		return (B_TRUE);
	}

	return (B_FALSE);
}

static tsc_calibrate_t tsc_calibration_xen = {
	.tscc_source = "Xen",
	.tscc_preference = 100,
	.tscc_calibrate = tsc_calibrate_xen,
};
TSC_CALIBRATION_SOURCE(tsc_calibration_xen);
