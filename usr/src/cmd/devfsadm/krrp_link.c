/*
 * Copyright 2015 Nexenta Systems, Inc.  All rights reserved.
 */

#include <regex.h>
#include <devfsadm.h>
#include <stdio.h>
#include <strings.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/mkdev.h>

#include <sys/krrp.h>

/* krrp name info */

static int krrp(di_minor_t minor, di_node_t node);

/*
 * devfs create callback register
 */
static devfsadm_create_t krrp_create_cbt[] = {
	{ "pseudo", "ddi_pseudo", KRRP_DRIVER,
	    TYPE_EXACT | DRV_EXACT, ILEVEL_0, krrp,
	},
};

DEVFSADM_CREATE_INIT_V0(krrp_create_cbt);

/*
 * The krrp control node looks like this:
 *	/dev/krrp -> /devices/pseudo/krrp@0:krrp
 */
static int
krrp(di_minor_t minor, di_node_t node)
{
	char mn[MAXNAMELEN + 1];

	(void) strcpy(mn, di_minor_name(minor));

	if (strcmp(mn, KRRP_DRIVER) == 0)
		(void) devfsadm_mklink(KRRP_DRIVER, node, minor, 0);

	return (DEVFSADM_CONTINUE);
}
