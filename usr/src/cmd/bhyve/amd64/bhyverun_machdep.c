/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2011 NetApp, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY NETAPP, INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL NETAPP, INC OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/*
 * This file and its contents are supplied under the terms of the
 * Common Development and Distribution License ("CDDL"), version 1.0.
 * You may only use this file in accordance with the terms of version
 * 1.0 of the CDDL.
 *
 * A full copy of the text of the CDDL should have accompanied this
 * source.  A copy of the CDDL is also available via the Internet at
 * http://www.illumos.org/license/CDDL.
 *
 * Copyright 2015 Pluribus Networks Inc.
 * Copyright 2018 Joyent, Inc.
 * Copyright 2022 Oxide Computer Company
 * Copyright 2022 OmniOS Community Edition (OmniOSce) Association.
 */

#include <sys/types.h>
#include <machine/vmm.h>

#include <assert.h>
#include <err.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/vmm.h>
#include <vmmapi.h>

#include "bhyverun.h"
#include "acpi.h"
#include "atkbdc.h"
#include "config.h"
#include "e820.h"
#include "fwctl.h"
#include "ioapic.h"
#include "inout.h"
#ifndef	__FreeBSD__
#include "kernemu_dev.h"
#endif
#include "mptbl.h"
#include "pci_irq.h"
#include "spinup_ap.h"
#include "pci_lpc.h"
#include "rtc.h"
#include "smbiostbl.h"
#include "xmsr.h"

void
bhyve_init_config(void)
{
	init_config();

	/* Set default values prior to option parsing. */
	set_config_bool("acpi_tables", false);
	set_config_bool("acpi_tables_in_memory", true);
	set_config_value("memory.size", "256M");
	set_config_bool("x86.strictmsr", true);
	set_config_value("lpc.fwcfg", "bhyve");
}

void
bhyve_init_vcpu(struct vcpu *vcpu)
{
	int err, tmp;

#ifdef	__FreeBSD__
	if (get_config_bool_default("x86.vmexit_on_hlt", false)) {
		err = vm_get_capability(vcpu, VM_CAP_HALT_EXIT, &tmp);
		if (err < 0) {
			fprintf(stderr, "VM exit on HLT not supported\n");
			exit(4);
		}
		vm_set_capability(vcpu, VM_CAP_HALT_EXIT, 1);
	}
#else
	/*
	 * We insist that vmexit-on-hlt is available on the host CPU, and enable
	 * it by default.  Configuration of that feature is done with both of
	 * those facts in mind.
	 */
	tmp = (int)get_config_bool_default("x86.vmexit_on_hlt", true);
	err = vm_set_capability(vcpu, VM_CAP_HALT_EXIT, tmp);
	if (err < 0) {
		fprintf(stderr, "VM exit on HLT not supported\n");
		exit(4);
	}
#endif /* __FreeBSD__ */

	if (get_config_bool_default("x86.vmexit_on_pause", false)) {
		/*
		 * pause exit support required for this mode
		 */
		err = vm_get_capability(vcpu, VM_CAP_PAUSE_EXIT, &tmp);
		if (err < 0) {
			fprintf(stderr,
			    "SMP mux requested, no pause support\n");
			exit(4);
		}
		vm_set_capability(vcpu, VM_CAP_PAUSE_EXIT, 1);
	}

	if (get_config_bool_default("x86.x2apic", false))
		err = vm_set_x2apic_state(vcpu, X2APIC_ENABLED);
	else
		err = vm_set_x2apic_state(vcpu, X2APIC_DISABLED);

	if (err) {
		fprintf(stderr, "Unable to set x2apic state (%d)\n", err);
		exit(4);
	}

#ifdef	__FreeBSD__
	vm_set_capability(vcpu, VM_CAP_ENABLE_INVPCID, 1);

	err = vm_set_capability(vcpu, VM_CAP_IPI_EXIT, 1);
	assert(err == 0);
#endif
}

void
bhyve_start_vcpu(struct vcpu *vcpu, bool bsp, bool suspend)
{
	int error;

	if (!bsp) {
#ifndef	__FreeBSD__
		/*
		 * On illumos, all APs are spun up halted and run-state
		 * transitions (INIT, SIPI, etc) are handled in-kernel.
		 */
		spinup_ap(vcpu, 0);
#endif

		bhyve_init_vcpu(vcpu);

#ifdef	__FreeBSD__
		/*
		 * Enable the 'unrestricted guest' mode for APs.
		 *
		 * APs startup in power-on 16-bit mode.
		 */
		error = vm_set_capability(vcpu, VM_CAP_UNRESTRICTED_GUEST, 1);
		assert(error == 0);
#endif
	}

#ifndef	__FreeBSD__
	/*
	 * The value of 'suspend' for the BSP depends on whether the -d
	 * (suspend_at_boot) flag was given to bhyve. Regardless of that
	 * value we always want to set the BSP to VRS_RUN and all others to
	 * VRS_HALT.
	 */
	error = vm_set_run_state(vcpu, bsp ? VRS_RUN : VRS_HALT, 0);
	assert(error == 0);
#endif

	fbsdrun_addcpu(vcpu_id(vcpu), suspend);
}

int
bhyve_init_platform(struct vmctx *ctx, struct vcpu *bsp __unused)
{
	int error;

	error = init_msr();
	if (error != 0)
		return (error);
	init_inout();
#ifdef	__FreeBSD__
	kernemu_dev_init();
#endif
	atkbdc_init(ctx);
	pci_irq_init(ctx);
	ioapic_init(ctx);
	rtc_init(ctx);
	sci_init(ctx);
#ifndef	__FreeBSD__
	pmtmr_init(ctx);
#endif
	error = e820_init(ctx);
	if (error != 0)
		return (error);

#ifndef	__FreeBSD__
	if (get_config_bool_default("e820.debug", false))
		e820_dump_table();
#endif

	return (0);
}

int
bhyve_init_platform_late(struct vmctx *ctx, struct vcpu *bsp __unused)
{
	int error;

	if (get_config_bool_default("x86.mptable", true)) {
		error = mptable_build(ctx, guest_ncpus);
		if (error != 0)
			return (error);
	}
	error = smbios_build(ctx);
	if (error != 0)
		return (error);
	error = e820_finalize();
	if (error != 0)
		return (error);

	if (lpc_bootrom() && strcmp(lpc_fwcfg(), "bhyve") == 0)
		fwctl_init();

	if (get_config_bool("acpi_tables")) {
		error = acpi_build(ctx, guest_ncpus);
		assert(error == 0);
	}

	return (0);
}
