/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */

/*
 * Copyright 2008 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * Stub driver for the devices that Xen emulates alongside its
 * paravirtualised equivalents. Under Xen HVM a disk is presented both as an
 * emulated PCI IDE controller and as a paravirtualised xdf device, and the
 * default NIC both as an emulated Realtek 8139 and as a paravirtualised xnf
 * device. Binding this do-nothing driver to the emulated devices keeps
 * pci-ide/cmdk and rtls from attaching to them so that the xdf and xnf
 * front-end drivers are used instead.
 *
 * The driver_aliases for this module match only the Xen subsystem variants of
 * the emulated devices (subsystem vendor 0x5853, "XS"), so it never claims
 * anything on bare metal. A specific compatible match also takes precedence
 * over the generic "pci-ide" node name assigned during PCI enumeration.
 */

#include <sys/sunddi.h>
#include <sys/errno.h>
#include <sys/modctl.h>

static int
stubattach(dev_info_t *dip __unused, ddi_attach_cmd_t cmd __unused)
{
	return (DDI_SUCCESS);
}

static int
stubdetach(dev_info_t *dip __unused, ddi_detach_cmd_t cmd __unused)
{
	return (DDI_SUCCESS);
}

static struct dev_ops stub_ops = {
	DEVO_REV,
	0,
	NULL,
	nulldev,
	nulldev,
	stubattach,
	stubdetach,
	nodev,
	NULL,
	NULL,
	NULL,
	ddi_quiesce_not_needed
};

static struct modldrv modldrv = {
	&mod_driverops,
	"Xen HVM emulated device stub",
	&stub_ops
};

static struct modlinkage modlinkage = {
	MODREV_1, (void *)&modldrv, NULL
};

int
_init(void)
{
	return (mod_install(&modlinkage));
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

int
_fini(void)
{
	return (EBUSY);
}
