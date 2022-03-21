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
 * Copyright 2019 Joyent, Inc.
 * Copyright 2021 OmniOS Community Edition (OmniOSce) Association.
 */

/*
 * The code here is derived from, and should be kept in sync with, the kernel
 * hma implementation in uts/i86pc/os/hma.c and the checks in
 * usr/src/uts/i86pc/io/vmm/intel/vmx.c
 */

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <err.h>

#include <sys/vmm.h>
#include <sys/cpuid_drv.h>
#define	_KERNEL
#include <sys/x86_archext.h>
#undef _KERNEL
#include <sys/controlregs.h>
#include "vmx_controls.h"

#define	VMX_CTL_ONE_SETTING(val, flag)  \
	(((val) & ((uint64_t)(flag) << 32)) != 0)

boolean_t g_status = B_FALSE;
int g_fd;

uint64_t
rdmsr(uint_t msr)
{
	struct cpuid_rdmsr crm;

	crm.cr_msr_nr = msr;

	if (ioctl(g_fd, CPUID_RDMSR, &crm) != 0) {
		if (errno == EPERM)
			errx(EXIT_FAILURE, "Permission denied.");
		err(EXIT_FAILURE, "rdmsr(%#lx) failed", crm.cr_msr_nr);
	}

	return (crm.cr_msr_val);
}

struct cpuid_regs *
cpuid(uint64_t leaf)
{
	static struct cpuid_regs crs;

	if (pread(g_fd, &crs, sizeof (crs), (off_t)leaf) != sizeof (crs))
		err(EXIT_FAILURE, "cpuid(%#lx) read failed", leaf);

	return (&crs);
}

void
note(char *fmt, ...)
{
	va_list ap;

	if (g_status)
		return;

	printf("... ");
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
	printf("\n");
}

static inline boolean_t
check_onectl(uint64_t msr, boolean_t required, uint32_t bit, char *descr)
{
	boolean_t ret = VMX_CTL_ONE_SETTING(msr, bit);

	if (ret) {
		note("VMX supports %s", descr);
	} else {
		note("VMX does not support %s (%s)", descr,
		    required ? "essential" : "optional");
	}
	return (ret);
}

#define REQUIRE(msr, req, bit, descr) \
	if (!check_onectl((msr), (req), (bit), (descr)) && (req)) \
		ret = B_FALSE

boolean_t
vmx_check(void)
{
	struct cpuid_regs *cp;
	uint64_t msr;
	boolean_t ret = B_TRUE;

	cp = cpuid(1);
	if ((cp->cp_ecx & CPUID_INTC_ECX_VMX) == 0) {
		note("CPU does not support VMX");
		return (B_FALSE);
	}
	note("CPU supports VMX");

	msr = rdmsr(MSR_IA32_FEAT_CTRL);
	if ((msr & IA32_FEAT_CTRL_LOCK) != 0 &&
	    (msr & IA32_FEAT_CTRL_VMX_EN) == 0) {
		note("VMX support not enabled in BIOS (essential)");
		return (B_FALSE);
	}
	note("VMX support is enabled in BIOS");

	/* The basic INS/OUTS functionality is cited as a necessary prereq */
	msr = rdmsr(MSR_IA32_VMX_BASIC);
	if ((msr & IA32_VMX_BASIC_INS_OUTS) == 0) {
		note("VMX does not support INS/OUTS (essential)");
		ret = B_FALSE;
	}

	boolean_t query_true_ctl = B_FALSE;
	boolean_t ept = B_FALSE;

	/*
	 * Bit 55 in the VMX_BASIC MSR determines how VMX control information
	 * can be queried.
	 */
	query_true_ctl = (msr & IA32_VMX_BASIC_TRUE_CTRLS) != 0;

	msr = rdmsr(query_true_ctl ?
	    MSR_IA32_VMX_TRUE_PROCBASED_CTLS : MSR_IA32_VMX_PROCBASED_CTLS);

	REQUIRE(msr, B_TRUE,
	    PROCBASED_TSC_OFFSET, "TSC Offsetting");
	REQUIRE(msr, B_TRUE,
	    PROCBASED_MWAIT_EXITING, "VM Exit on MWAIT");
	REQUIRE(msr, B_TRUE,
	    PROCBASED_MONITOR_EXITING, "VM Exit on MONITOR");
	REQUIRE(msr, B_TRUE,
	    PROCBASED_CR8_LOAD_EXITING, "VM Exit on CR8 Load");
	REQUIRE(msr, B_TRUE,
	    PROCBASED_CR8_STORE_EXITING, "VM Exit on CR8 Store");
	REQUIRE(msr, B_TRUE,
	    PROCBASED_IO_EXITING, "Unconditional I/O exiting");
	REQUIRE(msr, B_TRUE,
	    PROCBASED_MSR_BITMAPS, "MSR bitmap");
	REQUIRE(msr, B_TRUE,
	    PROCBASED_INT_WINDOW_EXITING, "Interrupt-window exiting");
	REQUIRE(msr, B_TRUE,
	    PROCBASED_NMI_WINDOW_EXITING, "NMI-window exiting");

	/* Check for EPT and VPID support */
	if (check_onectl(msr, B_TRUE, IA32_VMX_PROCBASED_2ND_CTLS,
	    "Secondary VMX controls")) {
		msr = rdmsr(MSR_IA32_VMX_PROCBASED2_CTLS);
		if (check_onectl(msr, B_FALSE, IA32_VMX_PROCBASED2_EPT, "EPT"))
			ept = B_TRUE;
		(void) check_onectl(msr, B_FALSE,
		    IA32_VMX_PROCBASED2_VPID, "VPID");

		if (!check_onectl(msr, B_TRUE,
		    PROCBASED2_UNRESTRICTED_GUEST, "Unrestricted Guest")) {
			ret = B_FALSE;
		}
	} else {
		ret = B_FALSE;
	}

	/* Check for INVEPT support */
	if (ept) {
		msr = rdmsr(MSR_IA32_VMX_EPT_VPID_CAP);
		if ((msr & IA32_VMX_EPT_VPID_INVEPT) != 0) {
			if ((msr & IA32_VMX_EPT_VPID_INVEPT_SINGLE) != 0)
				note("VMX supports single INVEPT");
			if ((msr & IA32_VMX_EPT_VPID_INVEPT_ALL) != 0)
				note("VMX supports all INVEPT");
		}
	}

	return (ret);
}

int
svm_check(void)
{
	struct cpuid_regs *cp;
	uint64_t msr;

	cp = cpuid(0x80000001);
	if ((cp->cp_ecx & CPUID_AMD_ECX_SVM) == 0) {
		note("CPU does not support SVM");
		return (B_FALSE);
	}
	note("CPU supports SVM");

	msr = rdmsr(MSR_AMD_VM_CR);
	if ((msr & AMD_VM_CR_SVMDIS) != 0) {
		note("SVM support not enabled in BIOS (essential)");
		return (B_FALSE);
	}
	note("SVM support is enabled in BIOS");

	cp = cpuid(0x8000000a);
	const uint32_t nasid = cp->cp_ebx;
	const uint32_t feat = cp->cp_edx;

	if (nasid == 0) {
		note("Not enough ASIDs for guests (essential)");
		return (B_FALSE);
	}
	if ((feat & CPUID_AMD_EDX_NESTED_PAGING) == 0) {
		note("CPU does not support nested paging (essential)");
		return (B_FALSE);
	}
	if ((feat & CPUID_AMD_EDX_NRIPS) == 0) {
		note("CPU does not support NRIP save (essential)");
		return (B_FALSE);
	}

	return (B_TRUE);
}

int
main(int argc, char **argv)
{
	struct cpuid_regs *cp;
	boolean_t support = B_FALSE;
	int errflg = 0;
	int c;

	while ((c = getopt(argc, argv, "cs")) != EOF) {
		switch (c) {
		case 'c': {
			uint16_t sockets, cores, threads, maxcpus;

			error = vm_get_topology(ctx, &sockets, &cores,
			    &threads, &maxcpus);
			(void) printf("%u\n", error == 0 ? maxcpus : 1);

			return (EXIT_SUCCESS);
		}
		case 's':
			g_status = B_TRUE;
			break;
		case '?':
		default:
			errflg++;
			break;
		}
	}

	if (errflg != 0 || optind > argc)
		errx(EXIT_FAILURE, "Usage: bhhwcompat [-cs]");

	if ((g_fd = open("/dev/" CPUID_SELF_NAME, O_RDONLY)) == -1)
		err(EXIT_FAILURE, "Failed to open /dev/%s", CPUID_SELF_NAME);

	/* Retrieve the CPU vendor name */
	uint32_t regs[4];
	char *hvstr = (char *)regs;

	cp = cpuid(0);
	regs[0] = cp->cp_ebx;
	regs[1] = cp->cp_edx;
	regs[2] = cp->cp_ecx;
	regs[3] = 0;

	if (!g_status)
		printf("CPU vendor string: %s\n", hvstr);

	if (strcmp(hvstr, X86_VENDORSTR_Intel) == 0) {
		support = vmx_check();
	} else if (strcmp(hvstr, X86_VENDORSTR_AMD) == 0 ||
	    strcmp(hvstr, X86_VENDORSTR_HYGON) == 0) {
		support = svm_check();
	} else {
		errx(EXIT_FAILURE, "Unhandled CPU vendor, %s", hvstr);
	}

	(void) close(g_fd);

	if (!g_status) {
		printf("\nbhyve is %ssupported on this system.\n",
		    support ? "" : "NOT ");
	}

	if (support)
		return (0);

	return (2);	/* Unsupported hardware */
}
