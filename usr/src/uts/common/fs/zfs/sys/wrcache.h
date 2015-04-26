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
 * Copyright 2015 Nexenta Systems, Inc.  All rights reserved.
 */

#ifndef	_SYS_WRCACHE_H
#define	_SYS_WRCACHE_H

#include <sys/zfs_context.h>
#include <sys/sysmacros.h>
#include <sys/types.h>
#include <sys/fs/zfs.h>
#include <sys/spa.h>
#include <sys/dmu.h>
#include <sys/dmu_traverse.h>
#include <sys/dsl_dataset.h>
#include <sys/dsl_pool.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * write cache special class.
 */

#define	WRCIO_PERC_MIN	(25)
#define	WRCIO_PERC_MAX	(75)

/*
 * wrc_data structure contains all information associated with write cache and
 * is attached to spa structure.
 */
typedef struct wrc_data {
	kthread_t	*wrc_thread;		/* move thread */
	kthread_t	*wrc_walk_thread;	/* collector thread */

	kmutex_t	wrc_lock;

	avl_tree_t	wrc_blocks;		/* collected blocks */
	avl_tree_t	wrc_moved_blocks;	/* moved blocks */

	uint64_t	wrc_window_bytes;	/* bytes in current wnd */
	uint64_t	wrc_altered_bytes;	/* bytes altered in new wnd */
	uint64_t	wrc_roll_threshold;	/* max percent can be altered */
	uint64_t	wrc_altered_limit;	/* max bytes can be altered */

	uint64_t	wrc_block_count;	/* amount of blocks */

	taskq_t		*wrc_move_taskq;	/* pending blocks taskq */

	uint64_t	wrc_start_txg;		/* left boundary */
	uint64_t	wrc_finish_txg;		/* right boundary */
	uint64_t	wrc_txg_to_rele;	/* txg to rele */
	uint64_t	wrc_blocks_in;		/* collected */
	uint64_t	wrc_blocks_out;		/* planned */
	uint64_t	wrc_blocks_mv;		/* moved */

	uint64_t	wrc_latest_window_time;

	void		*wrc_autosnap_hdl;

	spa_t		*wrc_spa;

	kmem_cache_t	*wrc_zio_cache;		/* cache for move blocks */

	uint64_t	wrc_fault_moves;	/* amount of fault moves */

	boolean_t	wrc_purge;	/* should purge queued blocks */
	boolean_t	wrc_walk;	/* should walk */
	boolean_t	wrc_stop;	/* should pause */
	boolean_t	wrc_locked;	/* do not walk while locked */
	boolean_t	wrc_walking;	/* currently walking */

	boolean_t	wrc_delete;	/* delete state */

	boolean_t	wrc_thr_exit;	/* exit flag */
	boolean_t	wrc_isvalid;	/* wrc is inited */
	boolean_t	wrc_isfault;	/* wrc is fault */

	kcondvar_t	wrc_cv;
} wrc_data_t;

#define	WRC_SPECIAL_DVA 0
#define	WRC_NORMAL_DVA 1
#define	MAX_WRC_DVA 2

/*
 * In-core representation of a block which will be moved
 */
typedef struct wrc_block {
	/* associated wrc */
	wrc_data_t	*data;

	/* location information */
	uint64_t	size;

	/* birth txg for arc lookup */
	uint64_t	btxg;

	/* we need compression to be able move from arc */

	uint64_t	compression;

	/* dvas of blocks to move */
	dva_t		dva[2];

	avl_node_t	node;
} wrc_block_t;

typedef struct wrc_parseblock_cb {
	wrc_data_t	*wrc_data;

	/*
	 * A bookmark for resume
	 */
	zbookmark_phys_t	zb;

	/*
	 * Total size of all collected blocks
	 */
	uint64_t	bt_size;

	/*
	 * The time we started traversal process
	 */
	hrtime_t	start_time;

	uint64_t	actv_txg;
} wrc_parseblock_cb_t;

boolean_t wrc_activate(spa_t *spa);
void wrc_reactivate(spa_t *);
void wrc_deactivate(spa_t *spa);
void wrc_enter_fault_state(spa_t *spa);

wrc_data_t *spa_get_wrc_data(spa_t *spa);

void wrc_add_bytes(spa_t *spa, uint64_t txg, uint64_t bytes);

/*
 * write cache thread.
 */
boolean_t wrc_start_thread(spa_t *);
boolean_t wrc_stop_thread(spa_t *);
void wrc_trigger_wrcthread(spa_t *, uint64_t);

/*
 * callback function for traverse_dataset which validates
 * the block pointer and adds to the list.
 */
blkptr_cb_t	wrc_traverse_ds_cb;

boolean_t wrc_check_parseblocks_hold(spa_t *);
void wrc_check_parseblocks_rele(spa_t *spa);
boolean_t wrc_try_hold(spa_t *);
int wrc_walk_lock(spa_t *);
void wrc_walk_unlock(spa_t *);

void wrc_init();
void wrc_fini();
void wrc_free_block(wrc_block_t *block);
void wrc_clean_plan_tree(spa_t *spa);
void wrc_clean_moved_tree(spa_t *spa);

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_WRCACHE_H */
