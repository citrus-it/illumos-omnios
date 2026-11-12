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
 * Support for Redhawk, a NIC built around a Versal device that boots from an
 * image in flash. That flash is connected to the sequencer FPGA, which is also
 * the eSPI target on the host's second eSPI controller, and the FPGA controls
 * access to it. While the Versal is in reset we can read and write the flash
 * through espi_flash(4D).
 *
 * When the NIC is powered the FPGA computes a SHA3-256 digest over the start
 * of the flash. It then keeps the Versal in reset, with PCIe presence
 * deasserted for both of its slots, until we enable it. The digest and the
 * controls are in the platform-specific part of the target's configuration
 * register space, which we access through the control node of espi(4D). The
 * sequence here is:
 *
 *   - Compare the FPGA's digest with that of the firmware image that we have,
 *     padded out to the measured length with the value of erased flash.
 *
 *   - If they differ, write our image to the flash, erase the remainder of the
 *     measured range, and have the FPGA measure it again.
 *
 *   - Set mission_mode_en. The FPGA then gives the Versal access to the
 *     flash, releases it from reset and waits for it to report that it has
 *     been configured, at which point presence is asserted.
 *
 *   - Wait for the hotplug framework to configure both slots.
 *
 * The flash may not be accessed while the FPGA is measuring it or while the
 * Versal has access to it.
 */

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <libdevinfo.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/bitext.h>
#include <sys/espi_ctl.h>
#include <sys/espi_flash.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/time.h>
#include <sys/types.h>

#include "nicinit.h"
#include "nicinit_sha3.h"

#define	RH_ESPI_DRIVER		"espi"
#define	RH_ESPI_CTL_MINOR	"ctl"
#define	RH_FLASH_DRIVER		"espi_flash"
#define	RH_FLASH_MINOR		"flash"

/*
 * The driver for the PCIe devices that the NIC presents once it has been
 * configured.
 */
#define	RH_DRIVER		"rhdrv"

/*
 * The platform-specific configuration registers that the sequencer FPGA
 * provides.
 */

/*
 * Identifies the board that the target is on.
 */
#define	RH_REG_BOARD_ID			0x800
#define	RH_REG_BOARD_ID_GET_BOARD(r)	bitx32(r, 7, 0)
#define	RH_REG_BOARD_ID_BOARD_METRO	1

/*
 * Control over the measurement of the NIC's boot image. DONE is set once
 * a measurement has finished and the digest is available, and is cleared by
 * writing a 1 to it. Writing a 1 to REHASH requests a new measurement, and it
 * reads as 1 until that has finished.
 */
#define	RH_REG_HASH_CTRL		0x804
#define	RH_REG_HASH_CTRL_GET_REHASH(r)	bitx32(r, 1, 1)
#define	RH_REG_HASH_CTRL_GET_DONE(r)	bitx32(r, 0, 0)
#define	RH_REG_HASH_CTRL_REHASH		(1 << 1)
#define	RH_REG_HASH_CTRL_DONE		(1 << 0)

/*
 * Setting MISSION_MODE_EN has the FPGA configure the NIC from the image that
 * it has measured. Clearing it returns the NIC to reset.
 */
#define	RH_REG_CONFIG_CTRL			0x808
#define	RH_REG_CONFIG_CTRL_MISSION_MODE_EN	(1 << 0)

#define	RH_REG_CONFIG_STATUS			0x80c
#define	RH_REG_CONFIG_STATUS_GET_ERROR_OUT(r)	bitx32(r, 3, 3)
#define	RH_REG_CONFIG_STATUS_GET_TIMED_OUT(r)	bitx32(r, 2, 2)
#define	RH_REG_CONFIG_STATUS_GET_STATE(r)	bitx32(r, 1, 0)
#define	RH_REG_CONFIG_STATUS_STATE_IDLE		0
#define	RH_REG_CONFIG_STATUS_STATE_IN_PROGRESS	1
#define	RH_REG_CONFIG_STATUS_STATE_DONE		2
#define	RH_REG_CONFIG_STATUS_STATE_FAILED	3

/*
 * How long the FPGA gives the NIC to be configured before it gives up, in
 * milliseconds.
 */
#define	RH_REG_CONFIG_TIMEOUT		0x810

/*
 * The digest occupies RH_REG_HASH_COUNT registers. The first contains bytes 0
 * to 3 of the digest with byte 0 in its least significant byte.
 */
#define	RH_REG_HASH0			0x820
#define	RH_REG_HASH_COUNT		8

/*
 * The longest that we will wait for the FPGA to finish a measurement. A
 * measurement of 7.2MiB has been seen to take around 0.6 seconds.
 */
#define	RH_HASH_TIMEOUT_SEC	60

/*
 * How much longer than the FPGA's timeout we wait for it to report whether
 * the NIC was configured, and how long we wait for it to report that the NIC
 * is back in reset.
 */
#define	RH_CONFIG_MARGIN_SEC	5

/*
 * How long we give the hotplug framework to configure the slots once presence
 * has been asserted.
 */
#define	RH_AP_TIMEOUT_SEC	30

/*
 * The erase block size assumed for a target that does not advertise any, which
 * is the smallest that the protocol can ask for.
 */
#define	RH_DEFAULT_ERASE	(4 * 1024)

/*
 * Progress is reported each time this much of the flash has been dealt with.
 */
#define	RH_PROGRESS_BYTES	(4 * 1024 * 1024)

/*
 * The FPGA measures the flash with SHA3-256.
 */
#define	SHA3_256_DIGEST_LENGTH	32

#define	RH_DIGEST_STRLEN	(SHA3_256_DIGEST_LENGTH * 2 + 1)

/*
 * The erase block sizes that can be requested over the channel, smallest first.
 * espi_flash(4D) advertises sizes as a bitmask with bit n set for 2^n KiB
 * blocks.
 */
static const uint32_t rh_erase_sizes[] = {
	4 * 1024, 32 * 1024, 64 * 1024, 128 * 1024
};

typedef struct redhawk {
	char *rh_ctlpath;
	char *rh_flashpath;
	int rh_ctlfd;
	const uint8_t *rh_img;
	size_t rh_imglen;
	uint8_t rh_want[SHA3_256_DIGEST_LENGTH];
	/* The flash has changed since the FPGA last measured it. */
	bool rh_stale;
} redhawk_t;

static const char *
rh_digest_str(const uint8_t digest[SHA3_256_DIGEST_LENGTH],
    char buf[RH_DIGEST_STRLEN])
{
	static const char hex[] = "0123456789abcdef";

	for (uint_t i = 0; i < SHA3_256_DIGEST_LENGTH; i++) {
		buf[i * 2] = hex[digest[i] >> 4];
		buf[i * 2 + 1] = hex[digest[i] & 0xf];
	}
	buf[SHA3_256_DIGEST_LENGTH * 2] = '\0';

	return (buf);
}

static bool
rh_reg_read(const redhawk_t *rh, uint32_t reg, uint32_t *valp)
{
	espi_cfg_t cfg = {
		.ec_offset = reg
	};

	if (ioctl(rh->rh_ctlfd, ESPI_IOC_CFG_READ, &cfg) != 0) {
		warn("failed to read eSPI configuration register 0x%x", reg);
		return (false);
	}

	*valp = cfg.ec_value;
	return (true);
}

static bool
rh_reg_write(const redhawk_t *rh, uint32_t reg, uint32_t val)
{
	espi_cfg_t cfg = {
		.ec_offset = reg,
		.ec_value = val
	};

	if (ioctl(rh->rh_ctlfd, ESPI_IOC_CFG_WRITE, &cfg) != 0) {
		warn("failed to write 0x%x to eSPI configuration register 0x%x",
		    val, reg);
		return (false);
	}

	return (true);
}

/*
 * Transfer all of a buffer, retrying short transfers.
 */
static bool
rh_pread(int fd, void *buf, size_t len, off_t off)
{
	uint8_t *p = buf;

	while (len > 0) {
		const ssize_t n = pread(fd, p, len, off);

		if (n < 0) {
			if (errno == EINTR)
				continue;
			return (false);
		}
		if (n == 0) {
			errno = EIO;
			return (false);
		}
		p += n;
		off += n;
		len -= n;
	}

	return (true);
}

static bool
rh_pwrite(int fd, const void *buf, size_t len, off_t off)
{
	const uint8_t *p = buf;

	while (len > 0) {
		const ssize_t n = pwrite(fd, p, len, off);

		if (n < 0) {
			if (errno == EINTR)
				continue;
			return (false);
		}
		if (n == 0) {
			errno = EIO;
			return (false);
		}
		p += n;
		off += n;
		len -= n;
	}

	return (true);
}

/*
 * Find the eSPI controller that we talk to the FPGA through, which is the one
 * with a control node, and the flash device beneath it. The flash device only
 * has a minor node while its driver is attached, which it may not be. The path
 * to the minor node is therefore built from the path of the device, and
 * opening it will attach the driver.
 */
static bool
rh_find_devices(redhawk_t *rh)
{
	di_node_t root, node, child;

	root = di_init_driver(RH_ESPI_DRIVER, DINFOSUBTREE | DINFOMINOR);
	if (root == DI_NODE_NIL) {
		warn("failed to take a devinfo snapshot of the %s driver",
		    RH_ESPI_DRIVER);
		return (false);
	}

	for (node = di_drv_first_node(RH_ESPI_DRIVER, root);
	    node != DI_NODE_NIL; node = di_drv_next_node(node)) {
		rh->rh_ctlpath = nicinit_minor_path(node, RH_ESPI_CTL_MINOR);
		if (rh->rh_ctlpath != NULL)
			break;
	}

	if (node == DI_NODE_NIL) {
		warnx("found no eSPI controller with a control node");
		di_fini(root);
		return (false);
	}

	for (child = di_child_node(node); child != DI_NODE_NIL;
	    child = di_sibling_node(child)) {
		const char *drv = di_driver_name(child);

		char *path;

		if (drv == NULL || strcmp(drv, RH_FLASH_DRIVER) != 0)
			continue;

		if ((path = di_devfs_path(child)) == NULL) {
			warn("failed to get the path of the flash device");
			di_fini(root);
			return (false);
		}
		if (asprintf(&rh->rh_flashpath, "/devices%s:%s", path,
		    RH_FLASH_MINOR) < 0) {
			warn("failed to construct a device path");
			rh->rh_flashpath = NULL;
			di_devfs_path_free(path);
			di_fini(root);
			return (false);
		}
		di_devfs_path_free(path);
		break;
	}

	di_fini(root);

	if (rh->rh_flashpath == NULL) {
		warnx("found no flash device beneath %s", rh->rh_ctlpath);
		return (false);
	}

	nicinit_verbose("eSPI control node: %s", rh->rh_ctlpath);
	nicinit_verbose("     Flash device: %s", rh->rh_flashpath);

	return (true);
}

/*
 * Map the firmware image and compute the digest that the FPGA will report if
 * the flash contains it. The measured range is longer than the image and the
 * rest of it is expected to be erased.
 */
static bool
rh_load_image(nicinit_t *ni, redhawk_t *rh)
{
	static uint8_t pad[64 * 1024];
	char dstr[RH_DIGEST_STRLEN];
	sha3_ctx_t ctx;
	struct stat st;
	void *img;
	int fd;

	if (rh->rh_img != NULL)
		return (true);

	if ((fd = open(ni->ni_fwfile, O_RDONLY)) < 0) {
		warn("failed to open firmware file '%s'", ni->ni_fwfile);
		return (false);
	}

	if (fstat(fd, &st) != 0) {
		warn("failed to stat firmware file '%s'", ni->ni_fwfile);
		(void) close(fd);
		return (false);
	}

	if (st.st_size == 0 || (uint64_t)st.st_size > ni->ni_hashlen) {
		warnx("firmware file '%s' is %" PRIu64 " bytes but must be "
		    "between 1 and the measured length of %" PRIu64 " bytes",
		    ni->ni_fwfile, (uint64_t)st.st_size, ni->ni_hashlen);
		(void) close(fd);
		return (false);
	}

	img = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	if (img == MAP_FAILED) {
		warn("failed to map firmware file '%s'", ni->ni_fwfile);
		(void) close(fd);
		return (false);
	}
	(void) close(fd);

	rh->rh_img = img;
	rh->rh_imglen = st.st_size;

	(void) memset(pad, 0xff, sizeof (pad));
	(void) sha3_init(&ctx, SHA3_256_DIGEST_LENGTH);
	(void) sha3_update(&ctx, rh->rh_img, rh->rh_imglen);
	for (uint64_t left = ni->ni_hashlen - rh->rh_imglen; left > 0; ) {
		const size_t n = MIN(left, sizeof (pad));

		(void) sha3_update(&ctx, pad, n);
		left -= n;
	}
	(void) sha3_final(rh->rh_want, &ctx);

	nicinit_verbose("Firmware image %s: %zu bytes", ni->ni_fwfile,
	    rh->rh_imglen);
	nicinit_verbose("    Digest over %" PRIu64 " bytes: %s", ni->ni_hashlen,
	    rh_digest_str(rh->rh_want, dstr));
	nicinit_detail_string(ni, "expected-digest", dstr);

	return (true);
}

/*
 * Return our state, creating it on first use by finding and opening the
 * devices that we need and checking that this is a board we know about.
 */
static redhawk_t *
rh_state(nicinit_t *ni)
{
	redhawk_t *rh = ni->ni_priv;
	uint32_t val;

	if (rh != NULL)
		return (rh);

	if ((rh = calloc(1, sizeof (*rh))) == NULL) {
		warn("failed to allocate memory");
		return (NULL);
	}
	rh->rh_ctlfd = -1;
	ni->ni_priv = rh;

	if (!rh_find_devices(rh))
		return (NULL);

	if ((rh->rh_ctlfd = open(rh->rh_ctlpath, O_RDWR)) < 0) {
		warn("failed to open %s", rh->rh_ctlpath);
		return (NULL);
	}

	if (!rh_reg_read(rh, RH_REG_BOARD_ID, &val))
		return (NULL);
	if (RH_REG_BOARD_ID_GET_BOARD(val) != RH_REG_BOARD_ID_BOARD_METRO) {
		warnx("eSPI target reports board ID 0x%x, which does not have "
		    "a Redhawk", RH_REG_BOARD_ID_GET_BOARD(val));
		return (NULL);
	}

	return (rh);
}

/*
 * Wait for the FPGA to finish any measurement that it is making.
 */
static bool
rh_hash_wait(const redhawk_t *rh, uint32_t *ctrlp)
{
	const hrtime_t deadline = gethrtime() + SEC2NSEC(RH_HASH_TIMEOUT_SEC);

	for (;;) {
		if (!rh_reg_read(rh, RH_REG_HASH_CTRL, ctrlp))
			return (false);
		if (RH_REG_HASH_CTRL_GET_REHASH(*ctrlp) == 0)
			return (true);
		if (gethrtime() > deadline)
			break;
		(void) usleep(NICINIT_POLL_USEC);
	}

	warnx("the FPGA has not finished measuring the flash after %u seconds",
	    RH_HASH_TIMEOUT_SEC);
	return (false);
}

/*
 * Retrieve the FPGA's digest of the flash. The FPGA measures the flash when
 * the NIC is powered, so a digest is usually available. If one is not, or if
 * the flash has changed since, a new measurement is requested. That must only
 * be done while the NIC is in reset, as the FPGA cannot read the flash
 * otherwise. When the NIC is not in reset the caller passes a pointer, and
 * the lack of a digest is reported through that instead.
 */
static bool
rh_measure(nicinit_t *ni, redhawk_t *rh, uint8_t digest[SHA3_256_DIGEST_LENGTH],
    bool *nonep)
{
	char dstr[RH_DIGEST_STRLEN];
	uint32_t ctrl;

	if (!rh_hash_wait(rh, &ctrl))
		return (false);

	if (nonep != NULL) {
		*nonep = RH_REG_HASH_CTRL_GET_DONE(ctrl) == 0;
		if (*nonep)
			return (true);
	} else if (rh->rh_stale || RH_REG_HASH_CTRL_GET_DONE(ctrl) == 0) {
		nicinit_verbose("Requesting a measurement of the flash");

		if (RH_REG_HASH_CTRL_GET_DONE(ctrl) != 0 &&
		    !rh_reg_write(rh, RH_REG_HASH_CTRL,
		    RH_REG_HASH_CTRL_DONE)) {
			return (false);
		}
		if (!rh_reg_write(rh, RH_REG_HASH_CTRL,
		    RH_REG_HASH_CTRL_REHASH)) {
			return (false);
		}
		if (!rh_hash_wait(rh, &ctrl))
			return (false);

		if (RH_REG_HASH_CTRL_GET_DONE(ctrl) == 0) {
			warnx("the FPGA did not produce a measurement of the "
			    "flash when one was requested");
			return (false);
		}
		rh->rh_stale = false;
	}

	for (uint_t i = 0; i < RH_REG_HASH_COUNT; i++) {
		uint32_t val;

		if (!rh_reg_read(rh, RH_REG_HASH0 + i * sizeof (val), &val))
			return (false);
		for (uint_t j = 0; j < sizeof (val); j++) {
			digest[i * sizeof (val) + j] =
			    bitx32(val, j * 8 + 7, j * 8);
		}
	}

	nicinit_detail_string(ni, "measured-digest",
	    rh_digest_str(digest, dstr));

	return (true);
}

/*
 * Tell the FPGA to put the NIC back into reset and wait until it has.
 */
static bool
rh_reset(const redhawk_t *rh)
{
	const hrtime_t deadline = gethrtime() + SEC2NSEC(RH_CONFIG_MARGIN_SEC);
	uint32_t status;

	if (!rh_reg_write(rh, RH_REG_CONFIG_CTRL, 0))
		return (false);

	for (;;) {
		if (!rh_reg_read(rh, RH_REG_CONFIG_STATUS, &status))
			return (false);
		if (RH_REG_CONFIG_STATUS_GET_STATE(status) ==
		    RH_REG_CONFIG_STATUS_STATE_IDLE) {
			return (true);
		}
		if (gethrtime() > deadline)
			break;
		(void) usleep(NICINIT_POLL_USEC);
	}

	warnx("the NIC has not returned to reset after %u seconds "
	    "(configuration status 0x%x)", RH_CONFIG_MARGIN_SEC, status);
	return (false);
}

/*
 * Take the NIC down. Its slots are disconnected first so that the drivers
 * detach from the devices in them before the devices are removed.
 */
static bool
rh_take_down(const nicinit_t *ni, const redhawk_t *rh)
{
	for (uint_t i = 0; i < ni->ni_naps; i++) {
		nicinit_ap_state_t state;

		if (!nicinit_ap_state(ni->ni_aps[i], &state))
			return (false);
		if (state != NICINIT_AP_CONNECTED &&
		    state != NICINIT_AP_CONFIGURED) {
			continue;
		}
		if (!nicinit_ap_disconnect(ni->ni_aps[i]))
			return (false);
	}

	nicinit_verbose("Putting the NIC into reset");
	return (rh_reset(rh));
}

/*
 * Wait for the FPGA to finish configuring the NIC, returning the final status.
 * The FPGA has a timeout for this and we wait a little longer than that for
 * it to report a result.
 */
static bool
rh_config_wait(const redhawk_t *rh, uint32_t *statusp)
{
	hrtime_t deadline;
	uint32_t ms;

	if (!rh_reg_read(rh, RH_REG_CONFIG_TIMEOUT, &ms))
		return (false);
	deadline = gethrtime() + MSEC2NSEC(ms) +
	    SEC2NSEC(RH_CONFIG_MARGIN_SEC);

	for (;;) {
		if (!rh_reg_read(rh, RH_REG_CONFIG_STATUS, statusp))
			return (false);
		switch (RH_REG_CONFIG_STATUS_GET_STATE(*statusp)) {
		case RH_REG_CONFIG_STATUS_STATE_DONE:
		case RH_REG_CONFIG_STATUS_STATE_FAILED:
			return (true);
		default:
			break;
		}
		if (gethrtime() > deadline)
			break;
		(void) usleep(NICINIT_POLL_USEC);
	}

	warnx("the FPGA has not finished configuring the NIC %u seconds after "
	    "its timeout of %ums (configuration status 0x%x)",
	    RH_CONFIG_MARGIN_SEC, ms, *statusp);
	return (false);
}

/*
 * Wait for each of the NIC's slots to be configured, which the hotplug
 * framework does automatically once presence is asserted.
 */
static bool
rh_wait(nicinit_t *ni)
{
	return (nicinit_ap_wait(ni, RH_AP_TIMEOUT_SEC));
}

static bool
rh_args(const nicinit_t *ni, bool offline)
{
	if (ni->ni_dpio != NULL || ni->ni_sromfile != NULL ||
	    ni->ni_pci_ss_did_set) {
		warnx("-D, -s and -P do not apply to a Redhawk");
		return (false);
	}

	if (!offline && (ni->ni_fwfile == NULL || ni->ni_hashlen == 0)) {
		warnx("-f and -L are mandatory when bringing a Redhawk online");
		return (false);
	}

	return (true);
}

/*
 * Determine whether the NIC has already been configured. If it has, and from
 * an image that matches ours, all that remains is to check that its slots
 * are configured. If the image does not match, or the FPGA has no digest for
 * it, then the NIC is taken down so that it can be verified and programmed in
 * the usual way.
 */
static bool
rh_online(nicinit_t *ni, bool *onlinep)
{
	redhawk_t *rh = rh_state(ni);
	uint8_t digest[SHA3_256_DIGEST_LENGTH];
	char got[RH_DIGEST_STRLEN], want[RH_DIGEST_STRLEN];
	uint32_t status;
	bool none;

	if (rh == NULL)
		return (false);

	*onlinep = false;

	if (!rh_reg_read(rh, RH_REG_CONFIG_STATUS, &status))
		return (false);

	if (RH_REG_CONFIG_STATUS_GET_STATE(status) ==
	    RH_REG_CONFIG_STATUS_STATE_IN_PROGRESS) {
		nicinit_verbose("The NIC is already being configured");
		if (!rh_config_wait(rh, &status))
			return (false);
	}

	switch (RH_REG_CONFIG_STATUS_GET_STATE(status)) {
	case RH_REG_CONFIG_STATUS_STATE_IDLE:
		return (true);
	case RH_REG_CONFIG_STATUS_STATE_FAILED:
		nicinit_verbose("An earlier attempt to configure the NIC "
		    "failed (configuration status 0x%x)", status);
		return (rh_reset(rh));
	default:
		break;
	}

	nicinit_verbose("The NIC has already been configured");

	if (!rh_load_image(ni, rh)) {
		nicinit_fail_class(ni, NICINIT_CLASS_PREPARE);
		return (false);
	}

	if (!rh_measure(ni, rh, digest, &none))
		return (false);

	if (none) {
		warnx("the NIC has been configured but the FPGA has no "
		    "measurement of the image that it was configured from, "
		    "taking it down");
		return (rh_take_down(ni, rh));
	}

	if (memcmp(digest, rh->rh_want, sizeof (digest)) == 0) {
		/*
		 * No device is about to be inserted in the slots, so one
		 * that is not configured must be configured explicitly.
		 */
		*onlinep = true;
		if (!nicinit_ap_wait(ni, 0)) {
			nicinit_fail_class(ni, NICINIT_CLASS_NOT_CONFIGURED);
			return (false);
		}
		return (true);
	}

	warnx("the NIC was configured from an image that does not match "
	    "(measured %s, expected %s), taking it down",
	    rh_digest_str(digest, got), rh_digest_str(rh->rh_want, want));
	return (rh_take_down(ni, rh));
}

static bool
rh_prepare(nicinit_t *ni)
{
	return (rh_load_image(ni, ni->ni_priv));
}

static bool
rh_verify(nicinit_t *ni, bool *matchp)
{
	redhawk_t *rh = ni->ni_priv;
	uint8_t digest[SHA3_256_DIGEST_LENGTH];
	char got[RH_DIGEST_STRLEN], want[RH_DIGEST_STRLEN];

	nicinit_verbose("Verifying flash");

	if (!rh_measure(ni, rh, digest, NULL))
		return (false);

	*matchp = memcmp(digest, rh->rh_want, sizeof (digest)) == 0;
	if (*matchp) {
		nicinit_verbose("Flash digest matches: %s",
		    rh_digest_str(digest, got));
	} else {
		(void) printf(
		    "Flash digest mismatch: measured %s, expected %s\n",
		    rh_digest_str(digest, got),
		    rh_digest_str(rh->rh_want, want));
	}

	return (true);
}

/*
 * Choose the erase block size to use. The smallest advertised size minimises
 * the amount that is written when only part of the flash needs to change.
 */
static uint32_t
rh_erase_size(const espi_flash_info_t *info)
{
	if (info->efi_erase_sizes == 0)
		return (RH_DEFAULT_ERASE);

	for (uint_t i = 0; i < ARRAY_SIZE(rh_erase_sizes); i++) {
		if ((info->efi_erase_sizes & (rh_erase_sizes[i] >> 10)) != 0)
			return (rh_erase_sizes[i]);
	}

	warnx("flash advertises no usable erase block size (0x%x), assuming "
	    "%u bytes", info->efi_erase_sizes, RH_DEFAULT_ERASE);
	return (RH_DEFAULT_ERASE);
}

static bool
rh_all_erased(const uint8_t *buf, size_t len)
{
	for (size_t i = 0; i < len; i++) {
		if (buf[i] != 0xff)
			return (false);
	}
	return (true);
}

/*
 * Write our image to the start of the measured range of the flash and erase
 * the rest of the range. This is done one erase block at a time, and blocks
 * that already have the right contents are skipped. The device is opened
 * exclusively so that no other process can use the flash while it is being
 * changed.
 */
static bool
rh_program(nicinit_t *ni)
{
	redhawk_t *rh = ni->ni_priv;
	const uint64_t range = ni->ni_hashlen;
	espi_flash_info_t info;
	uint_t nwritten = 0, nerased = 0, nsame = 0;
	uint8_t *buf = NULL, *want = NULL;
	bool ret = false;
	uint32_t ebs;
	int fd;

	/*
	 * The FPGA's digest is out of date from this point on, whether or not
	 * programming succeeds.
	 */
	rh->rh_stale = true;

	if ((fd = open(rh->rh_flashpath, O_RDWR | O_EXCL)) < 0) {
		warn("failed to open %s", rh->rh_flashpath);
		return (false);
	}

	if (ioctl(fd, ESPI_FLASH_IOC_INFO, &info) != 0) {
		warn("failed to get flash information from %s",
		    rh->rh_flashpath);
		goto out;
	}

	ebs = rh_erase_size(&info);
	if (info.efi_size != 0 && range > info.efi_size) {
		warnx("the measured length of %" PRIu64 " bytes is larger "
		    "than the flash, which is %" PRIu64 " bytes", range,
		    info.efi_size);
		goto out;
	}
	if (range % ebs != 0) {
		warnx("the measured length of %" PRIu64 " bytes is not a "
		    "multiple of the flash erase block size of %u bytes",
		    range, ebs);
		goto out;
	}

	if ((buf = malloc(ebs)) == NULL || (want = malloc(ebs)) == NULL) {
		warn("failed to allocate memory");
		goto out;
	}

	nicinit_verbose(
	    "Programming %" PRIu64 " bytes of flash in %u byte blocks",
	    range, ebs);

	for (uint64_t off = 0; off < range; off += ebs) {
		espi_flash_erase_t erase = {
			.efe_offset = off,
			.efe_length = ebs
		};
		size_t imglen = 0;

		if (off != 0 && off % RH_PROGRESS_BYTES == 0) {
			nicinit_verbose("    0x%08" PRIx64 ": %u written, "
			    "%u erased, %u unchanged", off, nwritten, nerased,
			    nsame);
		}

		if (off < rh->rh_imglen) {
			imglen = MIN(rh->rh_imglen - off, ebs);
			(void) memcpy(want, rh->rh_img + off, imglen);
		}
		(void) memset(want + imglen, 0xff, ebs - imglen);

		if (!rh_pread(fd, buf, ebs, off)) {
			warn("failed to read %u bytes of flash at offset 0x%"
			    PRIx64, ebs, off);
			goto out;
		}

		if (memcmp(buf, want, ebs) == 0) {
			nsame++;
			continue;
		}

		if (ioctl(fd, ESPI_FLASH_IOC_ERASE, &erase) != 0) {
			warn("failed to erase %u bytes of flash at offset 0x%"
			    PRIx64, ebs, off);
			goto out;
		}

		if (rh_all_erased(want, ebs)) {
			nerased++;
		} else if (!rh_pwrite(fd, want, ebs, off)) {
			warn("failed to write %u bytes of flash at offset 0x%"
			    PRIx64, ebs, off);
			goto out;
		} else {
			nwritten++;
		}

		if (!rh_pread(fd, buf, ebs, off)) {
			warn("failed to read back %u bytes of flash at offset "
			    "0x%" PRIx64, ebs, off);
			goto out;
		}

		if (memcmp(buf, want, ebs) != 0) {
			warnx("flash at offset 0x%" PRIx64 " does not hold "
			    "what was just written to it", off);
			goto out;
		}
	}

	(void) printf(
	    "Flash programmed: %u blocks written, %u erased, %u unchanged\n",
	    nwritten, nerased, nsame);
	ret = true;

out:
	(void) close(fd);
	free(buf);
	free(want);
	return (ret);
}

/*
 * Have the FPGA configure the NIC from the image that it has measured.
 */
static bool
rh_release(nicinit_t *ni)
{
	const redhawk_t *rh = ni->ni_priv;
	uint32_t status;

	nicinit_verbose("Configuring the NIC");

	if (!rh_reg_write(rh, RH_REG_CONFIG_CTRL,
	    RH_REG_CONFIG_CTRL_MISSION_MODE_EN)) {
		return (false);
	}

	if (!rh_config_wait(rh, &status)) {
		nicinit_fail_class(ni, NICINIT_CLASS_CONFIG_TIMEOUT);
		(void) rh_reset(rh);
		return (false);
	}

	nicinit_detail_uint32(ni, "config-status", status);

	if (RH_REG_CONFIG_STATUS_GET_STATE(status) ==
	    RH_REG_CONFIG_STATUS_STATE_DONE) {
		nicinit_verbose("The NIC has been configured");
		return (true);
	}

	if (RH_REG_CONFIG_STATUS_GET_TIMED_OUT(status) != 0) {
		nicinit_fail_class(ni, NICINIT_CLASS_CONFIG_TIMEOUT);
		warnx("the NIC did not report that it had been configured "
		    "before the FPGA gave up on it (configuration status 0x%x)",
		    status);
	} else {
		nicinit_fail_class(ni, NICINIT_CLASS_CONFIG_ERROR);
		warnx("the NIC reported an error while it was being "
		    "configured (configuration status 0x%x)", status);
	}

	/*
	 * Return the NIC to reset, which also makes the flash accessible to
	 * us again so that it can be examined.
	 */
	(void) rh_reset(rh);
	return (false);
}

static bool
rh_offline(nicinit_t *ni)
{
	const redhawk_t *rh = rh_state(ni);

	if (rh == NULL)
		return (false);

	return (rh_take_down(ni, rh));
}

static void
rh_fini(nicinit_t *ni)
{
	redhawk_t *rh = ni->ni_priv;

	if (rh == NULL)
		return;

	if (rh->rh_img != NULL)
		(void) munmap((void *)rh->rh_img, rh->rh_imglen);
	if (rh->rh_ctlfd >= 0)
		(void) close(rh->rh_ctlfd);
	free(rh->rh_ctlpath);
	free(rh->rh_flashpath);
	free(rh);
	ni->ni_priv = NULL;
}

const nicinit_ops_t nicinit_redhawk_ops = {
	.no_name = "redhawk",
	.no_driver = RH_DRIVER,
	.no_args = rh_args,
	.no_online = rh_online,
	.no_prepare = rh_prepare,
	.no_verify = rh_verify,
	.no_program = rh_program,
	.no_release = rh_release,
	.no_wait = rh_wait,
	.no_offline = rh_offline,
	.no_fini = rh_fini
};
