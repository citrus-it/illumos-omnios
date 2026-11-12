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
 * This utility brings the NIC on an Oxide board online after verifying that
 * its firmware is what we expect. If there is a mismatch, then it will program
 * the correct firmware before verifying again and moving on.
 *
 * The sequence of steps is common to all types of NIC and is implemented by
 * nicinit_online(). The steps themselves differ and are implemented by a set
 * of operations for each flavour of NIC. See nicinit.h for a description of
 * those. The flavours are:
 *
 *   t6		A Chelsio T6, which starts out in manufacturing mode and is
 *		switched to mission mode once its SROM and flash have been
 *		verified. See nicinit_t6.c.
 *
 *   redhawk	A Versal device which the sequencer FPGA measures the boot
 *		image of and then keeps in reset until we have checked the
 *		measurement and enabled it. See nicinit_redhawk.c.
 *
 * Each problem is written to standard error, which is the service log when we
 * are run by smf(7), to the system log, and to the console. Each also
 * generates an ireport of class ireport.oxide.nic.<problem>. Its payload is a
 * description of the NIC, the message and any details that the flavour has
 * added.
 */

#include <config_admin.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <libdevinfo.h>
#include <libgen.h>
#include <libipcc.h>
#include <libnvpair.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <fm/libfmevent.h>
#include <sys/sysmacros.h>
#include <sys/types.h>

#include "nicinit.h"

/*
 * The primary classification of the events that we publish. With the Oxide
 * ireport ruleset this results in classes that begin ireport.oxide.nic.
 */
#define	NICINIT_EVENT_CLASS	"nic"

#define	NICINIT_CONSOLE		"/dev/console"

/*
 * Where firmware that is specific to a platform is found.
 */
#define	NICINIT_FIRMWARE_FMT	"/platform/%s/firmware/%s"

/*
 * What we know about the NIC on each platform. A firmware file name that is
 * not an absolute path is relative to the platform's firmware directory.
 * np_link_gen and np_link_width are the PCIe generation and number of lanes
 * that the link to each of the NIC's devices is expected to run at once the
 * NIC is online.
 */
typedef struct nicinit_platform {
	const char *np_name;
	const nicinit_ops_t *np_ops;
	const char *np_aps[NICINIT_MAX_AP];
	const char *np_srom;
	const char *np_fw;
	uint16_t np_pci_ss_did;
	uint64_t np_hashlen;
	uint_t np_link_gen;
	uint_t np_link_width;
} nicinit_platform_t;

static const nicinit_platform_t nicinit_platforms[] = { {
	.np_name = "Oxide,Gimlet",
	.np_ops = &nicinit_t6_ops,
	.np_aps = { "pcie16" },
	.np_srom = "t6srom.bin",
	.np_fw = "/kernel/firmware/cxgbe/t6fw.bin",
	.np_pci_ss_did = 0xfff9,
	.np_link_gen = 3,
	.np_link_width = 16
}, {
	.np_name = "Oxide,Cosmo",
	.np_ops = &nicinit_t6_ops,
	.np_aps = { "pcie16" },
	.np_srom = "t6srom.bin",
	.np_fw = "/kernel/firmware/cxgbe/t6fw.bin",
	.np_pci_ss_did = 0xfff8,
	.np_link_gen = 3,
	.np_link_width = 16
}, {
	.np_name = "Oxide,Metro",
	.np_ops = &nicinit_redhawk_ops,
	.np_aps = { "pcie20", "pcie21" },
	.np_fw = "redhawk.pdi",
	/*
	 * XXX This is the number of bytes of flash that the FPGA measures and
	 * should be read from the FPGA once it provides it.
	 */
	.np_hashlen = 32 * 1024 * 1024,
	.np_link_gen = 5,
	.np_link_width = 8
} };

/*
 * PCIe link speeds in bits per second, as found in the devinfo properties of a
 * device, indexed by generation.
 */
static const int64_t nicinit_link_speeds[] = {
	[1] = 2500000000LL,
	[2] = 5000000000LL,
	[3] = 8000000000LL,
	[4] = 16000000000LL,
	[5] = 32000000000LL,
	[6] = 64000000000LL
};

static const char *progname;
static libipcc_handle_t *nicinit_lih;
static bool verbose;

static void usage(const char *, ...) __NORETURN __PRINTFLIKE(1);

static const nicinit_ops_t *nicinit_flavours[] = {
	&nicinit_t6_ops,
	&nicinit_redhawk_ops
};

void
nicinit_verbose(const char *fmt, ...)
{
	va_list va;

	if (!verbose)
		return;

	va_start(va, fmt);
	(void) vfprintf(stdout, fmt, va);
	(void) fprintf(stdout, "\n");
	(void) fflush(stdout);
	va_end(va);
}

void
nicinit_fail_class(nicinit_t *ni, const char *class)
{
	ni->ni_fail_class = class;
}

void
nicinit_detail_string(nicinit_t *ni, const char *name, const char *val)
{
	if (nvlist_add_string(ni->ni_detail, name, val) != 0)
		warnx("failed to record %s for reporting", name);
}

void
nicinit_detail_uint32(nicinit_t *ni, const char *name, uint32_t val)
{
	if (nvlist_add_uint32(ni->ni_detail, name, val) != 0)
		warnx("failed to record %s for reporting", name);
}

/*
 * Publish an event about the NIC. The payload is the details that the flavour
 * has added so far, a description of the NIC and the message.
 */
static void
nicinit_publish(const nicinit_t *ni, const char *class, fmev_pri_t pri,
    const char *msg)
{
	nvlist_t *nvl;

	if (nvlist_dup(ni->ni_detail, &nvl, 0) != 0) {
		warnx("failed to construct the payload of a %s event", class);
		return;
	}

	if (nvlist_add_string(nvl, "nic-type", ni->ni_ops->no_name) != 0 ||
	    nvlist_add_string(nvl, "platform", ni->ni_platform) != 0 ||
	    nvlist_add_string_array(nvl, "attachment-points",
	    (char **)ni->ni_aps, ni->ni_naps) != 0 ||
	    nvlist_add_string(nvl, "message", msg) != 0) {
		warnx("failed to construct the payload of a %s event", class);
		nvlist_free(nvl);
		return;
	}

	/*
	 * libfmevent frees the payload if the event is published. We must
	 * free it if it is not.
	 */
	if (fmev_rspublish_nvl(FMEV_RULESET_OXIDE_IREPORT, NICINIT_EVENT_CLASS,
	    class, pri, nvl) != FMEV_SUCCESS) {
		warnx("failed to publish a %s event: %s", class,
		    fmev_strerror(fmev_errno));
		nvlist_free(nvl);
	}
}

/*
 * Report a problem with the NIC to standard error, the system log and the
 * console, and publish an event for it.
 */
static void __PRINTFLIKE(3)
nicinit_problem(const nicinit_t *ni, const char *class, const char *fmt, ...)
{
	static const char rule[] = "########################################"
	    "######################################";
	char *msg;
	va_list va;
	FILE *cons;

	va_start(va, fmt);
	if (vasprintf(&msg, fmt, va) < 0) {
		va_end(va);
		warn("failed to format a message about a %s problem", class);
		return;
	}
	va_end(va);

	warnx("%s", msg);
	syslog(LOG_NOTICE, "%s", msg);

	if ((cons = fopen(NICINIT_CONSOLE, "w")) != NULL) {
		(void) fprintf(cons,
		    "\n%s\n#### NOTE: NIC PROBLEM\n#### %s\n%s\n", rule, msg,
		    rule);
		(void) fclose(cons);
	}

	nicinit_publish(ni, class, FMEV_HIPRI, msg);
	free(msg);
}

/*
 * Report the failure of one of the flavour's operations. The operation will
 * already have written the details to standard error, and may have set a more
 * specific class of problem than the default for the step.
 */
static void __PRINTFLIKE(3)
nicinit_failed(nicinit_t *ni, const char *class, const char *fmt, ...)
{
	char *msg;
	va_list va;

	if (ni->ni_fail_class != NULL) {
		class = ni->ni_fail_class;
		ni->ni_fail_class = NULL;
	}

	va_start(va, fmt);
	if (vasprintf(&msg, fmt, va) < 0)
		msg = NULL;
	va_end(va);

	nicinit_problem(ni, class, "%s", msg != NULL ? msg : "NIC failure");
	free(msg);
}

static int
cfg_confirm(void *arg __unused, const char *msg)
{
	nicinit_verbose("config confirm: %s", msg);
	return (1);
}

static int
cfg_msg(void *arg __unused, const char *msg)
{
	nicinit_verbose("config message: %s", msg);
	return (1);
}

static void
cfg_warn(const char *what, const char *ap, cfga_err_t cfgerr, char *errstr)
{
	const char *ep = config_strerror(cfgerr);

	if (ep == NULL)
		ep = "configuration administration unknown error";
	if (errstr != NULL && *errstr != '\0')
		warnx("failed to %s %s: %s: %s", what, ap, ep, errstr);
	else
		warnx("failed to %s %s: %s", what, ap, ep);
}

/*
 * Change the state of an attachment point. Only the commands that we use are
 * known here.
 */
static bool
nicinit_ap_change(const char *ap, cfga_cmd_t cmd)
{
	char * const aplist[] = { (char *)ap };
	char *errstr = NULL;
	const char *what;
	cfga_err_t cfgerr;
	struct cfga_confirm conf = {
		.confirm = cfg_confirm
	};
	struct cfga_msg msg = {
		.message_routine = cfg_msg
	};

	switch (cmd) {
	case CFGA_CMD_DISCONNECT:
		what = "disconnect";
		break;
	case CFGA_CMD_CONFIGURE:
		what = "configure";
		break;
	default:
		warnx("unexpected attachment point command %d for %s", cmd, ap);
		return (false);
	}

	nicinit_verbose("    %s %s", what, ap);

	cfgerr = config_change_state(cmd, 1, aplist, NULL, &conf, &msg,
	    &errstr, CFGA_FLAG_FORCE | CFGA_FLAG_VERBOSE);
	if (cfgerr != CFGA_OK)
		cfg_warn(what, ap, cfgerr, errstr);
	free(errstr);

	return (cfgerr == CFGA_OK);
}

bool
nicinit_ap_disconnect(const char *ap)
{
	return (nicinit_ap_change(ap, CFGA_CMD_DISCONNECT));
}

bool
nicinit_ap_configure(const char *ap)
{
	return (nicinit_ap_change(ap, CFGA_CMD_CONFIGURE));
}

bool
nicinit_ap_state(const char *ap, nicinit_ap_state_t *statep)
{
	char * const aplist[] = { (char *)ap };
	cfga_list_data_t *list = NULL;
	char *errstr = NULL;
	cfga_err_t cfgerr;
	int nlist = 0;

	cfgerr = config_list_ext(1, aplist, &list, &nlist, NULL, NULL,
	    &errstr, 0);
	if (cfgerr != CFGA_OK) {
		cfg_warn("get the state of", ap, cfgerr, errstr);
		free(errstr);
		return (false);
	}
	free(errstr);

	if (nlist != 1) {
		warnx("expected one attachment point for %s, found %d", ap,
		    nlist);
		free(list);
		return (false);
	}

	if (list[0].ap_o_state == CFGA_STAT_CONFIGURED)
		*statep = NICINIT_AP_CONFIGURED;
	else if (list[0].ap_r_state == CFGA_STAT_CONNECTED)
		*statep = NICINIT_AP_CONNECTED;
	else if (list[0].ap_r_state == CFGA_STAT_DISCONNECTED)
		*statep = NICINIT_AP_DISCONNECTED;
	else
		*statep = NICINIT_AP_EMPTY;

	free(list);
	return (true);
}

/*
 * Wait for each of the NIC's attachment points to be configured, for up to
 * the given number of seconds. The hotplug framework configures an attachment
 * point automatically when a device is inserted. It does not do so for a
 * device that was already present and has been disconnected, by us or by an
 * operator. An attachment point that is still occupied but not configured when
 * the time is up is therefore configured explicitly. A caller that is not
 * expecting a device to be inserted can pass a timeout of 0 to skip the wait.
 */
bool
nicinit_ap_wait(const nicinit_t *ni, uint_t timeout)
{
	const hrtime_t deadline = gethrtime() + SEC2NSEC(timeout);

	for (uint_t i = 0; i < ni->ni_naps; i++) {
		const char *ap = ni->ni_aps[i];
		nicinit_ap_state_t state;

		nicinit_verbose("Waiting for %s to be configured", ap);

		for (;;) {
			if (!nicinit_ap_state(ap, &state))
				return (false);
			if (state == NICINIT_AP_CONFIGURED)
				break;
			if (gethrtime() > deadline)
				break;
			(void) usleep(NICINIT_POLL_USEC);
		}

		if (state == NICINIT_AP_CONFIGURED)
			continue;

		if (state == NICINIT_AP_EMPTY) {
			warnx("%s is still empty after waiting %u seconds for "
			    "it to be configured", ap, timeout);
			return (false);
		}

		nicinit_verbose("%s is %s, configuring it", ap,
		    nicinit_ap_state_name(state));
		if (!nicinit_ap_configure(ap))
			return (false);
		if (!nicinit_ap_state(ap, &state))
			return (false);
		if (state != NICINIT_AP_CONFIGURED) {
			warnx("%s is %s after being configured", ap,
			    nicinit_ap_state_name(state));
			return (false);
		}
	}

	return (true);
}

const char *
nicinit_ap_state_name(nicinit_ap_state_t state)
{
	switch (state) {
	case NICINIT_AP_EMPTY:
		return ("empty");
	case NICINIT_AP_DISCONNECTED:
		return ("disconnected");
	case NICINIT_AP_CONNECTED:
		return ("connected");
	case NICINIT_AP_CONFIGURED:
		return ("configured");
	}
	return ("unknown");
}

/*
 * Build the path to a minor node of a device. This returns NULL if the device
 * has no such minor node, which is not reported, or if the path could not be
 * constructed, which is. The caller must free the path.
 */
char *
nicinit_minor_path(di_node_t node, const char *name)
{
	di_minor_t minor = DI_MINOR_NIL;
	char *mpath, *path;

	while ((minor = di_minor_next(node, minor)) != DI_MINOR_NIL) {
		if (strcmp(di_minor_name(minor), name) == 0)
			break;
	}
	if (minor == DI_MINOR_NIL)
		return (NULL);

	if ((mpath = di_devfs_minor_path(minor)) == NULL) {
		warn("failed to get the path of the %s minor node of %s%d",
		    name, di_driver_name(node), di_instance(node));
		return (NULL);
	}
	if (asprintf(&path, "/devices%s", mpath) < 0) {
		warn("failed to construct a device path");
		path = NULL;
	}
	di_devfs_path_free(mpath);

	return (path);
}

/*
 * Report a failure of a libipcc function along with the library's description
 * of the error.
 */
static void __PRINTFLIKE(2)
nicinit_ipcc_err(libipcc_handle_t *lih, const char *fmt, ...)
{
	va_list va;

	(void) fprintf(stderr, "%s: ", getprogname());
	va_start(va, fmt);
	(void) vfprintf(stderr, fmt, va);
	va_end(va);

	(void) fprintf(stderr, ": %s: %s (libipcc: 0x%x, sys: %d)\n",
	    libipcc_errmsg(lih), libipcc_strerror(libipcc_err(lih)),
	    libipcc_err(lih), libipcc_syserr(lih));
}

/*
 * Return the handle through which we talk to the service processor, creating
 * it on first use. It is released by nicinit_ipcc_fini().
 */
static libipcc_handle_t *
nicinit_ipcc_handle(void)
{
	char errmsg[LIBIPCC_ERR_LEN];

	if (nicinit_lih != NULL)
		return (nicinit_lih);

	if (!libipcc_init(&nicinit_lih, NULL, NULL, errmsg, sizeof (errmsg))) {
		warnx("Failed to init libipcc handle: %s", errmsg);
		nicinit_lih = NULL;
	}

	return (nicinit_lih);
}

static void
nicinit_ipcc_fini(void)
{
	if (nicinit_lih != NULL) {
		libipcc_fini(nicinit_lih);
		nicinit_lih = NULL;
	}
}

/*
 * Retrieve the base MAC address assigned by the service processor for use by
 * the NIC. The NIC needs at least count addresses that are stride apart, and
 * what the service processor has is checked against that.
 */
bool
nicinit_macaddr(uint16_t count, uint8_t stride, ether_addr_t *macp)
{
	libipcc_handle_t *lih = nicinit_ipcc_handle();
	char macstr[ETHERADDRSTRL];
	const struct ether_addr *base;
	libipcc_mac_t *mac;
	uint8_t sp_stride;
	uint16_t sp_count;

	if (lih == NULL)
		return (false);

	nicinit_verbose("Retrieving MAC addresses from SP");

	if (!libipcc_mac_nic(lih, &mac)) {
		nicinit_ipcc_err(lih, "could not retrieve MACs from SP");
		return (false);
	}

	base = libipcc_mac_addr(mac);
	sp_count = libipcc_mac_count(mac);
	sp_stride = libipcc_mac_stride(mac);

	if (ether_ntoa_r(base, macstr) == NULL) {
		warnx("Could not convert MAC address to string");
		libipcc_mac_free(mac);
		return (false);
	}

	(void) memcpy(macp, base, sizeof (*base));
	libipcc_mac_free(mac);

	nicinit_verbose("    Base:   %s", macstr);
	nicinit_verbose("    Count:  %x", sp_count);
	nicinit_verbose("    Stride: %x", sp_stride);

	if (strcmp(macstr, "0:0:0:0:0:0") == 0) {
		/*
		 * This can occur if the SP is unable to retrieve the MAC
		 * address from the VPD.
		 */
		warnx("All zero MAC address from SP - '%s'", macstr);
		return (false);
	}

	if (sp_count < count) {
		warnx("too few MAC addresses from SP, got %d, need at least %d",
		    sp_count, count);
		return (false);
	}
	if (sp_stride != stride) {
		warnx("MAC address stride incorrect, got %d, need %d",
		    sp_stride, stride);
		return (false);
	}

	return (true);
}

/*
 * The kernel guarantees that the ident strings are NUL terminated, but not
 * much else.
 */
static char *
cleanstr(const char *str)
{
	const char *end, *cp;
	size_t dstsize;
	char *dst;

	end = str + strlen(str);

	while (str < end && isspace((uchar_t)*str))
		str++;
	while (str < end && isspace((uchar_t)*(end - 1)))
		end--;

	if (str >= end) {
		errno = EOVERFLOW;
		return (NULL);
	}

	dstsize = end - str + 1;
	dst = calloc(1, dstsize);
	if (dst == NULL)
		return (NULL);

	cp = str;
	for (size_t i = 0; i < dstsize - 1; i++) {
		char c;

		if (cp >= end)
			break;
		c = *cp;
		if (isspace((uchar_t)c) || !isprint((uchar_t)c))
			dst[i] = '-';
		else
			dst[i] = c;
		cp++;
	}

	return (dst);
}

/*
 * Retrieve the model and serial number of the system from the service
 * processor. The caller must free both strings.
 */
bool
nicinit_ident(char **modelp, char **serialp)
{
	libipcc_handle_t *lih = nicinit_ipcc_handle();
	libipcc_ident_t *ident;
	char *model, *serial;

	if (lih == NULL)
		return (false);

	nicinit_verbose("Retrieving ident from SP");

	if (!libipcc_ident(lih, &ident)) {
		nicinit_ipcc_err(lih, "could not retrieve ident from SP");
		return (false);
	}

	model = cleanstr((const char *)libipcc_ident_model(ident));
	if (model == NULL) {
		warn("could not clean ident model string");
		libipcc_ident_free(ident);
		return (false);
	}

	serial = cleanstr((const char *)libipcc_ident_serial(ident));
	if (serial == NULL) {
		warn("could not clean ident serial string");
		free(model);
		libipcc_ident_free(ident);
		return (false);
	}

	libipcc_ident_free(ident);

	nicinit_verbose("       Model: '%s'", model);
	nicinit_verbose("      Serial: '%s'", serial);

	*modelp = model;
	*serialp = serial;

	return (true);
}

/*
 * Check that the PCIe link to each device that a driver is attached to is
 * running at the expected speed and width. A link that is not is reported as
 * a problem but is not treated as a failure. If the driver is not attached to
 * any device then no check is made. The caller must determine whether that is
 * a problem, as it depends on the state that the NIC should be in.
 */
void
nicinit_link_check(nicinit_t *ni, const char *driver, uint_t gen, uint_t width)
{
	di_node_t root, node;

	root = di_init_driver(driver, DINFOSUBTREE | DINFOPROP);
	if (root == DI_NODE_NIL) {
		nicinit_verbose("No %s devices to check the links of", driver);
		return;
	}

	for (node = di_drv_first_node(driver, root); node != DI_NODE_NIL;
	    node = di_drv_next_node(node)) {
		const int inst = di_instance(node);
		int64_t *speed;
		int *lanes;

		if (di_prop_lookup_int64(DDI_DEV_T_ANY, node,
		    "pcie-link-current-speed", &speed) != 1 ||
		    di_prop_lookup_ints(DDI_DEV_T_ANY, node,
		    "pcie-link-current-width", &lanes) != 1) {
			nicinit_verbose("%s%d has no PCIe link properties",
			    driver, inst);
			continue;
		}

		nicinit_verbose("%s%d link is %" PRId64 " bits/s x%d", driver,
		    inst, *speed, *lanes);

		if (*speed != nicinit_link_speeds[gen] ||
		    *lanes != (int)width) {
			nicinit_problem(ni, NICINIT_CLASS_LINK,
			    "%s%d link is %" PRId64 " bits/s x%d, expected "
			    "Gen %u (%" PRId64 " bits/s) x%u", driver, inst,
			    *speed, *lanes, gen, nicinit_link_speeds[gen],
			    width);
		}
	}

	di_fini(root);
}

static void
show_help(void)
{
	(void) fprintf(stderr,
	    "Usage:\n"
	    "  To bring the NIC online:\n"
	    "        %1$s [-v] [options]\n"
	    "  To take the NIC back offline:\n"
	    "        %1$s [-v] -M [options]\n"
	    "  The NIC is described by what is known about this platform. A\n"
	    "  T6 also requires that -D is specified. The remaining options\n"
	    "  can be used to override that knowledge:\n"
	    "        -A <attach>          Specify an attachment point\n"
	    "        -D <dpio name>       Specify the T6 mode DPIO name\n"
	    "        -f <fw file>         Specify the flash firmware file\n"
	    "        -L <length>          Specify the number of bytes of\n"
	    "                             flash that are measured\n"
	    "        -M                   Take the NIC offline\n"
	    "        -P <id>              Specify the PCI sub-system ID\n"
	    "        -s <srom file>       Specify the SROM firmware file\n"
	    "        -T <type>            Specify the type of NIC, t6 or\n"
	    "                             redhawk\n"
	    "        -v                   Enable verbose output\n",
	    progname);
}

static void
usage(const char *fmt, ...)
{
	va_list va;

	va_start(va, fmt);
	(void) vfprintf(stderr, fmt, va);
	va_end(va);
	(void) fprintf(stderr, "\n");

	show_help();
	exit(EXIT_USAGE);
}

static const nicinit_ops_t *
nicinit_flavour(const char *name)
{
	for (uint_t i = 0; i < ARRAY_SIZE(nicinit_flavours); i++) {
		if (strcmp(name, nicinit_flavours[i]->no_name) == 0)
			return (nicinit_flavours[i]);
	}

	usage("unknown NIC type: %s", name);
}

/*
 * Determine the name of the platform that we are running on, which is that of
 * the root of the device tree.
 */
static char *
nicinit_platform_name(void)
{
	di_node_t root;
	char *name;

	if ((root = di_init("/", DINFOCPYONE)) == DI_NODE_NIL)
		err(EXIT_FAILURE, "failed to take a devinfo snapshot");

	if ((name = strdup(di_node_name(root))) == NULL)
		err(EXIT_FAILURE, "failed to allocate memory");

	di_fini(root);
	return (name);
}

static const char *
nicinit_firmware_path(const nicinit_t *ni, const char *file)
{
	char *path;

	if (file == NULL || file[0] == '/')
		return (file);

	if (asprintf(&path, NICINIT_FIRMWARE_FMT, ni->ni_platform, file) < 0)
		err(EXIT_FAILURE, "failed to allocate memory");

	return (path);
}

/*
 * Fill in whatever has not been specified with options from what we know about
 * the platform that we are on. If the platform is not one that we know about,
 * then at least the type of NIC must have been specified.
 */
static void
nicinit_platform_defaults(nicinit_t *ni)
{
	const nicinit_platform_t *np = NULL;

	for (uint_t i = 0; i < ARRAY_SIZE(nicinit_platforms); i++) {
		const nicinit_platform_t *p = &nicinit_platforms[i];

		if (strcmp(ni->ni_platform, p->np_name) == 0) {
			np = p;
			break;
		}
	}

	if (np == NULL) {
		if (ni->ni_ops != NULL)
			return;
		warnx("there is no known NIC for %s", ni->ni_platform);
		exit(EXIT_UNSUPPORTED);
	}

	if (ni->ni_ops == NULL)
		ni->ni_ops = np->np_ops;

	/*
	 * Everything else that we know only makes sense for the type of NIC
	 * that the platform has.
	 */
	if (ni->ni_ops != np->np_ops)
		return;

	if (ni->ni_naps == 0) {
		while (ni->ni_naps < NICINIT_MAX_AP &&
		    np->np_aps[ni->ni_naps] != NULL) {
			ni->ni_aps[ni->ni_naps] = np->np_aps[ni->ni_naps];
			ni->ni_naps++;
		}
	}

	if (ni->ni_sromfile == NULL)
		ni->ni_sromfile = nicinit_firmware_path(ni, np->np_srom);
	if (ni->ni_fwfile == NULL)
		ni->ni_fwfile = nicinit_firmware_path(ni, np->np_fw);
	if (ni->ni_hashlen == 0)
		ni->ni_hashlen = np->np_hashlen;
	ni->ni_link_gen = np->np_link_gen;
	ni->ni_link_width = np->np_link_width;
	if (!ni->ni_pci_ss_did_set && np->np_pci_ss_did != 0) {
		ni->ni_pci_ss_did = np->np_pci_ss_did;
		ni->ni_pci_ss_did_set = true;
	}
}

static void
nicinit_online_link_check(nicinit_t *ni)
{
	const nicinit_ops_t *ops = ni->ni_ops;

	if (ops->no_driver != NULL && ni->ni_link_gen != 0) {
		nicinit_link_check(ni, ops->no_driver, ni->ni_link_gen,
		    ni->ni_link_width);
	}
}

/*
 * Bring the NIC online. The return value is the exit status.
 */
static int
nicinit_online(nicinit_t *ni)
{
	const nicinit_ops_t *ops = ni->ni_ops;
	const char *name = ops->no_name;
	bool match = true;

	if (ops->no_online != NULL) {
		bool online = false;

		if (!ops->no_online(ni, &online)) {
			nicinit_failed(ni, NICINIT_CLASS_STATE,
			    "failed to determine the state of the %s", name);
			return (EXIT_FAILURE);
		}
		if (online) {
			(void) printf("%s is already online\n", name);
			nicinit_online_link_check(ni);
			return (EXIT_SUCCESS);
		}
	}

	if (ops->no_discover != NULL) {
		bool found = false;

		if (!ops->no_discover(ni, &found)) {
			nicinit_failed(ni, NICINIT_CLASS_NODEV,
			    "failed to look for a %s", name);
			return (EXIT_FAILURE);
		}
		if (!found) {
			nicinit_failed(ni, NICINIT_CLASS_NODEV,
			    "no %s found to bring online", name);
			return (EXIT_NODEV);
		}
	}

	if (ops->no_prepare != NULL && !ops->no_prepare(ni)) {
		nicinit_failed(ni, NICINIT_CLASS_PREPARE,
		    "failed to prepare to verify the %s", name);
		return (EXIT_FAILURE);
	}

	if (ops->no_verify != NULL && !ops->no_verify(ni, &match)) {
		nicinit_failed(ni, NICINIT_CLASS_VERIFY,
		    "failed to verify the %s firmware", name);
		return (EXIT_FAILURE);
	}

	if (!match) {
		/*
		 * A mismatch that we go on to correct is not a problem, but
		 * publish a low priority event so that there is a record of
		 * the firmware having been programmed.
		 */
		nicinit_publish(ni, NICINIT_CLASS_MISMATCH, FMEV_LOPRI,
		    "firmware does not match and will be programmed");

		if (ops->no_program == NULL) {
			nicinit_failed(ni, NICINIT_CLASS_PROGRAM,
			    "%s firmware does not match and cannot be "
			    "programmed", name);
			return (EXIT_FAILURE);
		}

		nicinit_verbose("%s firmware verification failed, programming",
		    name);
		if (!ops->no_program(ni)) {
			nicinit_failed(ni, NICINIT_CLASS_PROGRAM,
			    "failed to program the %s firmware", name);
			return (EXIT_FAILURE);
		}

		if (!ops->no_verify(ni, &match)) {
			nicinit_failed(ni, NICINIT_CLASS_VERIFY,
			    "failed to verify the %s firmware after "
			    "programming", name);
			return (EXIT_FAILURE);
		}
		if (!match) {
			nicinit_failed(ni, NICINIT_CLASS_STILL_MISMATCH,
			    "%s firmware verification failed AFTER "
			    "programming", name);
			return (EXIT_FAILURE);
		}
	}

	if (!ops->no_release(ni)) {
		nicinit_failed(ni, NICINIT_CLASS_RELEASE,
		    "failed to bring the %s online", name);
		return (EXIT_FAILURE);
	}

	if (ops->no_wait != NULL && !ops->no_wait(ni)) {
		nicinit_failed(ni, NICINIT_CLASS_NOT_CONFIGURED,
		    "%s did not come online", name);
		return (EXIT_FAILURE);
	}

	nicinit_online_link_check(ni);

	return (EXIT_SUCCESS);
}

int
main(int argc, char **argv)
{
	nicinit_t ni = { 0 };
	const char *errstr;
	bool offline = false;
	int c, ret;

	progname = basename(argv[0]);
	openlog(progname, LOG_PID, LOG_DAEMON);

	while ((c = getopt(argc, argv, ":hMvA:D:f:L:P:s:T:")) != -1) {
		switch (c) {
		case 'A':
			if (ni.ni_naps == NICINIT_MAX_AP) {
				usage("at most %u attachment points may be "
				    "specified", NICINIT_MAX_AP);
			}
			ni.ni_aps[ni.ni_naps++] = optarg;
			break;
		case 'D':
			ni.ni_dpio = optarg;
			break;
		case 'f':
			ni.ni_fwfile = optarg;
			break;
		case 'h':
			show_help();
			return (EXIT_SUCCESS);
		case 'L':
			ni.ni_hashlen = (uint64_t)strtonumx(optarg, 1,
			    UINT32_MAX, &errstr, 0);
			if (errstr != NULL) {
				errx(EXIT_USAGE, "measured length '%s' is %s",
				    optarg, errstr);
			}
			break;
		case 'M':
			offline = true;
			break;
		case 'P':
			ni.ni_pci_ss_did = (uint16_t)strtonumx(optarg, 0,
			    UINT16_MAX, &errstr, 0);
			if (errstr != NULL) {
				errx(EXIT_USAGE, "PCI sub-system ID '%s' is %s",
				    optarg, errstr);
			}
			ni.ni_pci_ss_did_set = true;
			break;
		case 's':
			ni.ni_sromfile = optarg;
			break;
		case 'T':
			ni.ni_ops = nicinit_flavour(optarg);
			break;
		case 'v':
			verbose = true;
			break;
		case ':':
			usage("Option -%c requires an argument", optopt);
		case '?':
		default:
			usage("unknown option: -%c", optopt);
		}
	}

	if (optind != argc)
		usage("unexpected argument: %s", argv[optind]);

	ni.ni_platform = nicinit_platform_name();
	nicinit_platform_defaults(&ni);

	if (ni.ni_naps == 0)
		usage("no attachment point is known, specify one with -A");

	if (ni.ni_ops->no_args != NULL && !ni.ni_ops->no_args(&ni, offline))
		usage("invalid options for a %s", ni.ni_ops->no_name);

	if (nvlist_alloc(&ni.ni_detail, NV_UNIQUE_NAME, 0) != 0)
		errx(EXIT_FAILURE, "failed to allocate memory");

	nicinit_verbose("Platform %s, NIC type %s", ni.ni_platform,
	    ni.ni_ops->no_name);

	if (!offline) {
		ret = nicinit_online(&ni);
	} else if (ni.ni_ops->no_offline == NULL) {
		warnx("a %s cannot be taken offline", ni.ni_ops->no_name);
		ret = EXIT_FAILURE;
	} else if (ni.ni_ops->no_offline(&ni)) {
		ret = EXIT_SUCCESS;
	} else {
		warnx("failed to take the %s offline", ni.ni_ops->no_name);
		ret = EXIT_FAILURE;
	}

	if (ni.ni_ops->no_fini != NULL)
		ni.ni_ops->no_fini(&ni);
	nicinit_ipcc_fini();
	nvlist_free(ni.ni_detail);

	return (ret);
}
