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
	MAIN,
	STARTUP,
		SSP_INIT,
		STARTUP_INIT,
		STARTUP_MEMLIST,
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
			MF_INIT_MEMLISTS,
			MF_ROUTE_PCI_BUS,
			MF_ROUTE_IO_PORTS,
			MF_ROUTE_MMIO,
			MF_INIT_TOM,
			MF_INIT_PCIE_REFCLK,
			MF_INIT_PCI_TO,
			MF_INIT_IOHC_FEATURES,
			MF_INIT_IOHC_FCH_LINK,
			MF_INIT_ARBITRATION_IOMS,
			MF_INIT_ARBITRATION_NBIF,
			MF_INIT_SDP_CONTROL,
			MF_INIT_NBIF_SYSHUB_DMA,
			MF_INIT_IOAPIC,
			MF_INIT_BUS_NUM,
			MF_INIT_NBIF_DEV_STRAPS,
			MF_INIT_NBIF_BRIDGE,
			MF_DXIO_INIT,
				MF_DXIO_RPC_SM_RELOAD,
				MF_DXIO_RPC_SM_RELOAD_DONE,
				MF_DXIO_RPC_INIT,
				MF_DXIO_CLOCK_GATING,
				MF_DXIO_SET_VARS,
				MF_DXIO_POWEROFF_CONFIG,
				MF_DXIO_SET_VARS2,
				MF_DXIO_SET_VARS3,
			MF_DXIO_PLAT_DATA,
			MF_DXIO_LOAD_DATA,
			MF_DXIO_MORE_CONF,
			MF_DXIO_STATE_MACHINE,
			MF_INIT_PCIE_PORTS,
			MF_INIT_BRIDGES,
			MF_HACK_BRIDGES,
			MF_HOTPLUG_INIT,
			MF_DONE,
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
