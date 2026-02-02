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

#include <sys/debug.h>
#include <sys/sunddi.h>
#include <sys/pci.h>
#include <sys/pcie.h>
#include <sys/pci_misc.h>
#include <sys/stdbool.h>
#include <sys/sysmacros.h>

/*
 * Miscellaneous PCI utilities for all pci platforms
 */

/*
 * Convert a PCI address type (from pci_phys_hi & PCI_ADDR_MASK) to the
 * corresponding pci_bar_type_t flag.
 */
static pci_bar_type_t
pci_addr_to_bar_type(uint_t addr_type)
{
	switch (addr_type) {
	case PCI_ADDR_IO:
		return (PCI_BARTYPE_IO);
	case PCI_ADDR_MEM32:
		return (PCI_BARTYPE_MEM32);
	case PCI_ADDR_MEM64:
		return (PCI_BARTYPE_MEM64);
	default:
		return (0);
	}
}

/*
 * Iterate over BAR entries in the reg property, invoking the callback for each
 * valid BAR. The callback receives the regset index, the BAR number (0-5), and
 * the BAR type. If the callback returns true, iteration stops and the regset
 * index is returned. Returns -1 if no BAR satisfies the callback or if the
 * reg property cannot be read.
 */
typedef bool (*pci_bar_cb_t)(int, uint8_t, pci_bar_type_t, void *);

static int
pci_bar_iter(dev_info_t *dip, pci_bar_cb_t cb, void *arg)
{
	uint_t regs_length, rcount;
	pci_regspec_t *regs;
	int rnumber = -1;

	if (ddi_prop_lookup_int_array(DDI_DEV_T_ANY, dip,
	    DDI_PROP_DONTPASS, "reg", (int **)&regs, &regs_length) !=
	    DDI_PROP_SUCCESS) {
		return (-1);
	}

	rcount = regs_length * sizeof (int) / sizeof (pci_regspec_t);

	/*
	 * Entry 0 in the reg property is the config space register;
	 * BAR entries start at index 1.
	 */
	for (int i = 1; i < rcount; i++) {
		const uint_t offset = PCI_REG_REG_G(regs[i].pci_phys_hi);
		const uint_t addr_type = regs[i].pci_phys_hi & PCI_ADDR_MASK;
		const pci_bar_type_t bar_type = pci_addr_to_bar_type(addr_type);
		uint8_t bar;

		if (offset < PCI_CONF_BASE0 || offset > PCI_CONF_BASE5)
			continue;
		if ((offset - PCI_CONF_BASE0) % sizeof (uint32_t) != 0)
			continue;

		bar = (offset - PCI_CONF_BASE0) / sizeof (uint32_t);

		if (cb(i, bar, bar_type, arg)) {
			rnumber = i;
			break;
		}
	}

	ddi_prop_free(regs);

	return (rnumber);
}

typedef struct {
	uint8_t		bar;
	pci_bar_type_t	*typep;
} pci_bar_to_rnum_arg_t;

static bool
pci_bar_to_rnum_cb(int rnumber __unused, uint8_t bar, pci_bar_type_t bar_type,
    void *arg)
{
	pci_bar_to_rnum_arg_t *a = arg;

	if (bar != a->bar)
		return (false);

	if (a->typep != NULL)
		*a->typep = bar_type;

	return (true);
}

/*
 * Map a PCI BAR (0-5) to a regset number. BARs may be sparse but regset
 * numbers are not.
 */
int
pci_bar_to_rnumber(dev_info_t *dip, uint8_t bar, pci_bar_type_t *typep)
{
	pci_bar_to_rnum_arg_t arg = {
		.bar = bar,
		.typep = typep,
	};

	if (bar > 5)
		return (-1);

	return (pci_bar_iter(dip, pci_bar_to_rnum_cb, &arg));
}

typedef struct {
	pci_bar_type_t	type_flags;
	uint8_t		*barp;
} pci_bar_find_type_arg_t;

static bool
pci_bar_find_type_cb(int rnumber __unused, uint8_t bar,
    pci_bar_type_t bar_type, void *arg)
{
	pci_bar_find_type_arg_t *a = arg;

	if ((bar_type & a->type_flags) == 0)
		return (false);

	if (a->barp != NULL)
		*a->barp = bar;

	return (true);
}

/*
 * Find the first BAR matching the given type flags. Returns the regset number
 * or -1 if no matching BAR is found. If barp is not NULL, stores the BAR
 * number (0-5) there.
 */
int
pci_bar_find_type(dev_info_t *dip, pci_bar_type_t type_flags, uint8_t *barp)
{
	pci_bar_find_type_arg_t arg = {
		.type_flags = type_flags,
		.barp = barp,
	};

	return (pci_bar_iter(dip, pci_bar_find_type_cb, &arg));
}
