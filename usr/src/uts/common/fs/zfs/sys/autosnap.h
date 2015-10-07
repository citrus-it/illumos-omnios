/*
 * Copyright 2015 Nexenta Systems, Inc. All rights reserved.
 */
#ifndef _SYS_AUTOSNAP_H
#define	_SYS_AUTOSNAP_H

#include <sys/dsl_dataset.h>
#include <sys/spa.h>
#include <sys/nvpair.h>
#include <sys/list.h>
#include <sys/avl.h>

#ifdef	__cplusplus
extern "C" {
#endif

typedef boolean_t (*autosnap_confirm_cb)(const char *name, boolean_t recursive,
    uint64_t txg, void *arg);
typedef boolean_t (*autosnap_notify_created_cb)(const char *name,
    boolean_t recursive, boolean_t autosnap, uint64_t txg, uint64_t etxg,
    void *arg);
typedef void (*autosnap_error_cb)(const char *name, int err,
    uint64_t txg, void *arg);

typedef struct autosnap_snapshot {
	avl_node_t node; /* for release */
	list_node_t dnode; /* for destroyer */
	char name[MAXPATHLEN];
	boolean_t recursive;
	uint64_t txg;
	uint64_t etxg;
	list_t listeners;
	boolean_t orphaned;
} autosnap_snapshot_t;

typedef struct zfs_autosnap zfs_autosnap_t;

/* The zone lock protects the list of the listeners */
/* Pools are distinguished by dataset and prefix */
typedef struct autosnap_zone {
	list_node_t node;
	char dataset[MAXPATHLEN];
	uint64_t flags;
	list_t listeners;
	zfs_autosnap_t *autosnap;
	boolean_t created;
	boolean_t delayed;
	boolean_t globalized;
} autosnap_zone_t;

struct zfs_autosnap {
	avl_tree_t snapshots;
	kmutex_t autosnap_lock;
	kcondvar_t autosnap_cv;
	list_t autosnap_zones;
	list_t autosnap_destroy_queue;
	autosnap_zone_t autosnap_global;
	kthread_t *destroyer;
	boolean_t need_stop;
	boolean_t initialized;
	boolean_t locked;
};

/*
 * confirm_cb - should snapshot be created
 * nc_cb - snapshot is created
 * err_cb - can't create snapshot
 * Client must not rely on confirm_cb to store
 * information about existing snapshots. This
 * callback's call can be ommited for any client
 * if autosnap decides that it has enough data or
 * in no_creation case. The only reliable way to
 * know about snapshots that are created by autosnap
 * is nc_cb. Also, releasing snapshot doesn't destroy
 * a snapshot. After all references to a snapshot are
 * dropped, it is moved to destroyer's queue and
 * destroyed asynchronously.
 */
typedef struct autosnap_handler {
	list_node_t node;
	autosnap_confirm_cb confirm_cb;
	autosnap_notify_created_cb nc_cb;
	autosnap_error_cb err_cb;
	void *cb_arg;
	uint64_t mark;
	uint64_t flags;
	autosnap_zone_t *zone;
} autosnap_handler_t;

extern void *autosnap_register_handler(const char *name, uint64_t flags,
    autosnap_confirm_cb confirm_cb,
    autosnap_notify_created_cb nc_cb,
    autosnap_error_cb, void *cb_arg);
extern void autosnap_unregister_handler(void *opaque);
extern autosnap_zone_t *autosnap_find_zone(spa_t *spa, const char *name,
    boolean_t recursive);
extern boolean_t autosnap_has_children_zone(spa_t *spa, const char *name);
extern void autosnap_exempt_snapshot(spa_t *spa, const char *name);
extern void autosnap_force_snap_by_name(const char *dsname,
    autosnap_zone_t *zone, boolean_t sync);
extern void autosnap_force_snap(void *opaque, boolean_t sync);
extern boolean_t autosnap_confirm_snap(const char *name, uint64_t txg,
    autosnap_zone_t *zone);
extern void autosnap_error_snap(const char *name, int err, uint64_t txg,
    autosnap_zone_t *zone);
extern void autosnap_notify_created(const char *name, uint64_t txg,
    autosnap_zone_t *zone);
extern void autosnap_notify_created_globalized(const char *snapname,
    uint64_t ftxg, uint64_t ttxg, zfs_autosnap_t *autosnap);
extern void autosnap_notify_received(const char *name);
extern void autosnap_reject_snap(const char *name, uint64_t txg,
    zfs_autosnap_t *autosnap);

#define	AUTOSNAP_PREFIX ".autosnap_"
#define	AUTOSNAP_PREFIX_LEN (sizeof (AUTOSNAP_PREFIX) - 1)
#define	AUTOSNAP_NO_SNAP UINT64_MAX
#define	AUTOSNAP_LAST_SNAP (UINT64_MAX-1)
#define	AUTOSNAP_FIRST_SNAP 0x0

typedef enum autosnap_flags {
	AUTOSNAP_RECURSIVE	= 1 << 0,
	AUTOSNAP_CREATOR	= 1 << 1,
	AUTOSNAP_DESTROYER	= 1 << 2,
	AUTOSNAP_GLOBAL		= 1 << 3,
	AUTOSNAP_OWNER		= 1 << 4
} autosnap_flags_t;

/*
 * No lock version should be called if and only if a
 * snapshot should be released in nc_cb context
 */
extern void autosnap_release_snapshots_by_txg(void *opaque,
    uint64_t from_txg, uint64_t to_txg);
extern void autosnap_release_snapshots_by_txg_no_lock(void *opaque,
    uint64_t from_txg, uint64_t to_txg);

extern nvlist_t *autosnap_get_owned_snapshots(void *opaque);
extern void autosnap_reap_orphaned_snaps(spa_t *spa);

extern void autosnap_toggle_global_mode(spa_t *spa,
    boolean_t toggle_on);

int autosnap_lock(spa_t *spa);
void autosnap_unlock(spa_t *spa);

void autosnap_collect_orphaned_snapshots(spa_t *spa);

boolean_t autosnap_is_autosnap(dsl_dataset_t *ds);
boolean_t autosnap_check_name(const char *snap_name);

extern void autosnap_destroyer_thread_start(spa_t *spa);
extern void autosnap_destroyer_thread_stop(spa_t *spa);
extern void autosnap_init(spa_t *spa);
extern void autosnap_fini(spa_t *spa);

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_AUTOSNAP_H */
