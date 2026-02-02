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

#if 0
#include <sys/note.h>
#include <sys/conf.h>
#include <sys/sunddi.h>
#include <sys/bitmap.h>
#include <sys/autoconf.h>
#include <sys/pci_cap.h>
#endif
#include <sys/debug.h>
#include <sys/pci.h>
#include <sys/pcie.h>
#include <sys/pci_misc.h>
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
		return (PCI_BAR_IO);
	case PCI_ADDR_MEM32:
		return (PCI_BAR_MEM32);
	case PCI_ADDR_MEM64:
		return (PCI_BAR_MEM64);
	default:
		return (0);
	}
}

/*
 * Map a PCI BAR (0-5) to a regset number. BARs may be sparse but regset
 * numbers are not.
 */
int
pci_bar_to_rnumber(dev_info_t *dip, uint8_t bar, pci_bar_type_t *typep)
{
	uint_t bar_offset, regs_length, rcount;
	pci_regspec_t *regs;
	int rnumber = -1;

	if (bar > 5)
		return (-1);

	/*
	 * PCI_CONF_BASE0 is 0x10; each BAR is 4 bytes apart.
	 */
	bar_offset = PCI_CONF_BASE0 + sizeof (uint32_t) * bar;

	if (ddi_prop_lookup_int_array(DDI_DEV_T_ANY, dip,
	    DDI_PROP_DONTPASS, "reg", (int **)&regs, &regs_length) !=
	    DDI_PROP_SUCCESS) {
		return (-1);
	}

	rcount = regs_length * sizeof (int) / sizeof (pci_regspec_t);

	for (int i = 1; i < rcount; i++) {
		const uint_t offset = PCI_REG_REG_G(regs[i].pci_phys_hi);
		const uint_t addr_type = regs[i].pci_phys_hi & PCI_ADDR_MASK;

		if (offset < PCI_CONF_BASE0 || offset > PCI_CONF_BASE5)
			continue;
		if ((offset - PCI_CONF_BASE0) % sizeof (uint32_t) != 0)
			continue;
		if (offset == bar_offset) {
			if (typep != NULL)
				*typep = pci_addr_to_bar_type(addr_type);
			rnumber = i;
			break;
		}
	}

	ddi_prop_free(regs);

	return ((rnumber < rcount) ? rnumber : -1);
}

/*
 * Find the first BAR matching the given type flags. Returns the regset number
 * or -1 if no matching BAR is found. If barp is not NULL, stores the BAR
 * number (0-5) there.
 */
int
pci_bar_find_type(dev_info_t *dip, pci_bar_type_t type_flags, uint8_t *barp)
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

	for (int i = 1; i < rcount; i++) {
		const uint_t offset = PCI_REG_REG_G(regs[i].pci_phys_hi);
		const uint_t addr_type = regs[i].pci_phys_hi & PCI_ADDR_MASK;
		const pci_bar_type_t bar_type = pci_addr_to_bar_type(addr_type);

		if (offset < PCI_CONF_BASE0 || offset > PCI_CONF_BASE5)
			continue;
		if ((offset - PCI_CONF_BASE0) % sizeof (uint32_t) != 0)
			continue;
		if ((bar_type & type_flags) != 0) {
			if (barp != NULL) {
				*barp = (offset - PCI_CONF_BASE0) /
				    sizeof (uint32_t);
			}
			rnumber = i;
			break;
		}
	}

	ddi_prop_free(regs);

	return (rnumber);
}
