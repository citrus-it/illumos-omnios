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

#ifndef _NICINIT_H
#define	_NICINIT_H

/*
 * Internal, common definitions for nicinit.
 */

#include <libdevinfo.h>
#include <libnvpair.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/ccompile.h>
#include <sys/ethernet.h>
#include <sys/time.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define	EXIT_USAGE	2
/*
 * There is no device to initialise. Either there is no NIC or it is not in
 * the state from which it can be initialised.
 */
#define	EXIT_NODEV	3
/*
 * We do not know of a NIC on this platform.
 */
#define	EXIT_UNSUPPORTED	4

#define	T6_PRODUCT_STR			"T62100-KR"
/*
 * The dual port T6 needs at least T6_MAC_COUNT MAC addresses that are
 * T6_MAC_STRIDE apart. Only the base address is programmed and the second
 * port is automatically given an address which is the base + T6_MAC_STRIDE.
 */
#define	T6_MAC_COUNT			2
#define	T6_MAC_STRIDE			8
#define	T6_PCI_SUBSYSTEM_VENDORID	0x1de

#define	T6_MFG_DRIVER			"t6mfg"
#define	T6_MISSION_DRIVER		"t4nex"
/*
 * The PCIe link that a T6 is expected to have in manufacturing mode, where we
 * limit it to Gen 1. The link that is expected in mission mode depends on the
 * platform.
 */
#define	T6_MFG_LINK_GEN			1
#define	T6_MFG_LINK_WIDTH		8
#define	T6_PCIEB_DRIVER			"pcieb"
#define	T6_PCIEB_MINOR			"devctl"

/*
 * The most attachment points that a NIC can occupy.
 */
#define	NICINIT_MAX_AP			2

typedef struct nicinit nicinit_t;

/*
 * Each flavour of NIC provides a set of operations which are called in the
 * order that they are listed here to take the NIC from the state that it is
 * in after boot to one in which it is usable by the system. All of them
 * return false on failure, having reported the reason. Any operation other
 * than no_release may be NULL if a NIC has no need of that step. A failure is
 * reported as a problem, the class of which depends on the step that failed.
 * An operation can set a more specific class with nicinit_fail_class() before
 * it returns, and the first class set for a failure is the one that is used.
 * It can also add details to be included in the report with the
 * nicinit_detail functions at any time.
 */
typedef struct nicinit_ops {
	/*
	 * The name of the flavour.
	 */
	const char *no_name;

	/*
	 * The driver for the NIC's devices once it is online. The PCIe link
	 * to each device that it is attached to is then checked against the
	 * speed and width that are expected on the platform.
	 */
	const char *no_driver;

	/*
	 * The options that only this flavour has, in the form that getopt(3C)
	 * takes. Each one that is given is passed to no_option with its
	 * argument.
	 */
	const char *no_optstring;
	bool (*no_option)(nicinit_t *, int, const char *);

	/*
	 * Check that the options provided are valid for this NIC and apply
	 * what the platform knows for those that were not given. This is
	 * called after no_option, both when bringing the NIC online and when
	 * taking it offline.
	 */
	bool (*no_args)(nicinit_t *, bool);

	/*
	 * Determine whether the NIC is already online, in which case no other
	 * operation is called.
	 */
	bool (*no_online)(nicinit_t *, bool *);

	/*
	 * Find the device in the state from which it can be initialised. If
	 * there is no such device this sets its second argument to false and
	 * no other operation is called.
	 */
	bool (*no_discover)(nicinit_t *, bool *);

	/*
	 * Gather whatever is needed to verify and program the device.
	 */
	bool (*no_prepare)(nicinit_t *);

	/*
	 * Determine whether the firmware in the device matches what we would
	 * program into it.
	 */
	bool (*no_verify)(nicinit_t *, bool *);

	/*
	 * Program the device. Only called if no_verify found a mismatch, and
	 * is followed by a second call to no_verify which must then find a
	 * match.
	 */
	bool (*no_program)(nicinit_t *);

	/*
	 * Provide anything else that the NIC needs from us before it is
	 * brought online, such as its MAC addresses.
	 */
	bool (*no_setup)(nicinit_t *);

	/*
	 * Bring the NIC online.
	 */
	bool (*no_release)(nicinit_t *);

	/*
	 * Wait for the NIC to appear to the system.
	 */
	bool (*no_wait)(nicinit_t *);

	/*
	 * The reverse of all of the above, used to take the NIC back to the
	 * state it was in after boot. This is a development aid.
	 */
	bool (*no_offline)(nicinit_t *);

	/*
	 * Release any resources.
	 */
	void (*no_fini)(nicinit_t *);
} nicinit_ops_t;

/*
 * The options below default to what is known about the platform that we are
 * running on, where it is one we know.
 */
struct nicinit {
	const char *ni_platform;
	const nicinit_ops_t *ni_ops;
	const char *ni_aps[NICINIT_MAX_AP];
	uint_t ni_naps;
	const char *ni_fwfile;
	const void *ni_platdata;
	uint_t ni_link_gen;
	uint_t ni_link_width;
	const char *ni_fail_class;
	nvlist_t *ni_detail;
	void *ni_priv;
};

/*
 * The classes of problem that we report. Each is the final component of the
 * class of the ireport that is generated for it.
 */
#define	NICINIT_CLASS_STATE		"state-check-failed"
#define	NICINIT_CLASS_NODEV		"no-device"
#define	NICINIT_CLASS_PREPARE		"prepare-failed"
#define	NICINIT_CLASS_RANGE		"bad-measured-range"
#define	NICINIT_CLASS_VERIFY		"verify-failed"
#define	NICINIT_CLASS_MISMATCH		"firmware-mismatch"
#define	NICINIT_CLASS_PROGRAM		"program-failed"
#define	NICINIT_CLASS_STILL_MISMATCH	"mismatch-after-program"
#define	NICINIT_CLASS_SETUP		"setup-failed"
#define	NICINIT_CLASS_RELEASE		"release-failed"
#define	NICINIT_CLASS_CONFIG_TIMEOUT	"config-timeout"
#define	NICINIT_CLASS_CONFIG_ERROR	"config-error"
#define	NICINIT_CLASS_NOT_CONFIGURED	"not-configured"
#define	NICINIT_CLASS_LINK		"link-degraded"

/*
 * T6-specific platform information, which is defined in nicinit_t6.c. This is
 * where ni_platdata points to on those platforms.
 */
typedef struct nicinit_t6_platform nicinit_t6_platform_t;
extern const nicinit_t6_platform_t nicinit_t6_gimlet;
extern const nicinit_t6_platform_t nicinit_t6_cosmo;

extern const nicinit_ops_t nicinit_t6_ops;
extern const nicinit_ops_t nicinit_redhawk_ops;

extern void nicinit_print(const char *, ...) __PRINTFLIKE(1);
extern void nicinit_verbose(const char *, ...) __PRINTFLIKE(1);

extern void nicinit_fail_class(nicinit_t *, const char *);
extern void nicinit_detail_string(nicinit_t *, const char *, const char *);
extern void nicinit_detail_uint32(nicinit_t *, const char *, uint32_t);
extern void nicinit_link_check(nicinit_t *, const char *, uint_t, uint_t);
extern char *nicinit_minor_path(di_node_t, const char *);
extern const char *nicinit_firmware_path(const nicinit_t *, const char *);

/*
 * Information that comes from the service processor.
 */
extern bool nicinit_ident(char **, char **);
extern bool nicinit_macaddr(uint16_t, uint8_t, ether_addr_t *);

/*
 * Attachment point helpers. An attachment point whose occupant is configured
 * is NICINIT_AP_CONFIGURED. The other states are those of its receptacle.
 */
typedef enum {
	NICINIT_AP_EMPTY,
	NICINIT_AP_DISCONNECTED,
	NICINIT_AP_CONNECTED,
	NICINIT_AP_CONFIGURED
} nicinit_ap_state_t;

extern bool nicinit_ap_state(const char *, nicinit_ap_state_t *);
extern const char *nicinit_ap_state_name(nicinit_ap_state_t);
extern bool nicinit_ap_disconnect(const char *);
extern bool nicinit_ap_configure(const char *);
extern bool nicinit_ap_wait(const nicinit_t *, uint_t);

/*
 * How long an attachment point may be occupied without having been configured
 * before we explicitly configure.
 */
#define	NICINIT_AP_GRACE_SEC	3

/*
 * How often we look at something that we are waiting on, and a function that
 * waits for that long.
 */
#define	NICINIT_POLL_MSEC	100
extern void nicinit_pause(void);

#ifdef __cplusplus
}
#endif

#endif /* _NICINIT_H */
