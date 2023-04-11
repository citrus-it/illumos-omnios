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
 * Copyright 2023 OmniOS Community Edition (OmniOSce) Association.
 */

#include <err.h>
#include <libdevinfo.h>
#include <priv.h>
#include <stdlib.h>
#include <string.h>
#include <sys/debug.h>
#include <sys/pci.h>

#include "debug.h"
#include "pci_emul.h"

static di_node_t rootnode = DI_NODE_NIL;

typedef struct cb_data {
	uint8_t cbd_bus;
	uint8_t cbd_dev;
	uint8_t cbd_func;
	di_node_t cbd_node;
} cb_data_t;

void
pci_illumos_init(void)
{
	assert(rootnode == DI_NODE_NIL);
	rootnode = di_init("/", DINFOCPYALL);
	if (rootnode == DI_NODE_NIL)
		err(4, "Could not initialise devinfo snapshot");
}

void
pci_illumos_fini(void)
{
	assert(rootnode != DI_NODE_NIL);
	di_fini(rootnode);
	rootnode = DI_NODE_NIL;
}

static bool
is_pci(di_node_t node)
{
	const char *name;
	char *compat;
	int nents;

	name = di_node_name(node);
	if (strncmp("pci", name, 3) == 0)
		return (name[3] != '\0');

	nents = di_prop_lookup_strings(DDI_DEV_T_ANY, node, "compatible",
	    &compat);
	if (nents <= 0)
		return (false);

	for (uint_t i = 0; i < nents; i++) {
		if (strncmp("pciclass,", compat, strlen("pciclass,")) == 0 ||
		    strncmp("pciexclass,", compat, strlen("pciexclass,")) ==
		    0) {
			return (true);
		}

		compat += strlen(compat) + 1;
	}

	return (false);
}

static int
find_device_cb(di_node_t node, void *arg)
{
	cb_data_t *cbd = arg;
	int nprop, *regs = NULL;
	uint8_t bus, dev, func;

	if (!is_pci(node))
		return (DI_WALK_CONTINUE);

	nprop = di_prop_lookup_ints(DDI_DEV_T_ANY, node, "reg", &regs);
	if (nprop <= 0)
		return (DI_WALK_CONTINUE);

	bus = PCI_REG_BUS_G(regs[0]);
	dev = PCI_REG_DEV_G(regs[0]);
	func = PCI_REG_FUNC_G(regs[0]);

	if (bus != cbd->cbd_bus || dev != cbd->cbd_dev || func != cbd->cbd_func)
		return (DI_WALK_CONTINUE);

	cbd->cbd_node = node;
	return (DI_WALK_TERMINATE);
}

static di_node_t
find_pci_device(uint8_t bus, uint8_t dev, uint8_t func)
{
	cb_data_t cbd = {
		.cbd_bus = bus,
		.cbd_dev = dev,
		.cbd_func = func,
		.cbd_node = DI_NODE_NIL,
	};

	(void) di_walk_node(rootnode, DI_WALK_CLDFIRST, &cbd, find_device_cb);

	return (cbd.cbd_node);
}

static int
find_lpc_cb(di_node_t node, void *arg)
{
	cb_data_t *cbd = arg;
	int nprop, *valp = NULL, *regs = NULL;
	uint32_t class_code;

	if (!is_pci(node))
		return (DI_WALK_CONTINUE);

	nprop = di_prop_lookup_ints(DDI_DEV_T_ANY, node, "class-code", &valp);
	if (nprop != 1)
		return (DI_WALK_CONTINUE);

	class_code = (uint32_t)*valp;
	class_code &= 0x00ffff00;

	if (class_code != ((PCI_CLASS_BRIDGE << 16) | (PCI_BRIDGE_ISA << 8)))
		return (DI_WALK_CONTINUE);

	nprop = di_prop_lookup_ints(DDI_DEV_T_ANY, node, "reg", &regs);
	if (nprop <= 0)
		return (DI_WALK_CONTINUE);

	cbd->cbd_bus = PCI_REG_BUS_G(regs[0]);
	cbd->cbd_dev = PCI_REG_DEV_G(regs[0]);
	cbd->cbd_func = PCI_REG_FUNC_G(regs[0]);
	cbd->cbd_node = node;

	return (DI_WALK_TERMINATE);
}

int
pci_illumos_find_lpc(struct pcisel *const sel)
{
	int ret = -1;

	cb_data_t cbd = {
		.cbd_node = DI_NODE_NIL,
	};

	pci_illumos_init();

	(void) di_walk_node(rootnode, DI_WALK_CLDFIRST, &cbd, find_lpc_cb);

	if (cbd.cbd_node != DI_NODE_NIL) {
		sel->pc_bus = cbd.cbd_bus;
		sel->pc_dev = cbd.cbd_dev;
		sel->pc_func = cbd.cbd_func;
		PRINTLN("Found LPC bridge at %x/%x/%x", cbd.cbd_bus,
		    cbd.cbd_dev, cbd.cbd_func);
		ret = 0;
	}

	pci_illumos_fini();

	return (ret);
}

uint32_t
read_config(const struct pcisel *sel, long reg, int width)
{
	di_node_t node;
	int nprop, *idp = NULL;
	const char *prop;

	assert(width == 2);

	node = find_pci_device(sel->pc_bus, sel->pc_dev, sel->pc_func);

	if (node == DI_NODE_NIL)
		return (-1);

	switch (reg) {
	case PCIR_DEVICE:
		prop = "device-id";
		break;
	case PCIR_VENDOR:
		prop = "vendor-id";
		break;
	case PCIR_REVID:
	case PCIR_SUBVEND_0:
	case PCIR_SUBDEV_0:
	default:
		warnx("Unhandled register in read_config - %ld+%d",
		    reg, width);
		return (-1);
	}

	nprop = di_prop_lookup_ints(DDI_DEV_T_ANY, node, prop, &idp);

	if (nprop != 1)
		return (-1);

	return ((uint16_t)*idp);
}

void
write_config(const struct pcisel *sel, long reg, int width, uint32_t data)
{
	errx(4, "write_config() unimplemented on illumos");
}
