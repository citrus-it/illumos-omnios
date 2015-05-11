/*
 * Copyright 2015 Nexenta Systems, Inc.  All rights reserved.
 */

#include <sys/spa.h>
#include <sys/autosnap.h>
#include <sys/dmu_objset.h>
#include <sys/dsl_pool.h>
#include <sys/dsl_dataset.h>
#include <sys/dsl_destroy.h>
#include <sys/unique.h>

/* AUTOSNAP-recollect routines */

/* Collect orphaned snapshots after reboot */
static int
autosnap_collect_orphaned_snapshots(spa_t *spa)
{
	zfs_autosnap_t *autosnap = spa_get_autosnap(spa);
	zfs_ds_collector_entry_t *el;
	int err;
	dsl_pool_t *dp = spa_get_dsl(spa);
	dsl_dataset_t *ds;
	objset_t *os;
	list_t ds_to_collect;

	list_create(&ds_to_collect, sizeof (zfs_ds_collector_entry_t),
	    offsetof(zfs_ds_collector_entry_t, node));

	dsl_pool_config_enter(dp, FTAG);
	/* collect all datasets of the pool */
	err = zfs_collect_ds(spa, spa_name(spa), B_TRUE, B_FALSE,
	    &ds_to_collect);
	dsl_pool_config_exit(dp, FTAG);

	if (err) {
		list_destroy(&ds_to_collect);
		return (err);
	}

	mutex_enter(&autosnap->autosnap_lock);

	/* iterate through the datasets */
	dsl_pool_config_enter(dp, FTAG);
	for (el = list_head(&ds_to_collect);
	    el != NULL;
	    el = list_head(&ds_to_collect)) {
		dsl_dataset_t *pdss = NULL;
		char name[MAXPATHLEN];
		uint64_t offp = 0, obj = 0;
		boolean_t cc = B_FALSE;

		if (!err)
			err = dsl_dataset_hold(dp, el->name, FTAG, &pdss);

		if (!err)
			err = dmu_objset_from_ds(pdss, &os);

		while (!err) {
			/* iterate through snapshots */
			(void) strcpy(name, el->name);
			(void) strcat(name, "@");

			err = dmu_snapshot_list_next(os,
			    MAXPATHLEN - strlen(name),
			    name + strlen(name), &obj, &offp, &cc);
			if (err == ENOENT) {
				err = 0;
				break;
			}

			if (!err) {
				char *snap = strchr(name, '@') + 1;
				autosnap_snapshot_t *snap_node;
				/* only autosnaps are collected */
				int cmp = strncmp(snap, AUTOSNAP_PREFIX,
				    AUTOSNAP_PREFIX_LEN);

				if (cmp != 0)
					continue;

				err = dsl_dataset_hold(dp, name, FTAG, &ds);
				if (err)
					continue;

				snap_node =
				    kmem_zalloc(sizeof (autosnap_snapshot_t),
				    KM_SLEEP);

				(void) strcpy(snap_node->name, name);
				snap_node->recursive = B_FALSE;
				snap_node->txg =
				    dsl_dataset_phys(ds)->ds_creation_txg;
				snap_node->etxg =
				    dsl_dataset_phys(ds)->ds_creation_txg;
				snap_node->orphaned = B_TRUE;
				list_create(&snap_node->listeners,
				    sizeof (autosnap_handler_t),
				    offsetof(autosnap_handler_t, node));

				avl_add(&autosnap->snapshots, snap_node);

				dsl_dataset_rele(ds, FTAG);
			}
		}

		if (pdss)
			dsl_dataset_rele(pdss, FTAG);
		(void) list_remove_head(&ds_to_collect);
		dsl_dataset_collector_cache_free(el);
	}
	dsl_pool_config_exit(dp, FTAG);
	mutex_exit(&autosnap->autosnap_lock);
	list_destroy(&ds_to_collect);

	return (err);
}

/* Plan to destroy all orphaned snapshots */
void
autosnap_reap_orphaned_snaps(spa_t *spa)
{
	zfs_autosnap_t *autosnap = spa_get_autosnap(spa);
	autosnap_snapshot_t *snap, *prev_snap;

	mutex_enter(&autosnap->autosnap_lock);
	prev_snap = NULL;
	snap = avl_first(&autosnap->snapshots);
	while (snap) {
		prev_snap = snap;
		snap = AVL_NEXT(&autosnap->snapshots, snap);
		if (prev_snap->orphaned && !list_head(&prev_snap->listeners)) {
			avl_remove(&autosnap->snapshots, prev_snap);
			list_insert_tail(&autosnap->autosnap_destroy_queue,
			    prev_snap);
		} else {
			prev_snap->orphaned = B_FALSE;
		}
	}
	mutex_exit(&autosnap->autosnap_lock);
}


/*
 * Return list of the snapshots which are owned by the caller
 * The function is used to reclaim orphaned snapshots
 */
nvlist_t *
autosnap_get_owned_snapshots(void *opaque)
{
	nvlist_t *dup;
	autosnap_snapshot_t *snap;
	autosnap_handler_t *hdl = opaque;
	zfs_autosnap_t *autosnap = hdl->zone->autosnap;

	if (!(hdl->flags & AUTOSNAP_OWNER))
		return (NULL);

	mutex_enter(&autosnap->autosnap_lock);

	if (nvlist_alloc(&dup, NV_UNIQUE_NAME, KM_SLEEP) != 0) {
		mutex_exit(&autosnap->autosnap_lock);
		return (NULL);
	}

	/* iterate though snapshots and find requested */
	for (snap = avl_first(&autosnap->snapshots);
	    snap != NULL;
	    snap = AVL_NEXT(&autosnap->snapshots, snap)) {
		char ds_name[MAXPATHLEN];
		uint64_t data[2];

		if (!snap->orphaned)
			continue;

		(void) strcpy(ds_name, snap->name);
		*(strchr(ds_name, '@')) = '\0';

		if (strcmp(ds_name, hdl->zone->dataset) &&
		    !(hdl->zone->flags & AUTOSNAP_GLOBAL))
			continue;

		data[0] = snap->txg;
		data[1] = snap->recursive;

		if (nvlist_add_uint64_array(dup, snap->name, data, 2) != 0) {
			nvlist_free(dup);
			mutex_exit(&autosnap->autosnap_lock);
			return (NULL);
		}

		snap->orphaned = B_FALSE;
	}

	mutex_exit(&autosnap->autosnap_lock);

	return (dup);
}

static autosnap_handler_t *
autosnap_clone_handler(autosnap_handler_t *hdl)
{
	autosnap_handler_t *clone =
	    kmem_alloc(sizeof (autosnap_handler_t), KM_SLEEP);

	(void) memcpy(clone, hdl, sizeof (autosnap_handler_t));

	return (clone);
}

/*
 * Insert owners handler to snapshots
 */
static void
autosnap_claim_orphaned_snaps(autosnap_handler_t *hdl)
{
	autosnap_zone_t *zone = hdl->zone;
	zfs_autosnap_t *autosnap = zone->autosnap;
	autosnap_snapshot_t *snap, *r_snap = NULL;

	snap = avl_first(&autosnap->snapshots);

	while (snap) {
		char ds_name[MAXPATHLEN];
		autosnap_snapshot_t *next_snap =
		    AVL_NEXT(&autosnap->snapshots, snap);

		if (snap->orphaned) {
			(void) strcpy(ds_name, snap->name);
			*(strchr(ds_name, '@')) = '\0';

			if (strcmp(ds_name, zone->dataset) == 0) {
				list_insert_tail(&snap->listeners,
				    autosnap_clone_handler(hdl));

				r_snap = snap;
			} else if (strncmp(ds_name,
			    zone->dataset, strlen(zone->dataset)) == 0 &&
			    (hdl->flags & AUTOSNAP_RECURSIVE) &&
			    r_snap != NULL) {
				avl_remove(&autosnap->snapshots, snap);
				kmem_free(snap, sizeof (autosnap_snapshot_t));
				r_snap->recursive = B_TRUE;
			}
		}

		snap = next_snap;
	}
}

/*
 * Global mode switch is used by wrc to change mode
 */
void
autosnap_toggle_global_mode(spa_t *spa, boolean_t toggle_on)
{
	zfs_autosnap_t *autosnap = spa_get_autosnap(spa);
	autosnap_zone_t *zone = &autosnap->autosnap_global;

	if (list_head(&zone->listeners) == NULL)
		return;

	if (toggle_on) {
		zone->flags |= AUTOSNAP_CREATOR;
	} else {
		zone->flags &= ~AUTOSNAP_CREATOR;
	}
}

/* AUTOSNAP_RELE routines */

void
autosnap_release_snapshots_by_txg_no_lock_impl(void *opaque, uint64_t from_txg,
    uint64_t to_txg, boolean_t destroy)
{
	autosnap_handler_t *hdl = opaque;
	autosnap_zone_t *zone = hdl->zone;
	zfs_autosnap_t *autosnap = zone->autosnap;
	avl_index_t where;
	int search_len;

	ASSERT(MUTEX_HELD(&autosnap->autosnap_lock));

	autosnap_snapshot_t search = { 0 };
	autosnap_snapshot_t *walker, *prev;

	search.txg = from_txg;
	if (zone->globalized) {
		char *slash;

		(void) strcpy(search.name, zone->dataset);
		slash = strchr(search.name, '/');
		if (slash)
			*slash = '\0';
	} else {
		(void) strcpy(search.name, zone->dataset);
	}
	search_len = strlen(search.name);
	walker = avl_find(&autosnap->snapshots, &search, &where);

	if (!walker) {
		walker = avl_nearest(&autosnap->snapshots,
		    where, AVL_AFTER);
	}

	if (walker == NULL)
		return;

	/* if we specifies only one txg then it must be present */
	if (to_txg == AUTOSNAP_NO_SNAP && walker->txg != from_txg)
		return;

	if (walker->txg < from_txg)
		walker = AVL_NEXT(&autosnap->snapshots, walker);

	if (walker->txg > to_txg)
		return;

	if (to_txg == AUTOSNAP_NO_SNAP)
		to_txg = from_txg;

	/* iterate over the specified range */
	do {
		autosnap_handler_t *tmp_hdl = NULL;
		boolean_t match, exact, pref, skip = B_TRUE;

		match = (strncmp(search.name, walker->name, search_len) == 0);
		if (match) {
			exact = (walker->name[search_len] == '@');
			pref = (walker->name[search_len] == '/');

			skip = !(exact ||
			    (pref && (zone->flags & AUTOSNAP_RECURSIVE)));
		}

		/* find client's entry in a snapshot */
		if (!skip) {
			for (tmp_hdl = list_head(&walker->listeners);
			    tmp_hdl != NULL;
			    tmp_hdl = list_next(&walker->listeners, tmp_hdl)) {
				if (tmp_hdl->mark == hdl->mark)
					break;
			}
		}

		prev = walker;

		walker = AVL_NEXT(&autosnap->snapshots, walker);

		/*
		 * If client holds reference to the snapshot
		 * then remove it
		 */
		if (tmp_hdl) {
			list_remove(&prev->listeners, tmp_hdl);
			kmem_free(tmp_hdl, sizeof (autosnap_handler_t));


			if (!destroy)
				prev->orphaned = B_TRUE;
			/*
			 * If it is a last reference then move
			 * snapshot to the destroyer's queue
			 */
			if (!prev->orphaned &&
			    list_head(&prev->listeners) == NULL) {
				avl_remove(&autosnap->snapshots, prev);
				list_insert_tail(
				    &autosnap->autosnap_destroy_queue,
				    prev);
				cv_broadcast(&autosnap->autosnap_cv);
			}
		}

	} while (walker && walker->txg <= to_txg);
}

/* No lock version should be used from autosnap callbacks */
void
autosnap_release_snapshots_by_txg_no_lock(void *opaque,
    uint64_t from_txg, uint64_t to_txg)
{
	autosnap_release_snapshots_by_txg_no_lock_impl(opaque,
	    from_txg, to_txg, B_TRUE);
}

/*
 * Release snapshot and remove a handler from it
 */
void
autosnap_release_snapshots_by_txg(void *opaque,
    uint64_t from_txg, uint64_t to_txg)
{
	autosnap_handler_t *hdl = opaque;
	autosnap_zone_t *zone = hdl->zone;
	mutex_enter(&zone->autosnap->autosnap_lock);
	autosnap_release_snapshots_by_txg_no_lock(opaque, from_txg, to_txg);
	mutex_exit(&zone->autosnap->autosnap_lock);
}

static int
snapshot_txg_compare(const void *arg1, const void *arg2)
{
	const autosnap_snapshot_t *snap1 = arg1;
	const autosnap_snapshot_t *snap2 = arg2;

	if (snap1->txg < snap2->txg) {
		return (-1);
	} else if (snap1->txg == snap2->txg) {
		int res = 0;
		int l1 = strlen(snap1->name);
		int l2 = strlen(snap2->name);
		int i;

		/* we need our own strcmp to ensure depth-first order */
		for (i = 0; i <= MIN(l1, l2); i++) {
			char c1 = snap1->name[i];
			char c2 = snap2->name[i];

			if (c1 != c2) {
				if (c1 == '\0') {
					res = -1;
				} else if (c2 == '\0') {
					res = +1;
				} else if (c1 == '@') {
					res = -1;
				} else if (c2 == '@') {
					res = +1;
				} else if (c1 == '/') {
					res = -1;
				} else if (c2 == '/') {
					res = +1;
				} else if (c1 < c2) {
					res = -1;
				} else {
					res = +1;
				}
				break;
			}
		}

		if (res < 0) {
			return (-1);
		} else if (res > 0) {
			return (+1);
		} else {
			return (0);
		}
	} else {
		return (+1);
	}
}

/* AUTOSNAP-HDL routines */

void *
autosnap_register_handler(const char *name, uint64_t flags,
    autosnap_confirm_cb confirm_cb,
    autosnap_notify_created_cb nc_cb,
    autosnap_error_cb err_cb, void *cb_arg)
{
	spa_t *spa;
	autosnap_handler_t *hdl = NULL;
	autosnap_zone_t *zone = NULL;
	autosnap_zone_t *rzone = NULL;
	boolean_t children_zone, has_gzone;
	zfs_autosnap_t *autosnap;
	boolean_t namespace_alteration = B_TRUE;

	if (nc_cb == NULL)
		return (NULL);

	/* special case for unregistering on deletion */
	if (!MUTEX_HELD(&spa_namespace_lock)) {
		mutex_enter(&spa_namespace_lock);
		namespace_alteration = B_FALSE;
	}

	spa = spa_lookup(name);

	if (!spa)
		goto out;

	autosnap = spa_get_autosnap(spa);

	mutex_enter(&autosnap->autosnap_lock);

	has_gzone = list_head(&autosnap->autosnap_global.listeners) != NULL;

	/* Look for zone */
	if (flags & AUTOSNAP_GLOBAL) {
		zone = &autosnap->autosnap_global;
		children_zone = autosnap_has_children_zone(spa, name);
	} else {
		zone = autosnap_find_zone(spa, name, B_FALSE);
		rzone = autosnap_find_zone(spa, name, B_TRUE);
		children_zone = autosnap_has_children_zone(spa, name);
	}

	if (rzone && !zone) {
		cmn_err(CE_WARN, "AUTOSNAP: the dataset is already under"
		    " an autosnap zone [%s under %s]\n",
		    name, rzone->dataset);
		goto out;
	} else if (has_gzone && !zone && (strchr(name, '/') != NULL ||
	    !(flags & AUTOSNAP_RECURSIVE))) {
		cmn_err(CE_WARN, "AUTOSNAP: can't register non-root"
		    " or non-recursive zone when wrc is present %s\n",
		    name);
		goto out;
	} else if (children_zone && (flags & AUTOSNAP_RECURSIVE)) {
		cmn_err(CE_WARN, "AUTOSNAP: can't register recursive zone"
		    " when there is a child under autosnap%s\n",
		    name);
		goto out;
	}

	/* Create a new zone if it is absent */
	if (!zone) {
		zone = kmem_zalloc(sizeof (autosnap_zone_t), KM_SLEEP);
		(void) strcpy(zone->dataset, name);

		list_create(&zone->listeners,
		    sizeof (autosnap_handler_t),
		    offsetof(autosnap_handler_t, node));

		zone->created = B_FALSE;
		zone->flags = 0;
		zone->autosnap = autosnap;
		zone->delayed = B_FALSE;
		zone->globalized = B_FALSE;
		list_insert_tail(&autosnap->autosnap_zones, zone);
	} else {
		if ((list_head(&zone->listeners) != NULL) &&
		    ((flags & AUTOSNAP_CREATOR) ^
		    (zone->flags & AUTOSNAP_CREATOR))) {
			cmn_err(CE_WARN,
			    "AUTOSNAP: can't register two different"
			    " modes for the same autosnap zone %s %s\n",
			    name, flags & AUTOSNAP_RECURSIVE ? "[r]" : "");
			goto out;
		} else if ((list_head(&zone->listeners) != NULL) &&
		    ((flags & AUTOSNAP_RECURSIVE) ^
		    (zone->flags & AUTOSNAP_RECURSIVE))) {
			cmn_err(CE_WARN,
			    "AUTOSNAP: can't register two different"
			    " recursion modes for the same autosnap zone "
			    "%s %s\n",
			    name, flags & AUTOSNAP_RECURSIVE ? "[r]" : "");
			goto out;
		}
	}

	zone->flags |= flags;

	hdl = kmem_zalloc(sizeof (autosnap_handler_t), KM_SLEEP);

	hdl->confirm_cb = confirm_cb;
	hdl->nc_cb = nc_cb;
	hdl->err_cb = err_cb;
	hdl->cb_arg = cb_arg;
	hdl->zone = zone;
	hdl->flags = flags;
	hdl->mark = unique_create();

	list_insert_tail(&zone->listeners, hdl);

	if (flags & AUTOSNAP_OWNER)
		autosnap_claim_orphaned_snaps(hdl);

out:
	if (spa)
		mutex_exit(&autosnap->autosnap_lock);
	if (!namespace_alteration)
		mutex_exit(&spa_namespace_lock);
	return (hdl);
}

void
autosnap_unregister_handler(void *opaque)
{
	spa_t *spa;
	autosnap_handler_t *hdl = opaque;
	autosnap_zone_t *zone = hdl->zone;
	zfs_autosnap_t *autosnap = NULL;
	boolean_t namespace_alteration = B_TRUE;

	/* special case for unregistering on deletion */
	if (!MUTEX_HELD(&spa_namespace_lock)) {
		mutex_enter(&spa_namespace_lock);
		namespace_alteration = B_FALSE;
	}

	spa = spa_lookup(zone->dataset);

	/* if zone is absent, then just destroy handler */
	if (!spa) {
		zone = NULL;
		goto free_hdl;
	}

	autosnap = spa_get_autosnap(spa);

	mutex_enter(&autosnap->autosnap_lock);

	autosnap_release_snapshots_by_txg_no_lock_impl(
	    opaque, AUTOSNAP_FIRST_SNAP, AUTOSNAP_LAST_SNAP,
	    B_FALSE);

free_hdl:

	/*
	 * Remove the client from zone. If it is a last client then destroy the
	 * zone. If some clients left but there are no owners among them, then
	 * unset the flag from the zone.
	 */
	if (zone) {
		list_remove(&zone->listeners, hdl);

		if (list_head(&zone->listeners) == NULL &&
		    !(zone->flags & AUTOSNAP_GLOBAL)) {
			list_remove(&autosnap->autosnap_zones, zone);
			list_destroy(&zone->listeners);
			kmem_free(zone, sizeof (autosnap_zone_t));
			zone = NULL;
		} else {
			autosnap_handler_t *walk;

			for (walk = list_head(&zone->listeners);
			    walk != NULL;
			    walk = list_next(&zone->listeners, walk)) {
				if (walk->flags & AUTOSNAP_OWNER)
					break;
			}

			if (walk == NULL)
				zone->flags &= ~AUTOSNAP_OWNER;
		}
	}

	unique_remove(hdl->mark);
	kmem_free(hdl, sizeof (autosnap_handler_t));

out:
	if (spa)
		mutex_exit(&autosnap->autosnap_lock);
	if (!namespace_alteration)
		mutex_exit(&spa_namespace_lock);
}

boolean_t
autosnap_has_children_zone(spa_t *spa, const char *name)
{
	char dataset[MAXPATHLEN];
	char *snapshot;
	autosnap_zone_t *zone;
	zfs_autosnap_t *autosnap = spa_get_autosnap(spa);

	ASSERT(MUTEX_HELD(&autosnap->autosnap_lock));

	(void) strcpy(dataset, name);
	snapshot = strchr(dataset, '@');
	if (snapshot != NULL)
		*snapshot++ = '\0';

	for (zone = list_head(&autosnap->autosnap_zones);
	    zone != NULL;
	    zone = list_next(&autosnap->autosnap_zones, zone)) {
		int cmp = strncmp(dataset, zone->dataset,
		    strlen(dataset));
		if (cmp == 0)
			return (B_TRUE);
	}


	return (B_FALSE);
}

autosnap_zone_t *
autosnap_find_zone(spa_t *spa, const char *name, boolean_t recursive)
{
	char dataset[MAXPATHLEN];
	char *snapshot;
	autosnap_zone_t *zone;
	zfs_autosnap_t *autosnap = spa_get_autosnap(spa);

	ASSERT(MUTEX_HELD(&autosnap->autosnap_lock));

	(void) strcpy(dataset, name);
	snapshot = strchr(dataset, '@');

	if (snapshot != NULL)
		*snapshot++ = '\0';

	for (zone = list_head(&autosnap->autosnap_zones);
	    zone != NULL;
	    zone = list_next(&autosnap->autosnap_zones, zone)) {
		int cmp = strncmp(dataset, zone->dataset,
		    strlen(zone->dataset));
		if (strcmp(dataset, zone->dataset) == 0)
			break;
		if (recursive && cmp == 0 &&
		    (zone->flags & AUTOSNAP_RECURSIVE))
			break;
	}


	return (zone);
}

/* AUTOSNAP-LOCK routines */

int
autosnap_lock(spa_t *spa)
{
	zfs_autosnap_t *autosnap = spa_get_autosnap(spa);
	int err = 0;

	mutex_enter(&autosnap->autosnap_lock);

	while (autosnap->locked && !autosnap->need_stop) {
		(void) cv_wait(&autosnap->autosnap_cv,
		    &autosnap->autosnap_lock);
	}

	if (autosnap->need_stop) {
		err = ENOLCK;
	} else {
		autosnap->locked = B_TRUE;
	}

	cv_broadcast(&autosnap->autosnap_cv);
	mutex_exit(&autosnap->autosnap_lock);

	return (err);
}

void
autosnap_unlock(spa_t *spa)
{
	zfs_autosnap_t *autosnap = spa_get_autosnap(spa);

	mutex_enter(&autosnap->autosnap_lock);
	ASSERT(autosnap->locked);

	autosnap->locked = B_FALSE;

	cv_signal(&autosnap->autosnap_cv);
	mutex_exit(&autosnap->autosnap_lock);
}

/* AUTOSNAP-FSNAP routines */

void
autosnap_exempt_snapshot(spa_t *spa, const char *name)
{
	zfs_autosnap_t *autosnap = spa_get_autosnap(spa);
	uint64_t txg;
	int err;
	dsl_dataset_t *ds;
	autosnap_snapshot_t search = { 0 }, *found;

	err = dsl_dataset_hold(spa_get_dsl(spa), name, FTAG, &ds);
	if (err) {
		txg = UINT64_MAX;
	} else {
		txg = dsl_dataset_phys(ds)->ds_creation_txg;
		dsl_dataset_rele(ds, FTAG);
	}

	mutex_enter(&autosnap->autosnap_lock);

	(void) strcpy(search.name, name);
	search.txg = txg;

	found = avl_find(&autosnap->snapshots, &search, NULL);

	if (found) {
		autosnap_handler_t *hdl;

		while ((hdl = list_remove_head(&found->listeners)) != NULL)
			kmem_free(hdl, sizeof (autosnap_handler_t));
		avl_remove(&autosnap->snapshots, found);
		kmem_free(found, sizeof (autosnap_snapshot_t));
	}

	mutex_exit(&autosnap->autosnap_lock);
}

void
autosnap_force_snap_by_name(const char *dsname, boolean_t sync)
{
	dsl_pool_t *dp;
	dsl_dataset_t *ds;
	objset_t *os;
	uint64_t txg = 0;
	autosnap_zone_t *zone;
	zfs_autosnap_t *autosnap;
	int error;

	error = dsl_pool_hold(dsname, FTAG, &dp);
	if (error)
		return;

	autosnap = spa_get_autosnap(dp->dp_spa);
	if (!autosnap) {
		dsl_pool_rele(dp, FTAG);
		return;
	}

	mutex_enter(&autosnap->autosnap_lock);
	zone = autosnap_find_zone(dp->dp_spa, dsname, B_TRUE);
	if (zone == NULL) {
		mutex_exit(&autosnap->autosnap_lock);
		dsl_pool_rele(dp, FTAG);
		return;
	}

	error = dsl_dataset_hold(dp, dsname, FTAG, &ds);
	if (error) {
		mutex_exit(&autosnap->autosnap_lock);
		dsl_pool_rele(dp, FTAG);
		return;
	}
	error = dmu_objset_from_ds(ds, &os);
	if (error) {
		dsl_dataset_rele(ds, FTAG);
		mutex_exit(&autosnap->autosnap_lock);
		dsl_pool_rele(dp, FTAG);
		return;
	}
	if (dmu_objset_is_snapshot(os)) {
		dsl_dataset_rele(ds, FTAG);
		mutex_exit(&autosnap->autosnap_lock);
		dsl_pool_rele(dp, FTAG);
		return;
	}

	dsl_pool_rele(dp, FTAG);

	if (zone->flags & AUTOSNAP_CREATOR) {
		dmu_tx_t *tx = dmu_tx_create(os);

		error = dmu_tx_assign(tx, TXG_NOWAIT);

		if (error) {
			dmu_tx_abort(tx);
			dsl_dataset_rele(ds, FTAG);
			mutex_exit(&autosnap->autosnap_lock);
			return;
		}

		txg = dmu_tx_get_txg(tx);
		dsl_dataset_dirty(ds, tx);
		dmu_tx_commit(tx);
	}

	dsl_dataset_rele(ds, FTAG);
	mutex_exit(&autosnap->autosnap_lock);

	if (sync)
		txg_wait_synced(dp, txg);
}

/* Force creation of an autosnap */
void
autosnap_force_snap(void *opaque, boolean_t sync)
{
	autosnap_handler_t *hdl;
	autosnap_zone_t *zone;

	if (!opaque)
		return;

	hdl = opaque;
	zone = hdl->zone;

	autosnap_force_snap_by_name(zone->dataset, sync);
}

/* AUTOSNAP-NOTIFIER routines */

/* iterate through handlers and call its confirm callbacks */
boolean_t
autosnap_confirm_snap(const char *name, uint64_t txg, autosnap_zone_t *zone)
{
	autosnap_handler_t *hdl;
	boolean_t confirmation = B_FALSE;

	if ((zone->flags & AUTOSNAP_CREATOR) == 0)
		return (B_FALSE);

	for (hdl = list_head(&zone->listeners);
	    hdl != NULL;
	    hdl = list_next(&zone->listeners, hdl)) {
		confirmation |=
		    hdl->confirm_cb == NULL ? B_TRUE :
		    hdl->confirm_cb(name, !!(zone->flags & AUTOSNAP_RECURSIVE),
		    txg, hdl->cb_arg);
	}

	return (confirmation);
}

/* iterate through handlers and call its error callbacks */
void
autosnap_error_snap(const char *name, int err, uint64_t txg,
    autosnap_zone_t *zone)
{
	autosnap_handler_t *hdl;

	for (hdl = list_head(&zone->listeners);
	    hdl != NULL;
	    hdl = list_next(&zone->listeners, hdl)) {
		if (hdl->err_cb)
			hdl->err_cb(name, err, txg, hdl->cb_arg);
	}
}

static boolean_t
autosnap_contains_handler(list_t *listeners, autosnap_handler_t *chdl)
{
	autosnap_handler_t *hdl;

	for (hdl = list_head(listeners);
	    hdl != NULL;
	    hdl = list_next(listeners, hdl)) {
		if (hdl->mark == chdl->mark)
			return (B_TRUE);
	}

	return (B_FALSE);
}

/* iterate through handlers and call its notify callbacks */
static void
autosnap_iterate_listeners(autosnap_zone_t *zone, autosnap_snapshot_t *snap,
    boolean_t globalized, boolean_t destruction)
{
	autosnap_handler_t *hdl;

	if (globalized)
		zone->globalized = B_TRUE;
	for (hdl = list_head(&zone->listeners);
	    hdl != NULL;
	    hdl = list_next(&zone->listeners, hdl)) {
		if (!hdl->nc_cb(snap->name,
		    !!(zone->flags & AUTOSNAP_RECURSIVE),
		    B_TRUE, snap->txg, snap->etxg, hdl->cb_arg))
			continue;
		if (destruction &&
		    !autosnap_contains_handler(&snap->listeners, hdl)) {
			list_insert_tail(&snap->listeners,
			    autosnap_clone_handler(hdl));
		}
	}
}

/* Notify listeners about creation of a global autosnapshot */
void
autosnap_notify_created_globalized(const char *snapname,
    uint64_t ftxg, uint64_t ttxg, zfs_autosnap_t *autosnap)
{
	autosnap_snapshot_t *snapshot, search;
	autosnap_zone_t *zone;
	boolean_t destruction;
	boolean_t asnap = B_FALSE;
	boolean_t resumed = B_FALSE;
	char *snap_start = strchr(snapname, '@') + 1;

	ASSERT(MUTEX_HELD(&autosnap->autosnap_lock));

	(void) strcpy(search.name, snapname);
	search.txg = ftxg;
	search.etxg = ttxg;

	snapshot = avl_find(&autosnap->snapshots, &search, NULL);

	if (snapshot) {
		resumed = B_TRUE;
	} else {
		snapshot = kmem_zalloc(sizeof (autosnap_snapshot_t), KM_SLEEP);
		(void) strcpy(snapshot->name, snapname);
		snapshot->recursive = B_TRUE;
		list_create(&snapshot->listeners, sizeof (autosnap_handler_t),
		    offsetof(autosnap_handler_t, node));
	}
	snapshot->txg = ftxg;
	snapshot->etxg = ttxg;

	if (strncmp(snap_start, AUTOSNAP_PREFIX, AUTOSNAP_PREFIX_LEN) == 0)
		asnap = B_TRUE;

	for (zone = list_head(&autosnap->autosnap_zones);
	    zone != NULL;
	    zone = list_next(&autosnap->autosnap_zones, zone)) {
		destruction = asnap && !!(zone->flags & AUTOSNAP_DESTROYER);
		autosnap_iterate_listeners(zone, snapshot, B_TRUE, destruction);
	}

	destruction = asnap &&
	    !!(autosnap->autosnap_global.flags & AUTOSNAP_DESTROYER);
	autosnap_iterate_listeners(&autosnap->autosnap_global, snapshot,
	    B_TRUE, destruction);

	if (asnap) {
		if (list_head(&snapshot->listeners) != NULL) {
			if (!resumed)
				avl_add(&autosnap->snapshots, snapshot);
		} else {
			list_insert_tail(
			    &autosnap->autosnap_destroy_queue, snapshot);
			cv_broadcast(&autosnap->autosnap_cv);
		}
	} else {
		kmem_free(snapshot, sizeof (autosnap_snapshot_t));
	}
}

/* Notify listeners about a received snapshot */
void
autosnap_notify_received(const char *name)
{
	uint64_t ftxg;
	uint64_t ttxg = dsl_dataset_creation_txg(name);
	spa_t *spa;
	zfs_autosnap_t *autosnap;
	char *snap = strchr(name, '@');
	char *slash;
	char snapname[MAXPATHLEN];

	if (strncmp(snap + 1, AUTOSNAP_PREFIX, AUTOSNAP_PREFIX_LEN))
		return;

	(void) strcpy(snapname, name);
	slash = strchr(snapname, '/');
	if (slash)
		(void) strcpy(slash, snap);

	mutex_enter(&spa_namespace_lock);
	spa = spa_lookup(name);
	mutex_exit(&spa_namespace_lock);

	if (!spa)
		return;

	autosnap = spa_get_autosnap(spa);

	ftxg = dsl_dataset_creation_txg(snapname);
	mutex_enter(&autosnap->autosnap_lock);
	autosnap_notify_created_globalized(snapname, ftxg, ttxg, autosnap);
	mutex_exit(&autosnap->autosnap_lock);
}

/* Notify listeners about an autosnapshot */
void
autosnap_notify_created(const char *name, uint64_t txg,
    autosnap_zone_t *zone)
{
	autosnap_snapshot_t *snapshot = NULL, search;
	boolean_t found = B_FALSE;
	char *snapname = strchr(name, '@');
	boolean_t autosnap = B_FALSE;
	boolean_t destruction = B_TRUE;

	ASSERT(MUTEX_HELD(&zone->autosnap->autosnap_lock));

	if (!snapname)
		return;

	snapname++;

	if (strncmp(snapname, AUTOSNAP_PREFIX, AUTOSNAP_PREFIX_LEN) == 0)
		autosnap = B_TRUE;

	destruction = (autosnap && (!!(zone->flags & AUTOSNAP_DESTROYER)));

	search.txg = txg;
	(void) strcpy(search.name, name);
	snapshot = avl_find(&zone->autosnap->snapshots, &search, NULL);
	if (snapshot) {
		found = B_TRUE;
	} else {
		snapshot = kmem_zalloc(sizeof (autosnap_snapshot_t), KM_SLEEP);
		(void) strcpy(snapshot->name, name);
		snapshot->txg = txg;
		snapshot->etxg = txg;
		snapshot->recursive =
		    !!(zone->flags & (AUTOSNAP_RECURSIVE | AUTOSNAP_GLOBAL));
		list_create(&snapshot->listeners, sizeof (autosnap_handler_t),
		    offsetof(autosnap_handler_t, node));
	}

	autosnap_iterate_listeners(zone, snapshot, B_FALSE, destruction);

	if (destruction) {
		if (list_head(&snapshot->listeners) != NULL) {
			if (!found)
				avl_add(&zone->autosnap->snapshots, snapshot);
		} else {
			list_insert_tail(
			    &zone->autosnap->autosnap_destroy_queue, snapshot);
			cv_broadcast(&zone->autosnap->autosnap_cv);
		}
	} else if (!found) {
		kmem_free(snapshot, sizeof (autosnap_snapshot_t));
	}
}

/* Reject a creation of an autosnapshot */
void
autosnap_reject_snap(const char *name, uint64_t txg, zfs_autosnap_t *autosnap)
{
	char *snapname = strchr(name, '@');
	autosnap_snapshot_t *snapshot = NULL;

	ASSERT(MUTEX_HELD(&autosnap->autosnap_lock));

	if (!snapname)
		return;
	snapname++;
	if (strncmp(snapname, AUTOSNAP_PREFIX, AUTOSNAP_PREFIX_LEN))
		return;

	snapshot = kmem_zalloc(sizeof (autosnap_snapshot_t), KM_SLEEP);
	(void) strcpy(snapshot->name, name);
	snapshot->txg = txg;
	snapshot->etxg = txg;
	snapshot->recursive = B_FALSE;

	list_insert_tail(
	    &autosnap->autosnap_destroy_queue, snapshot);
	cv_broadcast(&autosnap->autosnap_cv);
}

/* AUTOSNAP-DESTROYER routines */

/* Collect snapshots for destroy */
static int
dsl_pool_collect_ds_for_autodestroy(spa_t *spa, const char *root_ds,
    const char *snap_name, boolean_t recursive, nvlist_t *nv_auto)
{
	list_t ds_to_send;
	zfs_ds_collector_entry_t *el;
	dsl_pool_t *dp = spa_get_dsl(spa);
	int err;

	list_create(&ds_to_send, sizeof (zfs_ds_collector_entry_t),
	    offsetof(zfs_ds_collector_entry_t, node));

	dsl_pool_config_enter(dp, FTAG);
	err = zfs_collect_ds(spa, root_ds, recursive, B_FALSE, &ds_to_send);
	dsl_pool_config_exit(dp, FTAG);

	for (el = list_head(&ds_to_send);
	    err == 0 && el != NULL;
	    el = list_head(&ds_to_send)) {
		if (!err) {
			(void) strcat(el->name, "@");
			(void) strcat(el->name, snap_name);

			err = nvlist_add_boolean(nv_auto, el->name);
		}

		(void) list_remove_head(&ds_to_send);
		dsl_dataset_collector_cache_free(el);
	}
	list_destroy(&ds_to_send);

	return (err);
}

void
autosnap_destroyer_thread(spa_t *spa)
{
	zfs_autosnap_t *autosnap = spa_get_autosnap(spa);

	mutex_enter(&autosnap->autosnap_lock);
	while (!autosnap->need_stop) {
		nvlist_t *nvl = fnvlist_alloc();
		nvlist_t *errlist = fnvlist_alloc();
		nvpair_t *pair;
		autosnap_snapshot_t *snapshot, *tmp;
		int err;

		if (list_head(&autosnap->autosnap_destroy_queue) == NULL) {
			cv_wait(&autosnap->autosnap_cv,
			    &autosnap->autosnap_lock);
		}

		if (autosnap->need_stop) {
			fnvlist_free(errlist);
			fnvlist_free(nvl);
			break;
		}

		/* iterate through list of snapshots to be destroyed */
		snapshot = list_head(&autosnap->autosnap_destroy_queue);
		while (snapshot) {
			char ds[MAXPATHLEN];
			char *snap;

			(void) strcpy(ds, snapshot->name);
			snap = strchr(ds, '@');
			VERIFY(snap != NULL);
			*snap++ = '\0';

			mutex_exit(&autosnap->autosnap_lock);
			err = dsl_pool_collect_ds_for_autodestroy(spa, ds, snap,
			    snapshot->recursive, nvl);
			mutex_enter(&autosnap->autosnap_lock);
			if (err)
				break;

			tmp = list_next(&autosnap->autosnap_destroy_queue,
			    snapshot);
			list_remove(&autosnap->autosnap_destroy_queue,
			    snapshot);
			kmem_free(snapshot, sizeof (autosnap_snapshot_t));
			snapshot = tmp;
		}

		/* destroy pack of snpashots */
		mutex_exit(&autosnap->autosnap_lock);
		err = dsl_destroy_snapshots_nvl(nvl, B_TRUE, errlist);
		mutex_enter(&autosnap->autosnap_lock);

		/* return not destroyed snapshots to the queue */
		if (err) {
			for (pair = nvlist_next_nvpair(errlist, NULL);
			    pair != NULL;
			    pair = nvlist_next_nvpair(errlist, pair)) {
				cmn_err(CE_WARN,
				    "Can't destroy snapshots %s : [%d]\n",
				    nvpair_name(pair),
				    fnvpair_value_int32(pair));
				if (err != EBUSY && err != EEXIST)
					continue;
				snapshot = kmem_zalloc(
				    sizeof (autosnap_snapshot_t), KM_SLEEP);
				(void) strcpy(snapshot->name,
				    nvpair_name(pair));
				snapshot->recursive = B_FALSE;
				list_insert_tail(
				    &autosnap->autosnap_destroy_queue,
				    snapshot);
			}
		}

		fnvlist_free(errlist);
		fnvlist_free(nvl);
	}
	mutex_exit(&autosnap->autosnap_lock);
}

void
autosnap_destroyer_thread_start(spa_t *spa)
{
	zfs_autosnap_t *autosnap = spa_get_autosnap(spa);

	autosnap->destroyer = thread_create(NULL, 32 << 10,
	    autosnap_destroyer_thread, spa, 0, &p0,
	    TS_RUN, minclsyspri);
}

void
autosnap_destroyer_thread_stop(spa_t *spa)
{
	zfs_autosnap_t *autosnap = spa_get_autosnap(spa);

	if (!autosnap->initialized)
		return;

	mutex_enter(&autosnap->autosnap_lock);
	if (autosnap->need_stop || !autosnap->destroyer) {
		mutex_exit(&autosnap->autosnap_lock);
		return;
	}
	autosnap->need_stop = B_TRUE;
	cv_broadcast(&autosnap->autosnap_cv);
	mutex_exit(&autosnap->autosnap_lock);
#ifdef _KERNEL
	thread_join(autosnap->destroyer->t_did);
#endif
	autosnap->destroyer = NULL;
}

/* AUTOSNAP-INIT routines */

void
autosnap_init(spa_t *spa)
{
	zfs_autosnap_t *autosnap = spa_get_autosnap(spa);
	mutex_init(&autosnap->autosnap_lock, NULL, MUTEX_ADAPTIVE, NULL);
	cv_init(&autosnap->autosnap_cv, NULL, CV_DEFAULT, NULL);
	list_create(&autosnap->autosnap_zones, sizeof (autosnap_zone_t),
	    offsetof(autosnap_zone_t, node));
	list_create(&autosnap->autosnap_destroy_queue,
	    sizeof (autosnap_snapshot_t),
	    offsetof(autosnap_snapshot_t, dnode));
	autosnap->need_stop = B_FALSE;

	avl_create(&autosnap->snapshots,
	    snapshot_txg_compare,
	    sizeof (autosnap_snapshot_t),
	    offsetof(autosnap_snapshot_t, node));

	(void) strcpy(autosnap->autosnap_global.dataset, spa_name(spa));

	list_create(&autosnap->autosnap_global.listeners,
	    sizeof (autosnap_handler_t),
	    offsetof(autosnap_handler_t, node));

	autosnap->autosnap_global.created = B_FALSE;
	autosnap->autosnap_global.delayed = B_FALSE;
	autosnap->autosnap_global.flags = AUTOSNAP_GLOBAL;
	autosnap->autosnap_global.autosnap = autosnap;

	(void) autosnap_collect_orphaned_snapshots(spa);

#ifdef _KERNEL
	autosnap_destroyer_thread_start(spa);
#endif

	autosnap->initialized = B_TRUE;
}

void
autosnap_fini(spa_t *spa)
{
	zfs_autosnap_t *autosnap = spa_get_autosnap(spa);
	autosnap_zone_t *zone;
	autosnap_handler_t *hdl;
	autosnap_snapshot_t *snap;
	void *cookie = NULL;

	if (!autosnap->initialized)
		return;

	if (autosnap->destroyer)
		autosnap_destroyer_thread_stop(spa);

	autosnap->initialized = B_FALSE;

	while ((zone = list_head(&autosnap->autosnap_zones)) != NULL) {
		while ((hdl = list_head(&zone->listeners)) != NULL)
			autosnap_unregister_handler(hdl);
	}

	while ((hdl = list_head(&autosnap->autosnap_global.listeners)) != NULL)
		autosnap_unregister_handler(hdl);

	while ((snap =
	    avl_destroy_nodes(&autosnap->snapshots, &cookie)) != NULL)
		kmem_free(snap, sizeof (*snap));

	list_destroy(&autosnap->autosnap_global.listeners);
	avl_destroy(&autosnap->snapshots);

	while ((snap =
	    list_remove_head(&autosnap->autosnap_destroy_queue)) != NULL)
		kmem_free(snap, sizeof (*snap));
	list_destroy(&autosnap->autosnap_destroy_queue);
	list_destroy(&autosnap->autosnap_zones);
	mutex_destroy(&autosnap->autosnap_lock);
	cv_destroy(&autosnap->autosnap_cv);
}
