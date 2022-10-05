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
 * Copyright 2022 Oxide Computer Company
 */

#ifndef _SYS_KERNEL_IPCC_H
#define	_SYS_KERNEL_IPCC_H

#include <sys/ipcc_impl.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	IPCC_INIT_UNSET = 0,
	IPCC_INIT_EARLYBOOT,
	IPCC_INIT_KVMAVAIL,
	IPCC_INIT_DEVTREE,
} ipcc_init_t;

void kernel_ipcc_init(ipcc_init_t);
extern void kernel_ipcc_reboot(void);
extern void kernel_ipcc_poweroff(void);
extern void kernel_ipcc_panic(void);
extern int kernel_ipcc_bsu(uint8_t *);
extern int kernel_ipcc_ident(ipcc_ident_t *);
extern int kernel_ipcc_macs(ipcc_mac_t *);
extern int kernel_ipcc_status(uint64_t *);
extern int kernel_ipcc_ackstart(void);

typedef enum {
	BS_START,
	APOB_INIT,
	FABRIC_TOPO_INIT,
	CCX_INIT,
	APOB_RESERVE_PHYS,
	MAIN,
	STARTUP,
		SSP_INIT,
		STARTUP_INIT,
		STARTUP_MEMLIST,
			MMU_INIT,
			BUILD_MEM_NODES,
			KBM_PROBE,
			PERFORM_ALLOCATIONS,
			MEMLIST_PHYS_INSTALL,
			MEMLIST_PHYS_AVAIL,
			MEMLIST_FREE_UNUSED,
			MEMLIST_PHYS_RSVD,
			MEMLIST_FREE_UNUSED2,
			PAGE_COLORING,
			PAGE_CTRS_ALLOC,
			PCF_INIT,
			KPHYSM_INIT,
			INIT_DEBUG_INFO,
			BOOT_MAPIN,
		STARTUP_KMEM,
			LAYOUT_KERNEL_VA,
			KMEM_INIT,
				KMEM_ARENAS,
				KMEM_XLOG_INIT,
				KMEM_CLOG_INIT,
				KMEM_KA_INIT,
		STARTUP_VM,
			KVM_INIT,
			PCIE_CFGSPACE_REMAP,
			PMEM_INIT,
		FABRIC_INIT,
		STARTUP_SMAP,
		STARTUP_MODULES,
		STARTUP_END,
	VM_INIT,
	MOUNTROOT,
	FORCEATTACH,
	START_MP,
	START_INITPROC,
	READY,
} ipcc_bootstamp_t;
extern void kernel_ipcc_bootstamp(ipcc_bootstamp_t);

#ifdef __cplusplus
}
#endif

#endif /* _SYS_KERNEL_IPCC_H */
