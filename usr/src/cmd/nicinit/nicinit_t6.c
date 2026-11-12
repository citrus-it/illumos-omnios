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

/*
 * Support for the Chelsio T6. This transitions a T6 from manufacturing to
 * mission mode after verifying the firmware versions and SROM VPD contents
 * are as expected. If there is a mismatch, then the correct versions are
 * programmed before verifying again and moving on.
 */

#include <err.h>
#include <fcntl.h>
#include <libdevinfo.h>
#include <libt6mfg.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ccompile.h>
#include <sys/gpio/dpio.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/types.h>
#include <pcieb_ioctl.h>

#include "nicinit.h"

typedef struct t6_discover {
	uint_t td_num;
	int32_t td_inst;
} t6_discover_t;

typedef enum {
	T6_MODE_MISSION,
	T6_MODE_MFG,
} t6_mode_t;

typedef struct t6 {
	char t6_dpiopath[sizeof ("/dev/dpio/") + DPIO_NAMELEN];
	t6_mfg_t *t6_mfg;
	char *t6_model;
	char *t6_serial;
	ether_addr_t t6_mac;
	int t6_sromfd;
	int t6_flashfd;
	bool t6_srom_ok;
	bool t6_flash_ok;
} t6_t;

static const char *
t6_modename(t6_mode_t mode)
{
	switch (mode) {
	case T6_MODE_MISSION:
		return ("mission");
	case T6_MODE_MFG:
		return ("manufacturing");
	}
	return ("unknown");
}

static void __PRINTFLIKE(2)
t6_err(t6_mfg_t *t6mfg, const char *fmt, ...)
{
	va_list va;

	(void) fprintf(stderr, "%s: ", getprogname());
	va_start(va, fmt);
	(void) vfprintf(stderr, fmt, va);
	va_end(va);

	(void) fprintf(stderr, ": %s: %s (libt6: 0x%x, sys: %u)\n",
	    t6_mfg_errmsg(t6mfg), t6_mfg_err2str(t6mfg, t6_mfg_err(t6mfg)),
	    t6_mfg_err(t6mfg), t6_mfg_syserr(t6mfg));
}

/*
 * Return our state, creating it on first use.
 */
static t6_t *
t6_state(nicinit_t *ni)
{
	t6_t *t6 = ni->ni_priv;

	if (t6 != NULL)
		return (t6);

	if ((t6 = calloc(1, sizeof (*t6))) == NULL) {
		warn("failed to allocate memory");
		return (NULL);
	}
	t6->t6_sromfd = t6->t6_flashfd = -1;

	if (snprintf(t6->t6_dpiopath, sizeof (t6->t6_dpiopath), "/dev/dpio/%s",
	    ni->ni_dpio) >= sizeof (t6->t6_dpiopath)) {
		warnx("DPIO name '%s' is too long", ni->ni_dpio);
		free(t6);
		return (NULL);
	}

	ni->ni_priv = t6;
	return (t6);
}

/*
 * To deal with a series of Gen 2 related training failures that we've seen, we
 * purposefully try to limit the bridge to only operate at Gen 1 speeds during
 * manufacturing mode. This is something that can be cleared through the pcieb
 * driver logic on the T6's bridge. The attachment point minor node will point
 * to the same bridge whose devctl we need to open. As such, we find it through
 * that.
 */
static bool
t6_bridge_limit(const char *ap, uint32_t speed)
{
	char *bridge = NULL;
	int fd;
	pcieb_ioctl_target_speed_t ioc;
	di_node_t dnroot, dn;
	bool ret = true;

	dnroot = di_init("/", DINFOCPYALL);
	if (dnroot == DI_NODE_NIL) {
		warn("failed to take devinfo snapshot");
		return (false);
	}

	for (dn = di_drv_first_node(T6_PCIEB_DRIVER, dnroot);
	    dn != DI_NODE_NIL; dn = di_drv_next_node(dn)) {
		char *appath;

		if ((appath = nicinit_minor_path(dn, ap)) == NULL)
			continue;
		free(appath);

		bridge = nicinit_minor_path(dn, T6_PCIEB_MINOR);
		break;
	}

	di_fini(dnroot);
	if (bridge == NULL) {
		warnx("failed to map ap %s to a PCIe bridge devctl", ap);
		return (false);
	}

	fd = open(bridge, O_RDWR);
	if (fd < 0) {
		warn("failed to open ap %s bridge %s", ap, bridge);
		free(bridge);
		return (false);
	}

	(void) memset(&ioc, 0, sizeof (pcieb_ioctl_target_speed_t));
	ioc.pits_speed = speed;
	if (ioctl(fd, PCIEB_IOCTL_SET_TARGET_SPEED, &ioc) != 0) {
		warn("ioctl to set target speed to PCIe Gen %u failed", speed);
		ret = false;
	}

	(void) close(fd);
	free(bridge);
	return (ret);
}

static bool
get_dpio_mode(const t6_t *t6, t6_mode_t *modep)
{
	dpio_input_t val;
	int fd;

	nicinit_verbose("Reading DPIO status");

	fd = open(t6->t6_dpiopath, O_RDONLY);
	if (fd == -1) {
		warn("Could not open dpio at '%s'", t6->t6_dpiopath);
		return (false);
	}

	if (read(fd, &val, sizeof (val)) != sizeof (val)) {
		warn("Could not read dpio status from '%s'", t6->t6_dpiopath);
		(void) close(fd);
		return (false);
	}

	(void) close(fd);

	*modep = (val == DPIO_INPUT_HIGH) ?
	    T6_MODE_MISSION : T6_MODE_MFG;

	nicinit_verbose("    DPIO is %s (%s mode)",
	    val == DPIO_INPUT_HIGH ? "high" : "low", t6_modename(*modep));

	return (true);
}

static bool
set_dpio_mode(const t6_t *t6, t6_mode_t mode)
{
	dpio_output_t val;
	int fd;

	nicinit_verbose("Setting DPIO for %s mode", t6_modename(mode));

	val = (mode == T6_MODE_MISSION) ?
	    DPIO_OUTPUT_HIGH : DPIO_OUTPUT_LOW;

	fd = open(t6->t6_dpiopath, O_WRONLY);
	if (fd == -1) {
		warn("Could not open dpio at '%s'", t6->t6_dpiopath);
		return (false);
	}

	if (write(fd, &val, sizeof (val)) != sizeof (val)) {
		warn("Could not set dpio status at '%s'", t6->t6_dpiopath);
		(void) close(fd);
		return (false);
	}

	(void) close(fd);
	return (true);
}

static boolean_t
t6mfg_discover_cb(t6_mfg_disc_info_t *info, void *arg)
{
	t6_discover_t *d = arg;

	nicinit_verbose("Found T6 in manufacturing mode:");
	nicinit_verbose("    Instance: %d", info->tmdi_inst);
	nicinit_verbose("        Path: %s", info->tmdi_path);
	nicinit_verbose("         PCI: %x/%x", info->tmdi_vendid,
	    info->tmdi_devid);

	/*
	 * Just record the first one found, we will abort if there are
	 * more than one in any case.
	 */
	if (d->td_num++ == 0)
		d->td_inst = info->tmdi_inst;

	return (B_TRUE);
}

static boolean_t
srom_validate_cb(const t6_mfg_validate_data_t *val, void *arg __unused)
{
	if (val->tval_flags == T6_VALIDATE_F_OK) {
		nicinit_verbose("Region [0x%04x,0x%04x) OK", val->tval_addr,
		    val->tval_addr + val->tval_range);
		return (B_TRUE);
	}

	nicinit_verbose("Region [0x%04x,0x%04x) INVALID!", val->tval_addr,
	    val->tval_addr + val->tval_range);

	if ((val->tval_flags & T6_VALIDATE_F_ERR_OPAQUE) != 0) {
		nicinit_verbose("\tOpaque data mismatch: first incorrect byte "
		    "offset: 0x%x\n", val->tval_opaque_err);
	}

	if ((val->tval_flags & T6_VALIDATE_F_ERR_VPD_ERR) != 0)
		nicinit_verbose("\tVPD Section mismatch");
	if ((val->tval_flags & T6_VALIDATE_F_ERR_VPD_CKSUM) != 0)
		nicinit_verbose("\t\tVPD Checksum mismatch");
	if ((val->tval_flags & T6_VALIDATE_F_ERR_ID) != 0)
		nicinit_verbose("\t\tProduct ID mismatch");
	if ((val->tval_flags & T6_VALIDATE_F_ERR_PN) != 0)
		nicinit_verbose("\t\tPart Number mismatch");
	if ((val->tval_flags & T6_VALIDATE_F_ERR_SN) != 0)
		nicinit_verbose("\t\tSerial Number mismatch");
	if ((val->tval_flags & T6_VALIDATE_F_ERR_MAC) != 0)
		nicinit_verbose("\t\tMAC Address mismatch");
	if ((val->tval_flags & T6_VALIDATE_F_ERR_SS_VID) != 0)
		nicinit_verbose("\t\tSub-system Vendor ID mismatch");
	if ((val->tval_flags & T6_VALIDATE_F_ERR_SS_DID) != 0)
		nicinit_verbose("\t\tSub-system Device ID mismatch");

	/* Return false to indicate failure and stop iteration */
	return (B_FALSE);
}

static bool
verify_srom(t6_mfg_t *t6mfg)
{
	nicinit_verbose("Verifying SROM");

	if (!t6_mfg_srom_validate(t6mfg, T6_MFG_SOURCE_DEVICE,
	    srom_validate_cb, NULL)) {
		t6_err(t6mfg, "failed to verify SROM");
		return (false);
	}

	nicinit_verbose("SROM verification succeeded");
	return (true);
}

static bool
program_srom(t6_mfg_t *t6mfg)
{
	nicinit_verbose("Programming SROM");

	if (!t6_mfg_srom_write(t6mfg, T6_MFG_SOURCE_DEVICE,
	    T6_SROM_WRITE_F_ALL)) {
		t6_err(t6mfg, "failed to program SROM");
		return (false);
	}

	return (true);
}

static boolean_t
flash_validate_cb(const t6_mfg_flash_vdata_t *regdata, void *arg __unused)
{
	nicinit_verbose("Region [0x%07lx,0x%07lx)%s", regdata->tfv_addr,
	    regdata->tfv_addr + regdata->tfv_range,
	    (regdata->tfv_flags & T6_FLASH_VALIDATE_F_NO_SOURCE) != 0 ?
	    " (empty)" : "");

	if ((regdata->tfv_flags & T6_FLASH_VALIDATE_F_ERR) != 0) {
		nicinit_verbose(" INVALID!\n\tOpaque data mismatch: first "
		    "incorrect byte offset: 0x%x\n", regdata->tfv_err);
		/* Return false to indicate failure and stop iteration */
		return (B_FALSE);
	}

	return (B_TRUE);
}

static bool
verify_flash(t6_mfg_t *t6mfg)
{
	nicinit_verbose("Verifying flash");

	if (!t6_mfg_flash_validate(t6mfg, T6_MFG_SOURCE_DEVICE,
	    flash_validate_cb, NULL)) {
		t6_err(t6mfg, "failed to verify flash");
		return (false);
	}

	nicinit_verbose("T6 flash verification succeeded");
	return (true);
}

static bool
program_flash(t6_mfg_t *t6mfg)
{
	nicinit_verbose("Programming flash");

	if (!t6_mfg_flash_write(t6mfg, T6_MFG_SOURCE_DEVICE,
	    T6_FLASH_WRITE_F_ALL)) {
		t6_err(t6mfg, "failed to program flash");
		return (false);
	}

	return (true);
}

/*
 * Determine whether there is a T6 in the given mode, which is to say whether
 * the driver for that mode has attached to anything.
 */
static bool
verify_mode(t6_mode_t mode, bool *foundp)
{
	di_node_t dnroot, dn;

	dnroot = di_init("/", DINFOCPYALL);
	if (dnroot == DI_NODE_NIL) {
		warn("failed to take devinfo snapshot");
		return (false);
	}

	dn = di_drv_first_node(
	    (mode == T6_MODE_MISSION) ? T6_MISSION_DRIVER : T6_MFG_DRIVER,
	    dnroot);
	*foundp = (dn != DI_NODE_NIL);
	di_fini(dnroot);

	nicinit_verbose("Looking for T6 in %s mode: %s", t6_modename(mode),
	    *foundp ? "SUCCESS" : "FAILED");

	return (true);
}

static bool
start_mode(const t6_t *t6, const char *ap, t6_mode_t mode)
{
	uint32_t speed;
	bool found;

	nicinit_verbose("Switching to %s mode", t6_modename(mode));

	if (!nicinit_ap_disconnect(ap))
		return (false);

	if (!set_dpio_mode(t6, mode))
		return (false);

	/*
	 * We need to wait long enough after de-asserting PWREN_L for the SP
	 * to notice and for the sequencer to release CLD_RST_L and PERST_L.
	 * 1s is much longer than required.
	 */
	nicinit_verbose("    sleeping for 1s or so");
	(void) sleep(1);

	speed = mode == T6_MODE_MISSION ? PCIEB_LINK_SPEED_GEN3 :
	    PCIEB_LINK_SPEED_GEN1;
	nicinit_verbose("    setting bridge limit to PCIe Gen %u", speed);
	if (!t6_bridge_limit(ap, speed))
		return (false);

	if (!nicinit_ap_configure(ap))
		return (false);

	if (!verify_mode(mode, &found))
		return (false);

	if (!found) {
		warnx("no T6 found in %s mode after switching to it",
		    t6_modename(mode));
		return (false);
	}

	nicinit_verbose("Successfully switched to %s mode",
	    t6_modename(mode));
	return (true);
}

static bool
t6_args(const nicinit_t *ni, bool offline)
{
	if (ni->ni_naps != 1) {
		warnx("a T6 has exactly one attachment point");
		return (false);
	}

	if (ni->ni_dpio == NULL) {
		warnx("-D must be specified for a T6");
		return (false);
	}

	if (ni->ni_hashlen != 0) {
		warnx("-L does not apply to a T6");
		return (false);
	}

	if (!offline && (ni->ni_fwfile == NULL || ni->ni_sromfile == NULL ||
	    !ni->ni_pci_ss_did_set)) {
		warnx("-s, -f and -P are mandatory when switching to mission "
		    "mode");
		return (false);
	}

	return (true);
}

/*
 * The T6 is online if the DPIO has already been set for mission mode, in which
 * case there must be a device in that mode.
 */
static bool
t6_online(nicinit_t *ni, bool *onlinep)
{
	const t6_t *t6 = t6_state(ni);
	t6_mode_t mode;
	bool found;

	if (t6 == NULL || !get_dpio_mode(t6, &mode))
		return (false);

	if (mode != T6_MODE_MISSION) {
		*onlinep = false;
		return (true);
	}

	nicinit_verbose("DPIO is already set for mission mode");
	if (!verify_mode(T6_MODE_MISSION, &found))
		return (false);
	if (!found) {
		warnx("DPIO is set for mission mode but there is no mission "
		    "mode device");
		return (false);
	}

	*onlinep = true;
	return (true);
}

/*
 * Find a single T6 in manufacturing mode.
 */
static bool
t6_discover(nicinit_t *ni, bool *foundp)
{
	t6_t *t6 = t6_state(ni);
	t6_discover_t discover = { 0 };

	if (t6 == NULL)
		return (false);

	t6->t6_mfg = t6_mfg_init();
	if (t6->t6_mfg == NULL) {
		warnx("failed to create T6 library handle");
		return (false);
	}

	t6_mfg_discover(t6->t6_mfg, t6mfg_discover_cb, &discover);
	if (discover.td_num == 0) {
		warnx("failed to find any T6 in manufacturing mode");
		*foundp = false;
		return (true);
	}
	if (discover.td_num != 1) {
		warnx("found %u T6 devices in manufacturing mode, expected one",
		    discover.td_num);
		return (false);
	}

	if (!t6_mfg_set_dev(t6->t6_mfg, discover.td_inst)) {
		t6_err(t6->t6_mfg, "Failed to set T6 device to instance %d",
		    discover.td_inst);
		return (false);
	}

	nicinit_link_check(ni, T6_MFG_DRIVER, T6_MFG_LINK_GEN,
	    T6_MFG_LINK_WIDTH);

	*foundp = true;
	return (true);
}

/*
 * Retrieve the required information from the service processor and set up the
 * sources that the SROM and flash will be verified against.
 */
static bool
t6_prepare(nicinit_t *ni)
{
	t6_t *t6 = ni->ni_priv;
	t6_mfg_t *t6mfg = t6->t6_mfg;

	if (access(ni->ni_fwfile, R_OK) != 0) {
		warn("cannot read firmware file '%s'", ni->ni_fwfile);
		return (false);
	}
	if (access(ni->ni_sromfile, R_OK) != 0) {
		warn("cannot read SROM file '%s'", ni->ni_sromfile);
		return (false);
	}

	if (!nicinit_ident(&t6->t6_model, &t6->t6_serial) ||
	    !nicinit_macaddr(T6_MAC_COUNT, T6_MAC_STRIDE, &t6->t6_mac)) {
		return (false);
	}

	if (!t6_mfg_srom_set_pn(t6mfg, t6->t6_model)) {
		t6_err(t6mfg, "failed to set model number");
		return (false);
	}

	if (!t6_mfg_srom_set_sn(t6mfg, t6->t6_serial)) {
		t6_err(t6mfg, "failed to set serial number");
		return (false);
	}

	if (!t6_mfg_srom_set_mac(t6mfg, (uint8_t *)&t6->t6_mac)) {
		t6_err(t6mfg, "failed to set MAC address");
		return (false);
	}

	if (!t6_mfg_srom_set_id(t6mfg, (char *)T6_PRODUCT_STR)) {
		t6_err(t6mfg, "failed to set product string");
		return (false);
	}

	if (!t6_mfg_srom_set_pci_ss_vid(t6mfg, T6_PCI_SUBSYSTEM_VENDORID)) {
		t6_err(t6mfg, "failed to set PCI sub-system vendor ID");
		return (false);
	}

	if (!t6_mfg_srom_set_pci_ss_did(t6mfg, ni->ni_pci_ss_did)) {
		t6_err(t6mfg, "failed to set PCI sub-system device ID");
		return (false);
	}

	if ((t6->t6_sromfd = open(ni->ni_sromfile, O_RDONLY)) < 0) {
		warn("failed to open srom file %s", ni->ni_sromfile);
		return (false);
	}

	if (!t6_mfg_srom_set_base(t6mfg, t6->t6_sromfd)) {
		t6_err(t6mfg, "failed to set SROM base source");
		return (false);
	}

	if ((t6->t6_flashfd = open(ni->ni_fwfile, O_RDONLY)) < 0) {
		warn("failed to open flash file %s", ni->ni_fwfile);
		return (false);
	}

	if (!t6_mfg_flash_set_base(t6mfg, T6_MFG_FLASH_BASE_FW,
	    t6->t6_flashfd)) {
		t6_err(t6mfg, "failed to set flash base source");
		return (false);
	}

	return (true);
}

/*
 * Verify both the SROM and the flash, remembering which of them need to be
 * programmed. A verification failure has already been reported and is not an
 * error here.
 */
static bool
t6_verify(nicinit_t *ni, bool *matchp)
{
	t6_t *t6 = ni->ni_priv;

	t6->t6_srom_ok = verify_srom(t6->t6_mfg);
	t6->t6_flash_ok = verify_flash(t6->t6_mfg);
	*matchp = t6->t6_srom_ok && t6->t6_flash_ok;

	return (true);
}

static bool
t6_program(nicinit_t *ni)
{
	const t6_t *t6 = ni->ni_priv;

	if (!t6->t6_srom_ok && !program_srom(t6->t6_mfg))
		return (false);
	if (!t6->t6_flash_ok && !program_flash(t6->t6_mfg))
		return (false);

	return (true);
}

static bool
t6_release(nicinit_t *ni)
{
	t6_t *t6 = ni->ni_priv;

	/*
	 * The manufacturing mode device is about to go away, so let go of it
	 * and the files that it was being verified against.
	 */
	t6_mfg_fini(t6->t6_mfg);
	t6->t6_mfg = NULL;

	return (start_mode(t6, ni->ni_aps[0], T6_MODE_MISSION));
}

/*
 * Switch the T6 back to manufacturing mode.
 */
static bool
t6_offline(nicinit_t *ni)
{
	const t6_t *t6 = t6_state(ni);
	t6_mode_t mode;
	bool found;

	if (t6 == NULL || !get_dpio_mode(t6, &mode))
		return (false);

	if (mode == T6_MODE_MISSION)
		return (start_mode(t6, ni->ni_aps[0], T6_MODE_MFG));

	nicinit_verbose("DPIO is not set for mission mode");
	if (!verify_mode(T6_MODE_MFG, &found))
		return (false);
	if (!found) {
		warnx("DPIO is set for manufacturing mode but there is no "
		    "manufacturing mode device");
		return (false);
	}

	return (true);
}

static void
t6_fini(nicinit_t *ni)
{
	t6_t *t6 = ni->ni_priv;

	if (t6 == NULL)
		return;

	if (t6->t6_mfg != NULL)
		t6_mfg_fini(t6->t6_mfg);
	if (t6->t6_sromfd >= 0)
		(void) close(t6->t6_sromfd);
	if (t6->t6_flashfd >= 0)
		(void) close(t6->t6_flashfd);
	free(t6->t6_model);
	free(t6->t6_serial);
	free(t6);
	ni->ni_priv = NULL;
}

const nicinit_ops_t nicinit_t6_ops = {
	.no_name = "t6",
	.no_driver = T6_MISSION_DRIVER,
	.no_args = t6_args,
	.no_online = t6_online,
	.no_discover = t6_discover,
	.no_prepare = t6_prepare,
	.no_verify = t6_verify,
	.no_program = t6_program,
	.no_release = t6_release,
	.no_offline = t6_offline,
	.no_fini = t6_fini
};
