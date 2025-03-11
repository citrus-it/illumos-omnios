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
 * Copyright 2010 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 *
 * Copyright 2025 Oxide Computer Company
 * Copyright 2025 OmniOS Community Edition (OmniOSce) Association.
 */

/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	All Rights Reserved	*/

#include <sys/param.h>
#include <sys/time.h>
#include <sys/systm.h>
#include <sys/archsystm.h>
#include <sys/lockstat.h>
#include <sys/stdbool.h>
#include <sys/stddef.h>
#include <sys/types.h>
#include <sys/brand.h>
#include <sys/conf.h>
#include <sys/door.h>
#include <sys/list.h>
#include <sys/zone.h>
#include <sys/debug.h>

#include <sys/clock.h>
#include <sys/smp_impldefs.h>

/*
 * This file contains all generic part of clock and timer handling.
 * We do not support a hardware time-of-day unit, sometimes called a real-time
 * clock (distinct from the POSIX notion of CLOCK_REALTIME), on this
 * architecture so some of this is stubbed out.
 */

#define	DOOR_PATH	"var/run/utmpd_door"

typedef struct {
	list_node_t	dp_link;
	size_t		dp_size;
	char		dp_path[];
} door_path_t;

static int
process_zone_cb(zone_t *zone, void *door_paths)
{
	door_path_t *door_path;
	size_t dplen;

	/* Ignore non-native zone brands */
	if (ZONE_IS_BRANDED(zone))
		return (0);

	dplen = zone->zone_rootpathlen + sizeof (DOOR_PATH) - 1;

	/*
	 * Note that we return 0 even in the event of failures as this is
	 * best-effort; we keep going to notify as many zones as possible.
	 */
	door_path = kmem_alloc(sizeof (door_path_t) + dplen, KM_NOSLEEP);
	if (door_path == NULL)
		return (0);

	door_path->dp_size = sizeof (door_path_t) + dplen;
	if (snprintf(door_path->dp_path, dplen, "%s%s", zone->zone_rootpath,
	    DOOR_PATH) >= dplen) {
		kmem_free(door_path, door_path->dp_size);
		return (0);
	}

	list_insert_tail(door_paths, door_path);
	return (0);
}

static void
tod_set_cb(void *arg)
{
	list_t door_paths;
	time_t boot_ts;
	door_path_t *door_path;

	list_create(&door_paths, sizeof (door_path_t),
	    offsetof(door_path_t, dp_link));

	(void) zone_walk(process_zone_cb, &door_paths);

	boot_ts = (time_t)(uintptr_t)arg;

	while ((door_path = list_remove_head(&door_paths)) != NULL) {
		door_handle_t door;
		int ret = door_ki_open(door_path->dp_path, &door);

		if (ret == 0) {
			door_arg_t darg = {
				.data_ptr = (void *)&boot_ts,
				.data_size = sizeof (boot_ts),
			};

			ret = door_ki_upcall_limited(door, &darg, NULL, 0, 0);
			if (ret == 0) {
				cmn_err(CE_CONT, "?Time has stepped forwards; "
				    "successfully notified utmpd at %s.\n",
				    door_path->dp_path);
			} else {
				cmn_err(CE_WARN, "Time has stepped forwards; "
				    "failed upcall to utmpd at %s, err %d",
				    door_path->dp_path, ret);
			}

			door_ki_rele(door);
		} else {
			cmn_err(CE_WARN, "Time has stepped forwards; "
			    "failed to open door to utmpd at %s, err %d",
			    door_path->dp_path, ret);
		}

		kmem_free(door_path, door_path->dp_size);
	}

	list_destroy(&door_paths);
}

void
tod_set(timestruc_t ts)
{
	time_t adj;
	static bool already_stepped = false;
	extern time_t boot_time;

	ASSERT(MUTEX_HELD(&tod_lock));

	/*
	 * There is no TOD unit, so there's nothing to do regarding that.
	 *
	 * However we take this opportunity to spot when the clock is stepped
	 * significantly forward, and use that as a cue that the system clock
	 * has been set initially after time synchronisation. When this happens
	 * we go through and update the global `boot_time` variable, and the
	 * `zone_boot_time` stored in each active zone (including the GZ) to
	 * correct the kstats and so that userland software can use this to
	 * obtain a more correct notion of the time that the system, and each
	 * zone, booted.
	 *
	 * To protect somewhat against a system clock being stepped multiple
	 * times forwards and backwards, either by hand or as a result of
	 * an upstream NTP server being authoritatively stuck in the past, we
	 * are only prepared to do this once per boot.
	 */
	if (already_stepped)
		return;

	adj = ts.tv_sec - hrestime.tv_sec;
	if (adj < 86400)
		return;

	already_stepped = true;

	if (boot_time < INT64_MAX - adj)
		boot_time += adj;

	zone_boottime_adjust(adj);

	/*
	 * Call up to each zone's utmpd process and ask it to rewrite
	 * the utmpx and wtmpx databases since the time has stepped
	 * forwards. Since this can take a while we set-up a callback
	 * to avoid holding tod_lock or blocking stime(2).
	 */
	(void) timeout(tod_set_cb, (void *)(uintptr_t)boot_time, 1);
}

timestruc_t
tod_get(void)
{
	timestruc_t ts = { 0 };

	ASSERT(MUTEX_HELD(&tod_lock));

	return (ts);
}

/*
 * The following wrappers have been added so that locking
 * can be exported to platform-independent clock routines
 * (ie adjtime(), clock_setttime()), via a functional interface.
 */
int
hr_clock_lock(void)
{
	ushort_t s;

	CLOCK_LOCK(&s);
	return (s);
}

void
hr_clock_unlock(int s)
{
	CLOCK_UNLOCK(s);
}

void
sgmtl(time_t arg)
{
}

time_t
ggmtl(void)
{
	return (0);
}

void
rtcsync(void)
{
}
