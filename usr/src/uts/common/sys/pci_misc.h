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

#ifndef	_SYS_PCI_MISC_H
#define	_SYS_PCI_MISC_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Flags for pci_bar_find_type(). Can be ORed together to match multiple types.
 */
typedef enum pci_bar_type {
	PCI_BAR_IO	= 1 << 0,
	PCI_BAR_MEM32	= 1 << 1,
	PCI_BAR_MEM64	= 1 << 2,
	PCI_BAR_MEM	= PCI_BAR_MEM32 | PCI_BAR_MEM64
} pci_bar_type_t;

/* Maps a PCI BAR (0-5) to a regset number. */
extern int pci_bar_to_rnumber(dev_info_t *, uint8_t, pci_bar_type_t *);

/* Finds the regnum for the first BAR matching the given type flags. */
extern int pci_bar_find_type(dev_info_t *, pci_bar_type_t, uint8_t *);

#ifdef __cplusplus
}
#endif

#endif	/* _SYS_PCI_MISC_H */
