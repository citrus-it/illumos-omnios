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
 * Copyright 2016 Nexenta Systems, Inc.  All rights reserved.
 */

#include <fm/fmd_api.h>
#include <sys/note.h>
#include <fm/libtopo.h>
#include <sys/fm/protocol.h>
#include <strings.h>

typedef struct slow_io_stat {
	fmd_stat_t bad_fmri;
	fmd_stat_t bad_scheme;
} slow_io_stat_t;

slow_io_stat_t slow_io_stats = {
	{ "bad_FMRI", FMD_TYPE_UINT64,
		"event FMRI is missing or invalid" },
	{ "bad_scheme", FMD_TYPE_UINT64,
		"event does not contain a valid detector"},
};

static const fmd_prop_t fmd_props [] = {
	{ "io_N", FMD_TYPE_INT32, "10" },
	{ "io_T", FMD_TYPE_TIME, "10min"},
	{ NULL, 0, NULL }
};

void
slow_io_close(fmd_hdl_t *hdl, fmd_case_t *c)
{
	char *devid = fmd_case_getspecific(hdl, c);
	if (devid != NULL) {
		fmd_hdl_debug(hdl, "Destroying serd: %s", devid);
		fmd_serd_destroy(hdl, devid);
	}
}

void
slow_io_recv(fmd_hdl_t *hdl, fmd_event_t *event, nvlist_t *nvl,
	const char *class)
{
	nvlist_t *detector = NULL;
	char *devid = NULL;
	nvlist_t *fault;
	fmd_hdl_topo_node_info_t *node;
	_NOTE(ARGUNUSED(class));

	if (nvlist_lookup_nvlist(nvl, "detector", &detector) != 0) {
		slow_io_stats.bad_scheme.fmds_value.ui64++;
		return;
	}

	if (nvlist_lookup_string(detector, "devid", &devid) != 0) {
		slow_io_stats.bad_fmri.fmds_value.ui64++;
		return;
	}

	if (fmd_serd_exists(hdl, devid) == 0) {
		fmd_serd_create(hdl, devid, fmd_prop_get_int32(hdl, "io_N"),
		    fmd_prop_get_int64(hdl, "io_T"));
		(void) fmd_serd_record(hdl, devid, event);
		return;
	}

	if (fmd_serd_record(hdl, devid, event) == FMD_B_TRUE) {
		fmd_case_t *c = fmd_case_open(hdl, NULL);
		fmd_case_add_serd(hdl, c, devid);
		node = fmd_hdl_topo_node_get_by_devid(hdl, devid);

		/*
		 * If for some reason libtopo does not enumureate the device
		 * we still want to create a fault, we will see a "bad"
		 * FMA message however that lacks the FRU information.
		 */

		if (node == NULL) {
			fault = fmd_nvl_create_fault(hdl,
			    "fault.io.disk.slow-io", 100,
			    detector, NULL, NULL);
		} else {
			fault = fmd_nvl_create_fault(hdl,
			    "fault.io.disk.slow-io", 100,
			    detector, node->fru, node->resource);
			nvlist_free(node->fru);
			nvlist_free(node->resource);
			fmd_hdl_free(hdl, node,
			    sizeof (fmd_hdl_topo_node_info_t));
		}

		fmd_case_add_suspect(hdl, c, fault);
		fmd_case_setspecific(hdl, c, devid);
		fmd_case_solve(hdl, c);
	}
}

static const fmd_hdl_ops_t fmd_ops = {
	slow_io_recv,
	NULL,
	slow_io_close,
	NULL,
	NULL,
};

static const fmd_hdl_info_t fmd_info = {
	"slow-io-de", "0.2", &fmd_ops, fmd_props
};

void
_fmd_init(fmd_hdl_t *hdl)
{
	if (fmd_hdl_register(hdl, FMD_API_VERSION, &fmd_info) != 0) {
		fmd_hdl_debug(hdl, "Internal error\n");
		return;
	}

	(void) fmd_stat_create(hdl, FMD_STAT_NOALLOC, sizeof (slow_io_stats) /
	    sizeof (fmd_stat_t), (fmd_stat_t *)&slow_io_stats);
}

void
_fmd_fini(fmd_hdl_t *hdl)
{
	_NOTE(ARGUNUSED(hdl));
}
