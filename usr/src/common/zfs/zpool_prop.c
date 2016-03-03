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
 * Copyright (c) 2007, 2010, Oracle and/or its affiliates. All rights reserved.
 * Copyright (c) 2012, 2014 by Delphix. All rights reserved.
 * Copyright (c) 2014 Integros [integros.com]
 * Copyright 2016 Nexenta Systems, Inc. All rights reserved.
 */

#include <sys/zio.h>
#include <sys/spa.h>
#include <sys/special.h>
#include <sys/zfs_acl.h>
#include <sys/zfs_ioctl.h>
#include <sys/fs/zfs.h>

#include "zfs_prop.h"

#if defined(_KERNEL)
#include <sys/systm.h>
#else
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#endif

static zprop_desc_t zpool_prop_table[ZPOOL_NUM_PROPS];

zprop_desc_t *
zpool_prop_get_table(void)
{
	return (zpool_prop_table);
}

void
zpool_prop_init(void)
{
	static zprop_index_t boolean_table[] = {
		{ "off",	0},
		{ "on",		1},
		{ NULL }
	};

	static zprop_index_t meta_placement_table[] = {
		{ "off",	META_PLACEMENT_OFF},
		{ "on",		META_PLACEMENT_ON},
		{ "dual",	META_PLACEMENT_DUAL},
		{ NULL }
	};

	static zprop_index_t failuremode_table[] = {
		{ "wait",	ZIO_FAILURE_MODE_WAIT },
		{ "continue",	ZIO_FAILURE_MODE_CONTINUE },
		{ "panic",	ZIO_FAILURE_MODE_PANIC },
		{ NULL }
	};

	static zprop_index_t sync_to_special_table[] = {
		{ "disabled",	SYNC_TO_SPECIAL_DISABLED },
		{ "standard",	SYNC_TO_SPECIAL_STANDARD },
		{ "balanced",	SYNC_TO_SPECIAL_BALANCED },
		{ "always",	SYNC_TO_SPECIAL_ALWAYS},
		{ NULL }
	};

	/*
	 * NOTE: When either adding or changing a property make sure
	 * to update the zfs-tests zpool_get configuration file
	 * at usr/src/test/zfs-tests/tests/functional/cli_root/zpool_get/
	 * zpool_get.cfg
	 */

	/* string properties */
	zprop_register_string(ZPOOL_PROP_ALTROOT, "altroot", NULL, PROP_DEFAULT,
	    ZFS_TYPE_POOL, "<path>", "ALTROOT");
	zprop_register_string(ZPOOL_PROP_BOOTFS, "bootfs", NULL, PROP_DEFAULT,
	    ZFS_TYPE_POOL, "<filesystem>", "BOOTFS");
	zprop_register_string(ZPOOL_PROP_CACHEFILE, "cachefile", NULL,
	    PROP_DEFAULT, ZFS_TYPE_POOL, "<file> | none", "CACHEFILE");
	zprop_register_string(ZPOOL_PROP_COMMENT, "comment", NULL,
	    PROP_DEFAULT, ZFS_TYPE_POOL, "<comment-string>", "COMMENT");

	/* readonly number properties */
	zprop_register_number(ZPOOL_PROP_SIZE, "size", 0, PROP_READONLY,
	    ZFS_TYPE_POOL, "<size>", "SIZE");
	zprop_register_number(ZPOOL_PROP_FREE, "free", 0, PROP_READONLY,
	    ZFS_TYPE_POOL, "<size>", "FREE");
	zprop_register_number(ZPOOL_PROP_FREEING, "freeing", 0, PROP_READONLY,
	    ZFS_TYPE_POOL, "<size>", "FREEING");
	zprop_register_number(ZPOOL_PROP_LEAKED, "leaked", 0, PROP_READONLY,
	    ZFS_TYPE_POOL, "<size>", "LEAKED");
	zprop_register_number(ZPOOL_PROP_ALLOCATED, "allocated", 0,
	    PROP_READONLY, ZFS_TYPE_POOL, "<size>", "ALLOC");
	zprop_register_number(ZPOOL_PROP_EXPANDSZ, "expandsize", 0,
	    PROP_READONLY, ZFS_TYPE_POOL, "<size>", "EXPANDSZ");
	zprop_register_number(ZPOOL_PROP_FRAGMENTATION, "fragmentation", 0,
	    PROP_READONLY, ZFS_TYPE_POOL, "<percent>", "FRAG");
	zprop_register_number(ZPOOL_PROP_CAPACITY, "capacity", 0, PROP_READONLY,
	    ZFS_TYPE_POOL, "<size>", "CAP");
	zprop_register_number(ZPOOL_PROP_GUID, "guid", 0, PROP_READONLY,
	    ZFS_TYPE_POOL, "<guid>", "GUID");
	zprop_register_number(ZPOOL_PROP_HEALTH, "health", 0, PROP_READONLY,
	    ZFS_TYPE_POOL, "<state>", "HEALTH");
	zprop_register_number(ZPOOL_PROP_DEDUPRATIO, "dedupratio", 0,
	    PROP_READONLY, ZFS_TYPE_POOL, "<1.00x or higher if deduped>",
	    "DEDUP");
	zprop_register_index(ZPOOL_PROP_DDTCAPPED, "ddt_capped", 0,
	    PROP_READONLY, ZFS_TYPE_POOL, "off | on", "DDT_CAPPED",
	    boolean_table);

	/* default number properties */
	zprop_register_number(ZPOOL_PROP_VERSION, "version", SPA_VERSION,
	    PROP_DEFAULT, ZFS_TYPE_POOL, "<version>", "VERSION");
	zprop_register_number(ZPOOL_PROP_DEDUPDITTO, "dedupditto", 0,
	    PROP_DEFAULT, ZFS_TYPE_POOL, "<threshold (min 100)>", "DEDUPDITTO");
	zprop_register_number(ZPOOL_PROP_DEDUPMETA_DITTO, "dedup_meta_ditto", 0,
	    PROP_DEFAULT, ZFS_TYPE_POOL, "<number of copies>",
	    "DEDUP_META_DITTO");
	zprop_register_number(ZPOOL_PROP_DEDUP_LO_BEST_EFFORT,
	    "dedup_lo_best_effort", 60, PROP_DEFAULT, ZFS_TYPE_POOL,
	    "0-100", "DEDUP_LO_BEST_EFFORT");
	zprop_register_number(ZPOOL_PROP_DEDUP_HI_BEST_EFFORT,
	    "dedup_hi_best_effort", 80, PROP_DEFAULT, ZFS_TYPE_POOL,
	    "0-100", "DEDUP_HI_BEST_EFFORT");

	/* default index (boolean) properties */
	zprop_register_index(ZPOOL_PROP_DELEGATION, "delegation", 1,
	    PROP_DEFAULT, ZFS_TYPE_POOL, "on | off", "DELEGATION",
	    boolean_table);
	zprop_register_index(ZPOOL_PROP_AUTOREPLACE, "autoreplace", 0,
	    PROP_DEFAULT, ZFS_TYPE_POOL, "on | off", "REPLACE", boolean_table);
	zprop_register_index(ZPOOL_PROP_LISTSNAPS, "listsnapshots", 0,
	    PROP_DEFAULT, ZFS_TYPE_POOL, "on | off", "LISTSNAPS",
	    boolean_table);
	zprop_register_index(ZPOOL_PROP_AUTOEXPAND, "autoexpand", 0,
	    PROP_DEFAULT, ZFS_TYPE_POOL, "on | off", "EXPAND", boolean_table);
	zprop_register_index(ZPOOL_PROP_READONLY, "readonly", 0,
	    PROP_DEFAULT, ZFS_TYPE_POOL, "on | off", "RDONLY", boolean_table);
	zprop_register_index(ZPOOL_PROP_DDT_DESEGREGATION, "ddt_desegregation",
	    0, PROP_DEFAULT, ZFS_TYPE_POOL, "on | off", "DDT_DESEG",
	    boolean_table);
	zprop_register_index(ZPOOL_PROP_DEDUP_BEST_EFFORT, "dedup_best_effort",
	    0, PROP_DEFAULT, ZFS_TYPE_POOL, "on | off", "DEDUP_BEST_EFFORT",
	    boolean_table);

	zprop_register_index(ZPOOL_PROP_META_PLACEMENT, "meta_placement", 0,
	    PROP_DEFAULT, ZFS_TYPE_POOL, "on | off", "META_PLCMNT",
	    boolean_table);
	zprop_register_index(ZPOOL_PROP_SYNC_TO_SPECIAL, "sync_to_special",
	    SYNC_TO_SPECIAL_STANDARD, PROP_DEFAULT, ZFS_TYPE_POOL,
	    "never | standard | balanced | always", "SYNC_TO_SPECIAL",
	    sync_to_special_table);
	zprop_register_index(ZPOOL_PROP_DDT_META_TO_METADEV,
	    "ddt_meta_to_metadev", META_PLACEMENT_OFF, PROP_DEFAULT,
	    ZFS_TYPE_POOL, "on | dual | off",
	    "DDTMETA_TO_MD", meta_placement_table);
	zprop_register_index(ZPOOL_PROP_ZFS_META_TO_METADEV,
	    "zfs_meta_to_metadev", META_PLACEMENT_OFF, PROP_DEFAULT,
	    ZFS_TYPE_POOL, "on | dual | off",
	    "ZFSMETA_TO_MD", meta_placement_table);

	/* default index properties */
	zprop_register_index(ZPOOL_PROP_FAILUREMODE, "failmode",
	    ZIO_FAILURE_MODE_WAIT, PROP_DEFAULT, ZFS_TYPE_POOL,
	    "wait | continue | panic", "FAILMODE", failuremode_table);
	zprop_register_index(ZPOOL_PROP_FORCETRIM, "forcetrim",
	    SPA_FORCE_TRIM_OFF, PROP_DEFAULT, ZFS_TYPE_POOL,
	    "on | off", "FORCETRIM", boolean_table);
	zprop_register_index(ZPOOL_PROP_AUTOTRIM, "autotrim",
	    SPA_AUTO_TRIM_OFF, PROP_DEFAULT, ZFS_TYPE_POOL,
	    "on | off", "AUTOTRIM", boolean_table);

	/* special device status (enabled/disabled) */
	zprop_register_index(ZPOOL_PROP_ENABLESPECIAL, "enablespecial", 0,
	    PROP_READONLY, ZFS_TYPE_POOL, "on | off", "ENABLESPECIAL",
	    boolean_table);

	/* pool's min watermark in percents (for write cache) */
	zprop_register_number(ZPOOL_PROP_MINWATERMARK, "min-watermark",
	    20, PROP_DEFAULT, ZFS_TYPE_POOL,
	    "<watermark 0-100%>", "MINWATERMARK");

	/* pool's low watermark in percents (for write cache) */
	zprop_register_number(ZPOOL_PROP_LOWATERMARK, "low-watermark",
	    60, PROP_DEFAULT, ZFS_TYPE_POOL,
	    "<watermark 0-100%>", "LOWATERMARK");

	/* pool's high watermark in percents (for write cache) */
	zprop_register_number(ZPOOL_PROP_HIWATERMARK, "high-watermark",
	    80, PROP_DEFAULT, ZFS_TYPE_POOL,
	    "<watermark 0-100%>", "HIWATERMARK");

	zprop_register_number(ZPOOL_PROP_SMALL_DATA_TO_METADEV,
	    "small_data_to_metadev", 0, PROP_DEFAULT, ZFS_TYPE_POOL,
	    "Threshold to route to md", "SMALLDATA_TO_MD");

	/* hidden properties */
	zprop_register_hidden(ZPOOL_PROP_NAME, "name", PROP_TYPE_STRING,
	    PROP_READONLY, ZFS_TYPE_POOL, "NAME");
	zprop_register_hidden(ZPOOL_PROP_MAXBLOCKSIZE, "maxblocksize",
	    PROP_TYPE_NUMBER, PROP_READONLY, ZFS_TYPE_POOL, "MAXBLOCKSIZE");
}

/*
 * Given a property name and its type, returns the corresponding property ID.
 */
zpool_prop_t
zpool_name_to_prop(const char *propname)
{
	return (zprop_name_to_prop(propname, ZFS_TYPE_POOL));
}

/*
 * Given a pool property ID, returns the corresponding name.
 * Assuming the pool propety ID is valid.
 */
const char *
zpool_prop_to_name(zpool_prop_t prop)
{
	return (zpool_prop_table[prop].pd_name);
}

zprop_type_t
zpool_prop_get_type(zpool_prop_t prop)
{
	return (zpool_prop_table[prop].pd_proptype);
}

boolean_t
zpool_prop_readonly(zpool_prop_t prop)
{
	return (zpool_prop_table[prop].pd_attr == PROP_READONLY);
}

const char *
zpool_prop_default_string(zpool_prop_t prop)
{
	return (zpool_prop_table[prop].pd_strdefault);
}

uint64_t
zpool_prop_default_numeric(zpool_prop_t prop)
{
	return (zpool_prop_table[prop].pd_numdefault);
}

/*
 * Returns true if this is a valid feature@ property.
 */
boolean_t
zpool_prop_feature(const char *name)
{
	static const char *prefix = "feature@";
	return (strncmp(name, prefix, strlen(prefix)) == 0);
}

/*
 * Returns true if this is a valid unsupported@ property.
 */
boolean_t
zpool_prop_unsupported(const char *name)
{
	static const char *prefix = "unsupported@";
	return (strncmp(name, prefix, strlen(prefix)) == 0);
}

int
zpool_prop_string_to_index(zpool_prop_t prop, const char *string,
    uint64_t *index)
{
	return (zprop_string_to_index(prop, string, index, ZFS_TYPE_POOL));
}

int
zpool_prop_index_to_string(zpool_prop_t prop, uint64_t index,
    const char **string)
{
	return (zprop_index_to_string(prop, index, string, ZFS_TYPE_POOL));
}

uint64_t
zpool_prop_random_value(zpool_prop_t prop, uint64_t seed)
{
	return (zprop_random_value(prop, seed, ZFS_TYPE_POOL));
}

#ifndef _KERNEL

const char *
zpool_prop_values(zpool_prop_t prop)
{
	return (zpool_prop_table[prop].pd_values);
}

const char *
zpool_prop_column_name(zpool_prop_t prop)
{
	return (zpool_prop_table[prop].pd_colname);
}

boolean_t
zpool_prop_align_right(zpool_prop_t prop)
{
	return (zpool_prop_table[prop].pd_rightalign);
}
#endif
