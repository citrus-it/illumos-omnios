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
 * Copyright 2022 OmniOS Community Edition (OmniOSce) Association.
 */

#include <mdb/mdb_param.h>
#include <mdb/mdb_modapi.h>
#include <mdb/mdb_ctf.h>

#define	_SYS_MACHPARAM_H
#include <sys/vmm_impl.h>

#include <stddef.h>
#include <stdbool.h>

typedef struct mdb_mem_map {
	size_t len;
	int segid;
} mdb_mem_map_t;

#define MDB_VM_MAX_MEMMAPS	8

typedef struct mdb_vm {
	mdb_mem_map_t mem_maps[MDB_VM_MAX_MEMMAPS];
	uint16_t sockets, cores, threads;
	bool mem_transient;
} mdb_vm_t;

typedef struct mdb_vmm_softc {
	mdb_vm_t *vmm_vm;
	char vmm_name[VM_MAX_NAMELEN];
	zone_t *vmm_zone;
} mdb_vmm_softc_t;

static uintptr_t mdb_zone0;

static int
bhyve_dcmd(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	mdb_vmm_softc_t vmm;
	mdb_vm_t vm;

	if (argc > 2)
		return (DCMD_USAGE);

	if (!(flags & DCMD_ADDRSPEC)) {
		if (mdb_walk_dcmd("vmm", "vmm", argc, argv) == -1) {
			mdb_warn("can't walk virtual machines");
			return (DCMD_ERR);
		}
		return (DCMD_OK);
	}

	if (mdb_zone0 == 0) {
		GElf_Sym sym;

		if (mdb_lookup_by_name("zone0", &sym) == -1)
			mdb_warn("failed to find 'zone0'");
		else
			mdb_zone0 = sym.st_value;
	}

	if (DCMD_HDRSPEC(flags)) {
		mdb_printf("%<u>%?s %?s %5s %-6s %-2s %s%</u>\n",
		    "SOFTC", "VM", "TOPO", "MiB", "F", "NAME");
	}

	if (mdb_ctf_vread(&vmm, "vmm_softc_t", "mdb_vmm_softc_t",
	    addr, 0) == -1) {
		mdb_warn("can't read vmm_softc_t structure at %p", addr);
		return (DCMD_ERR);
	}

	if (mdb_ctf_vread(&vm, "struct vm", "mdb_vm_t",
	    (uintptr_t)vmm.vmm_vm, 0) == -1) {
		mdb_warn("can't read struct vm at %p", vmm.vmm_vm);
		return (DCMD_ERR);
	}

	size_t memsize = 0;
	for (uint_t i = 0; i < MDB_VM_MAX_MEMMAPS; i++)
		memsize += vm.mem_maps[i].len;

	mdb_printf("%0?p %0?p %d/%d/%d %-6d %c%c %s\n",
	    addr, vmm.vmm_vm,
	    vm.sockets, vm.cores, vm.threads,
	    memsize / (1024 * 1024),
	    (uintptr_t)vmm.vmm_zone == mdb_zone0 ? 'G' : ' ',
	    vm.mem_transient ? 'T' : ' ',
	    vmm.vmm_name);

	return (DCMD_OK);
}

static int
bhyve_walk_init(mdb_walk_state_t *wsp)
{
	GElf_Sym sym;

	if (wsp->walk_addr == 0) {
		if (mdb_lookup_by_name("vmm_list", &sym) == -1) {
			mdb_warn("failed to find 'vmm_list'");
			return (WALK_ERR);
		}
		wsp->walk_addr = (uintptr_t)sym.st_value;
	}
	if (mdb_layered_walk("list", wsp) == -1) {
		mdb_warn("couldn't walk 'list'");
		return (WALK_ERR);
	}
	return (WALK_NEXT);
}

static int
bhyve_walk_step(mdb_walk_state_t *wsp)
{
	return (wsp->walk_callback(wsp->walk_addr, wsp->walk_layer,
	    wsp->walk_cbdata));
}

static void
bhyve_help(void)
{
        mdb_printf("Prints summary information about vmm instances.\n");
}

static const mdb_dcmd_t dcmds[] = {
        { "vmm", "", "print virtual machine information", bhyve_dcmd,
            bhyve_help },
        { NULL }
};

static const mdb_walker_t walkers[] = {
        { "vmm", "walk a list of virtual machines",
	    bhyve_walk_init, bhyve_walk_step, NULL },
        { NULL }
};

static const mdb_modinfo_t modinfo = {
        MDB_API_VERSION, dcmds, walkers
};

const mdb_modinfo_t *
_mdb_init(void)
{
        return (&modinfo);
}
