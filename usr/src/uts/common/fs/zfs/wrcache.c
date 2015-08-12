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

/*
 * Writecache basics.
 * ZFS allows to store up to 3 dva per block pointer. Normally, all of the dvas
 * are valid at all time (or at least supposed to be so, and if data under a
 * dva is broken it is repaired with data under another dva). WRC alters the
 * behaviour. Each cached with wrc block has two dvas, and validity of them
 * changes during time. At first, when zfs decides to chace a block with wrc,
 * two dvas are allocated: one on a special device and one on a normal one.
 * Data is written to the special dva only. At the time the special dva is
 * valid and the normal one contains garbage. Later, after move operation is
 * performed for a block, i.e. the data stored under the special dva is copied
 * to the place pointed by the normal dva, the special dva is freed and can be
 * reused and the normal dva now valid and contains actual data.
 * To let zfs know which dva is valid and which is not, all data is moved by
 * chunks bounded with birth txg. When a new chunck of data should be moved, a
 * snapshot (recursive, starting at the very root dataset) is created. the
 * snapshot is used to perform simple traverse over it and not to miss any
 * block. The txg boundaries are from old_move_snap_txg + 1 to new_move_snap.
 * Checking blocks' birth txg against those boundaries, zfs understand which
 * dva is valid at the moment.
 */

#include <sys/fm/fs/zfs.h>
#include <sys/special.h>
#include <sys/spa_impl.h>
#include <sys/zio.h>
#include <sys/zio_checksum.h>
#include <sys/dmu.h>
#include <sys/dmu_tx.h>
#include <sys/zap.h>
#include <sys/zil.h>
#include <sys/ddt.h>
#include <sys/dmu_traverse.h>
#include <sys/dmu_objset.h>
#include <sys/dsl_pool.h>
#include <sys/dsl_dataset.h>
#include <sys/dsl_dir.h>
#include <sys/dsl_scan.h>
#include <sys/dsl_prop.h>
#include <sys/arc.h>
#include <sys/vdev_impl.h>
#include <sys/mutex.h>
#include <sys/time.h>
#include <sys/arc.h>
#include <sys/zio_compress.h>
#ifdef _KERNEL
#include <sys/ddi.h>
#endif

extern int zfs_txg_timeout;
extern int zfs_scan_min_time_ms;
extern uint64_t zfs_dirty_data_sync;
extern uint64_t krrp_debug;

typedef enum {
	WRC_READ_FROM_SPECIAL = 1,
	WRC_WRITE_TO_NORMAL,
} wrc_io_type_t;

/*
 * timeout (in seconds) that is used to schedule a job that moves
 * blocks from a special device to other deivices in a pool
 */
int zfs_wrc_schedtmo = 0;

uint64_t zfs_wrc_data_max = 48 << 20; /* Max data to migrate in a pass */

uint64_t wrc_mv_cancel_threshold_initial = 20;
/* we are not sure if we need logic of threshold increment */
uint64_t wrc_mv_cancel_threshold_step = 0;
uint64_t wrc_mv_cancel_threshold_cap = 50;

static boolean_t wrc_activate_impl(spa_t *spa);
static wrc_block_t *wrc_create_block(wrc_data_t *wrc_data,
    const blkptr_t *bp);
static void wrc_move_block(void *arg);
static int wrc_move_block_impl(wrc_block_t *block);
static int wrc_collect_special_blocks(dsl_pool_t *dp);
static void wrc_close_window(spa_t *spa);
static void wrc_write_update_window(void *void_spa, dmu_tx_t *tx);
static boolean_t dsl_pool_wrcio_limit(dsl_pool_t *dp, uint64_t txg);

static int wrc_io(wrc_io_type_t type, wrc_block_t *block, void *data);
static int wrc_blocks_compare(const void *arg1, const void *arg2);

void
wrc_init(wrc_data_t *wrc_data, spa_t *spa)
{
	(void) memset(wrc_data, 0, sizeof (wrc_data_t));

	wrc_data->wrc_spa = spa;

	mutex_init(&wrc_data->wrc_lock, NULL, MUTEX_DEFAULT, NULL);
	cv_init(&wrc_data->wrc_cv, NULL, CV_DEFAULT, NULL);

	avl_create(&wrc_data->wrc_blocks, wrc_blocks_compare,
	    sizeof (wrc_block_t), offsetof(wrc_block_t, node));
	avl_create(&wrc_data->wrc_moved_blocks, wrc_blocks_compare,
	    sizeof (wrc_block_t), offsetof(wrc_block_t, node));
}

void
wrc_fini(wrc_data_t *wrc_data)
{
	wrc_clean_plan_tree(wrc_data->wrc_spa);
	wrc_clean_moved_tree(wrc_data->wrc_spa);

	avl_destroy(&wrc_data->wrc_blocks);
	avl_destroy(&wrc_data->wrc_moved_blocks);

	cv_destroy(&wrc_data->wrc_cv);
	mutex_destroy(&wrc_data->wrc_lock);

	wrc_data->wrc_spa = NULL;
}

#ifndef _KERNEL
/*ARGSUSED*/
static clock_t
drv_usectohz(uint64_t time)
{
	return (1000);
}
#endif

static wrc_block_t *
wrc_create_block(wrc_data_t *wrc_data, const blkptr_t *bp)
{
	wrc_block_t *block;

	block = kmem_alloc(sizeof (*block), KM_NOSLEEP);
	if (block == NULL)
		return (NULL);

	/*
	 * Fill information describing data we need to move
	 */
#ifdef _KERNEL
	DTRACE_PROBE5(wrc_plan_block_data,
	    uint64_t, DVA_GET_VDEV(&bp->blk_dva[0]),
	    uint64_t, DVA_GET_OFFSET(&bp->blk_dva[0]),
	    uint64_t, DVA_GET_VDEV(&bp->blk_dva[1]),
	    uint64_t, DVA_GET_OFFSET(&bp->blk_dva[1]),
	    uint64_t, BP_GET_PSIZE(bp));
#endif

	mutex_init(&block->lock, NULL, MUTEX_DEFAULT, NULL);
	block->data = wrc_data;
	block->blk_prop = 0;

	block->dva[0] = bp->blk_dva[0];
	block->dva[1] = bp->blk_dva[1];
	block->btxg = BP_PHYSICAL_BIRTH(bp);

	WRCBP_SET_COMPRESS(block, BP_GET_COMPRESS(bp));
	WRCBP_SET_PSIZE(block, BP_GET_PSIZE(bp));
	WRCBP_SET_LSIZE(block, BP_GET_LSIZE(bp));

	return (block);
}

void
wrc_free_block(wrc_block_t *block)
{
	mutex_destroy(&block->lock);
	kmem_free(block, sizeof (*block));
}

void
wrc_clean_plan_tree(spa_t *spa)
{
	void *cookie = NULL;
	wrc_block_t *node = NULL;
	avl_tree_t *tree = &spa->spa_wrc.wrc_blocks;

	while ((node = avl_destroy_nodes(tree, &cookie)) != NULL)
		wrc_free_block(node);

	spa->spa_wrc.wrc_block_count = 0;
}

void
wrc_clean_moved_tree(spa_t *spa)
{
	void *cookie = NULL;
	wrc_block_t *node = NULL;
	avl_tree_t *tree = &spa->spa_wrc.wrc_moved_blocks;

	while ((node = avl_destroy_nodes(tree, &cookie)) != NULL)
		wrc_free_block(node);
}

/* WRC-MOVE routines */

/* Disable wrc threads but other params are left */
void
wrc_enter_fault_state(spa_t *spa)
{
	wrc_data_t *wrc_data = &spa->spa_wrc;

	mutex_enter(&wrc_data->wrc_lock);

	if (!wrc_data->wrc_isfault) {
		wrc_data->wrc_thr_exit = B_TRUE;
		wrc_data->wrc_isfault = B_TRUE;
		wrc_data->wrc_walking = B_FALSE;
		cv_broadcast(&wrc_data->wrc_cv);
	}

	mutex_exit(&wrc_data->wrc_lock);
}

uint64_t wrc_hdd_load_limit = 90;
uint64_t wrc_load_delay_time = 500000;

static boolean_t
spa_wrc_stop_move(spa_t *spa)
{
	boolean_t stop =
	    ((spa->spa_special_stat.ht_normal_ut > wrc_hdd_load_limit &&
	    spa->spa_watermark == SPA_WM_NONE) || spa->spa_wrc.wrc_locked);

	return (stop);
}

/*
 * Thread to manage the data movement from
 * special devices to normal devices.
 * This thread runs as long as the spa is active.
 */
static void
spa_wrc_thread(spa_t *spa)
{
	wrc_data_t *wrc_data = &spa->spa_wrc;
	wrc_block_t	*block = 0;
	uint64_t	actv_txg = 0;
	char		name[MAXNAMELEN];

	DTRACE_PROBE1(wrc_thread_start, char *, spa->spa_name);

	(void) strcpy(name, "wrc_zio_buf_");
	(void) strncat(name, spa->spa_name, MAXNAMELEN - strlen(name) - 1);
	name[MAXNAMELEN - 1] = '\0';

	for (;;) {
		uint64_t count = 0;
		uint64_t written_sz = 0;

		mutex_enter(&wrc_data->wrc_lock);

		/*
		 * Wait walker thread collecting some blocks which
		 * must be moved
		 */
		do {
			if (spa->spa_state == POOL_STATE_UNINITIALIZED ||
			    wrc_data->wrc_thr_exit) {
				mutex_exit(&wrc_data->wrc_lock);
				goto out;
			}

			if (wrc_data->wrc_block_count == 0 ||
			    spa_wrc_stop_move(spa)) {
				(void) cv_timedwait(&wrc_data->wrc_cv,
				    &wrc_data->wrc_lock,
				    ddi_get_lbolt() +
				    drv_usectohz(wrc_load_delay_time));
			}

			count = wrc_data->wrc_block_count;
		} while (count == 0);
		actv_txg = wrc_data->wrc_finish_txg;
		mutex_exit(&wrc_data->wrc_lock);

		DTRACE_PROBE2(wrc_nblocks, char *, spa->spa_name,
		    uint64_t, count);

		wrc_data->wrc_stop = B_FALSE;
		while (count > 0) {
			mutex_enter(&wrc_data->wrc_lock);

			if (wrc_data->wrc_thr_exit) {
				mutex_exit(&wrc_data->wrc_lock);
				break;
			}

			if (actv_txg != wrc_data->wrc_finish_txg) {
				mutex_exit(&wrc_data->wrc_lock);
				break;
			}

			/*
			 * Move the block to the of moved blocks
			 * and place into the queue of blocks to be
			 * physically moved
			 */
			block = avl_first(&wrc_data->wrc_blocks);
			if (block) {
				wrc_data->wrc_block_count--;
				ASSERT(wrc_data->wrc_block_count >= 0);
				avl_remove(&wrc_data->wrc_blocks, block);
				if (block->data && block->data->wrc_isvalid) {
					taskqid_t res;
					avl_add(
					    &wrc_data->wrc_moved_blocks, block);
					mutex_exit(&wrc_data->wrc_lock);
					res = taskq_dispatch(
					    wrc_data->wrc_move_taskq,
					    wrc_move_block, block, TQ_SLEEP);
					wrc_data->wrc_blocks_out++;
					if (res == 0) {
						atomic_inc_64(
						    &wrc_data->wrc_blocks_mv);
						wrc_enter_fault_state(spa);
						break;
					}
					mutex_enter(&wrc_data->wrc_lock);
					written_sz += WRCBP_GET_PSIZE(block);
				} else {
					wrc_free_block(block);
				}
			} else {
				mutex_exit(&wrc_data->wrc_lock);
				break;
			}

			mutex_exit(&wrc_data->wrc_lock);

			count--;
			if (written_sz >= zfs_wrc_data_max ||
			    wrc_data->wrc_stop || spa_wrc_stop_move(spa)) {
				DTRACE_PROBE1(wrc_sleep,
				    int, wrc_data->wrc_stop);
				break;
			}
		}
		DTRACE_PROBE2(wrc_nbytes, char *, spa->spa_name,
		    uint64_t, written_sz);
	}

out:
	taskq_wait(wrc_data->wrc_move_taskq);
	wrc_clean_moved_tree(spa);
	wrc_data->wrc_thread = NULL;

	DTRACE_PROBE1(wrc_thread_done, char *, spa->spa_name);

	thread_exit();
}

static uint64_t wrc_fault_limit = 10;

typedef struct {
	void *buf;
	int len;
} wrc_arc_bypass_t;

int
wrc_arc_bypass_cb(void *buf, int len, void *arg)
{
	wrc_arc_bypass_t *bypass = arg;

	bypass->len = len;

	(void) memcpy(bypass->buf, buf, len);

	return (0);
}

uint64_t wrc_arc_enabled = 1;
/*
 * Moves blocks from a special device to other devices in a pool.
 */
void
wrc_move_block(void *arg)
{
	wrc_block_t *block = arg;
	wrc_data_t *wrc_data = block->data;
	spa_t *spa = wrc_data->wrc_spa;
	dsl_pool_t *dp = spa->spa_dsl_pool;
	int err = 0;
	boolean_t stop_wrc_thr = B_FALSE;
	boolean_t first_iter = B_TRUE;

	do {
		if (!first_iter)
			delay(drv_usectohz(wrc_load_delay_time));

		first_iter = B_FALSE;

		if (wrc_data->wrc_isfault || !wrc_data->wrc_isvalid)
			return;
		/*
		 * If the queue is being purged, skip blocks.
		 */
		if (wrc_data->wrc_purge) {
			atomic_inc_64(&wrc_data->wrc_blocks_mv);
			return;
		}
	} while (spa_wrc_stop_move(spa));

	/*
	 * If txg is huge and write cache migration i/o interferes with
	 * Normal user traffic, then we should no longer dirty blocks.
	 */
	stop_wrc_thr = dsl_pool_wrcio_limit(dp, dp->dp_tx.tx_open_txg);

	err = wrc_move_block_impl(block);
	if (!err) {
		atomic_inc_64(&wrc_data->wrc_blocks_mv);

		if (stop_wrc_thr == B_TRUE)
			wrc_data->wrc_stop = B_TRUE;
	} else {
		/* io error occured */
		if (++wrc_data->wrc_fault_moves >= wrc_fault_limit) {
			/* error limit exceeded - disable wrc */
			cmn_err(CE_WARN,
			    "WRC: can not move data on %s with error[%d]\n"
			    "WRC: the facility is disabled "
			    "to prevent loss of data",
			    spa->spa_name, err);

			wrc_enter_fault_state(spa);
		} else {
			cmn_err(CE_WARN,
			    "WRC: can not move data on %s with error[%d]\n"
			    "WRC: retry block (fault limit: %llu/%llu)",
			    spa->spa_name, err,
			    (unsigned long long) wrc_data->wrc_fault_moves,
			    (unsigned long long) wrc_fault_limit);

			/*
			 * re-plan the block with the highest priority and
			 * try to move it again
			 */
			if (!taskq_dispatch(wrc_data->wrc_move_taskq,
			    wrc_move_block, block, TQ_SLEEP | TQ_FRONT)) {
				atomic_inc_64(&wrc_data->wrc_blocks_mv);
				wrc_enter_fault_state(spa);
			}
		}
	}
}

static int
wrc_move_block_impl(wrc_block_t *block)
{
	void *buf;
	int err = 0;
	wrc_data_t *wrc_data = block->data;
	spa_t *spa = wrc_data->wrc_spa;

	if (WRCBP_IS_DELETED(block))
		return (0);

	spa_config_enter(spa, SCL_VDEV | SCL_STATE_ALL, FTAG, RW_READER);

	buf = zio_data_buf_alloc(WRCBP_GET_PSIZE(block));

	if (wrc_arc_enabled) {
		blkptr_t pseudo_bp = { 0 };
		wrc_arc_bypass_t bypass = { 0 };
		void *dbuf = NULL;

		if (WRCBP_GET_COMPRESS(block) != ZIO_COMPRESS_OFF) {
			dbuf = zio_data_buf_alloc(WRCBP_GET_LSIZE(block));
			bypass.buf = dbuf;
		} else {
			bypass.buf = buf;
		}

		pseudo_bp.blk_dva[0] = block->dva[0];
		pseudo_bp.blk_dva[1] = block->dva[1];
		BP_SET_BIRTH(&pseudo_bp, block->btxg, block->btxg);

		mutex_enter(&block->lock);
		if (WRCBP_IS_DELETED(block)) {
			if (WRCBP_GET_COMPRESS(block) != ZIO_COMPRESS_OFF)
				zio_data_buf_free(dbuf, WRCBP_GET_LSIZE(block));

			goto out;
		}

		err = arc_io_bypass(spa, &pseudo_bp,
		    wrc_arc_bypass_cb, &bypass);

		if (!err && WRCBP_GET_COMPRESS(block) != ZIO_COMPRESS_OFF) {
			size_t size = zio_compress_data(
			    (enum zio_compress)WRCBP_GET_COMPRESS(block),
			    dbuf, buf, bypass.len);
			size_t rounded =
			    P2ROUNDUP(size, (size_t)SPA_MINBLOCKSIZE);
			if (rounded != WRCBP_GET_PSIZE(block)) {
				/* random error to get to slow path */
				err = ERANGE;
				cmn_err(CE_WARN, "WRC WARN: ARC COMPRESSION "
				    "FAILED: %u %u %u",
				    (unsigned)size,
				    (unsigned)WRCBP_GET_PSIZE(block),
				    (unsigned)WRCBP_GET_COMPRESS(block));
			} else if (rounded > size) {
				bzero((char *)buf + size, rounded - size);
			}
		}

		if (WRCBP_GET_COMPRESS(block) != ZIO_COMPRESS_OFF)
			zio_data_buf_free(dbuf, WRCBP_GET_LSIZE(block));

	} else {
		err = ENOTSUP;
		mutex_enter(&block->lock);
		if (WRCBP_IS_DELETED(block))
			goto out;
	}

	/*
	 * Any error means that arc read failed and block is being moved via
	 * slow path
	 */
	if (err) {
		err = wrc_io(WRC_READ_FROM_SPECIAL, block, buf);
		if (err) {
			cmn_err(CE_WARN, "WRC: move task has failed to read:"
			    " error [%d]", err);
			goto out;
		}
		DTRACE_PROBE(wrc_move_from_disk);
	} else {
		DTRACE_PROBE(wrc_move_from_arc);
	}

	err = wrc_io(WRC_WRITE_TO_NORMAL, block, buf);
	if (err) {
		cmn_err(CE_WARN, "WRC: move task has failed to write: "
		    "error [%d]", err);
		goto out;
	}

#ifdef _KERNEL
	DTRACE_PROBE5(wrc_move_block_data,
	    uint64_t, DVA_GET_VDEV(&block->dva[0]),
	    uint64_t, DVA_GET_OFFSET(&block->dva[0]),
	    uint64_t, DVA_GET_VDEV(&block->dva[1]),
	    uint64_t, DVA_GET_OFFSET(&block->dva[1]),
	    uint64_t, WRCBP_GET_PSIZE(block));
#endif

out:
	mutex_exit(&block->lock);
	zio_data_buf_free(buf, WRCBP_GET_PSIZE(block));

	spa_config_exit(spa, SCL_VDEV | SCL_STATE_ALL, FTAG);

	return (err);
}

/* WRC-WALK routines */

int
wrc_walk_lock(spa_t *spa)
{
	wrc_data_t *wrc_data = &spa->spa_wrc;

	mutex_enter(&wrc_data->wrc_lock);
	while (wrc_data->wrc_locked)
		(void) cv_wait(&wrc_data->wrc_cv, &wrc_data->wrc_lock);
	if (wrc_data->wrc_thr_exit) {
		mutex_exit(&wrc_data->wrc_lock);
		return (ENOLCK);
	}

	wrc_data->wrc_locked = B_TRUE;
	while (wrc_data->wrc_walking)
		(void) cv_wait(&wrc_data->wrc_cv, &wrc_data->wrc_lock);
	if (wrc_data->wrc_thr_exit) {
		mutex_exit(&wrc_data->wrc_lock);
		return (ENOLCK);
	}

	cv_broadcast(&wrc_data->wrc_cv);
	mutex_exit(&wrc_data->wrc_lock);

	return (0);
}

void
wrc_walk_unlock(spa_t *spa)
{
	wrc_data_t *wrc_data = &spa->spa_wrc;
	mutex_enter(&wrc_data->wrc_lock);
	wrc_data->wrc_locked = B_FALSE;
	cv_broadcast(&wrc_data->wrc_cv);
	mutex_exit(&wrc_data->wrc_lock);
}

/* thread to collect blocks that must be moved */
static void
spa_wrc_walk_thread(spa_t *spa)
{
	wrc_data_t *wrc_data = &spa->spa_wrc;
	int		err = 0;

	DTRACE_PROBE1(wrc_walk_thread_start, char *, spa->spa_name);

	for (;;) {
		mutex_enter(&wrc_data->wrc_lock);

		wrc_data->wrc_walking = B_FALSE;

		cv_broadcast(&wrc_data->wrc_cv);

		/* Set small wait time to delay walker restart */
		/* XXX: add logic to wait until load is not very high */
		do {
			(void) cv_timedwait(&wrc_data->wrc_cv,
			    &wrc_data->wrc_lock,
			    ddi_get_lbolt() + hz / 4);
		} while ((spa->spa_state == POOL_STATE_UNINITIALIZED ||
		    spa_wrc_stop_move(spa)) && !wrc_data->wrc_thr_exit);

		if (wrc_data->wrc_thr_exit || !spa->spa_dsl_pool) {
			mutex_exit(&wrc_data->wrc_lock);
			goto out;
		}

		wrc_data->wrc_walking = B_TRUE;

		cv_broadcast(&wrc_data->wrc_cv);

		mutex_exit(&wrc_data->wrc_lock);

		err = wrc_collect_special_blocks(spa->spa_dsl_pool);
		if (err && err != ERESTART && err != EAGAIN) {
			cmn_err(CE_WARN, "WRC: can not "
			    "traverse pool: error [%d]\n"
			    "WRC: collector thread will be disabled", err);
			break;
		}
	}
out:
	if (err)
		wrc_enter_fault_state(spa);
	taskq_wait(wrc_data->wrc_move_taskq);
	wrc_clean_plan_tree(spa);
	wrc_data->wrc_walk_thread = NULL;

	DTRACE_PROBE1(wrc_walk_thread_done, char *, spa->spa_name);

	thread_exit();
}

int wrc_force_trigger = 1;
/*
 * This function triggers the write cache thread if the past
 * two sync context dif not sync more than 1/8th of
 * zfs_dirty_data_sync.
 * This function is called only if the current sync context
 * did not sync more than 1/16th of zfs_dirty_data_sync.
 */
void
wrc_trigger_wrcthread(spa_t *spa, uint64_t prev_sync_avg)
{
	wrc_data_t *wrc_data = &spa->spa_wrc;

	/*
	 * Using mutex_tryenter() because if the worker is
	 * holding the mutex, it is already up, no need
	 * to cv_signal()
	 */
	if ((wrc_force_trigger || prev_sync_avg < zfs_dirty_data_sync / 8) &&
	    mutex_tryenter(&wrc_data->wrc_lock)) {
		if (wrc_data->wrc_block_count) {
			DTRACE_PROBE1(wrc_trigger_worker, char *,
			    spa->spa_name);
			cv_signal(&wrc_data->wrc_cv);
		}
		mutex_exit(&wrc_data->wrc_lock);
	}
}

static boolean_t
wrc_should_pause_scanblocks(dsl_pool_t *dp,
    wrc_parseblock_cb_t *cbd, const zbookmark_phys_t *zb)
{
	hrtime_t elapsed_ns;

	/*
	 * We know how to resume iteration on level 0
	 * blocks only
	 */
	if (zb->zb_level != 0)
		return (B_FALSE);

	/* We're resuming */
	if (!ZB_IS_ZERO(&cbd->zb))
		return (B_FALSE);

	/*
	 * We should stop if either traversal time
	 * took more than zfs_txg_timeout or it took
	 * more zfs_scan_min_time while somebody is waiting
	 * for our transaction group.
	 */
	elapsed_ns = gethrtime() - cbd->start_time;
	if (elapsed_ns / NANOSEC > zfs_txg_timeout ||
	    (elapsed_ns / MICROSEC > zfs_scan_min_time_ms &&
	    txg_sync_waiting(dp)) || spa_shutting_down(dp->dp_spa))
		return (B_TRUE);

	return (B_FALSE);
}

/*
 * Callback passed in traversal function. Checks whether block is
 * special and hence should be planned for move
 */
/* ARGSUSED */
int
wrc_traverse_ds_cb(spa_t *spa, zilog_t *zilog, const blkptr_t *bp,
    const zbookmark_phys_t *zb, const dnode_phys_t *dnp, void *arg)
{
	wrc_data_t *wrc_data = &spa->spa_wrc;
	wrc_parseblock_cb_t *cbd = arg;
	wrc_block_t *block;
	int ndvas;
	avl_index_t where;
	vdev_t *vd1, *vd2;

	/* skip ZIL blocks */
	if (bp == NULL || zb->zb_level == ZB_ZIL_LEVEL)
		return (0);

	if (BP_IS_EMBEDDED(bp) || BP_IS_HOLE(bp))
		return (0);

	/* skip metadata */
	if (BP_IS_METADATA(bp))
		return (0);

	/*  Skip blocks which are not placed on both classes */
	ndvas = BP_GET_NDVAS(bp);
	if (ndvas == 1)
		return (0);

	spa_config_enter(spa, SCL_VDEV, FTAG, RW_READER);
	vd1 = vdev_lookup_top(spa, DVA_GET_VDEV(&bp->blk_dva[0]));
	vd2 = vdev_lookup_top(spa, DVA_GET_VDEV(&bp->blk_dva[1]));
	if (!vdev_is_special(vd1) || vdev_is_special(vd2)) {
		spa_config_exit(spa, SCL_VDEV, FTAG);
		return (0);
	}
	spa_config_exit(spa, SCL_VDEV, FTAG);

	mutex_enter(&wrc_data->wrc_lock);

	if (spa_wrc_stop_move(spa)) {
		mutex_exit(&wrc_data->wrc_lock);
		return (ERESTART);
	}

	if (cbd->actv_txg != wrc_data->wrc_finish_txg) {
		mutex_exit(&wrc_data->wrc_lock);
		return (ERESTART);
	}

	if (wrc_should_pause_scanblocks(spa->spa_dsl_pool, cbd, zb)) {
		mutex_exit(&wrc_data->wrc_lock);
		return (ERESTART);
	}

	block = wrc_create_block(wrc_data, bp);
	if (block == NULL) {
		mutex_exit(&wrc_data->wrc_lock);
		return (ERESTART);
	}

	/*
	 * Add block to the tree of coolected blocks or drop it
	 * if it already there (it is possible with deduplication,
	 * for example)
	 */
	if (avl_find(&wrc_data->wrc_blocks, block, &where) == NULL) {
		avl_insert(&wrc_data->wrc_blocks, block, where);
		cbd->bt_size += WRCBP_GET_PSIZE(block);
		wrc_data->wrc_block_count++;
		wrc_data->wrc_blocks_in++;
	} else {
		wrc_free_block(block);
	}
	mutex_exit(&wrc_data->wrc_lock);

	return (0);
}

/*
 * Iterate through data blocks on a "special" device and collect those
 * ones that can be moved to other devices in a pool.
 *
 * XXX: For now we collect as many blocks as possible in order to dispatch
 * them to the taskq later. It may be reasonable to invent a mechanism
 * which will allow not to store the whole `moving` tree in-core
 * (persistent move bookmark, for example)
 */
int
wrc_collect_special_blocks(dsl_pool_t *dp)
{
	spa_t *spa = dp->dp_spa;
	wrc_data_t *wrc_data = &spa->spa_wrc;
	wrc_parseblock_cb_t cb_data;
	int err = 0;
	hrtime_t scan_start;
	uint64_t diff;

	if (!zfs_wrc_schedtmo)
		zfs_wrc_schedtmo = zfs_txg_timeout * 2;

	scan_start = gethrtime();
	diff = scan_start - dp->dp_spec_rtime;
	if (diff / NANOSEC < zfs_wrc_schedtmo)
		return (EAGAIN);

	cb_data.wrc_data = wrc_data;
	cb_data.zb = spa->spa_lszb;
	cb_data.start_time = scan_start;
	cb_data.actv_txg = wrc_data->wrc_finish_txg;
	cb_data.bt_size = 0ULL;

	/*
	 * Traverse the range of txg to collect blocks
	 */
	if (wrc_data->wrc_walk && wrc_data->wrc_finish_txg) {
		if (krrp_debug) {
			cmn_err(CE_NOTE, "WRC: new window (%llu; %llu)",
			    (unsigned long long)wrc_data->wrc_start_txg,
			    (unsigned long long)wrc_data->wrc_finish_txg);
		}
		err = traverse_pool(spa, wrc_data->wrc_start_txg - 1,
		    wrc_data->wrc_finish_txg + 1,
		    TRAVERSE_PREFETCH_METADATA | TRAVERSE_POST,
		    wrc_traverse_ds_cb, &cb_data, &cb_data.zb);
	}

	spa->spa_lszb = cb_data.zb;
	if (err != ERESTART && err != EAGAIN && (cb_data.bt_size == 0ULL) ||
	    ZB_IS_ZERO(&cb_data.zb)) {
		/*
		 * No more blocks to move or error state
		 */
		mutex_enter(&wrc_data->wrc_lock);
		wrc_data->wrc_walk = B_FALSE;
		if (err) {
			/*
			 * Something went wrong during the traversing
			 */
			if (wrc_data->wrc_thr_exit) {
				mutex_exit(&wrc_data->wrc_lock);
				return (EINTR);
			}

			cmn_err(CE_WARN,
			    "WRC: Can not collect data "
			    "because of error [%d]", err);

			wrc_purge_window(spa, NULL);
			mutex_exit(&wrc_data->wrc_lock);

			err = 0;
		} else if (wrc_data->wrc_blocks_in == wrc_data->wrc_blocks_mv) {
			/* Everything is moved, close the window */
			if (wrc_data->wrc_finish_txg)
				wrc_close_window(spa);

			/* Say to others that walking stopped */
			wrc_data->wrc_walking = B_FALSE;
			cv_broadcast(&wrc_data->wrc_cv);

			/* and wait until a new window appears */
			for (;;) {
				uint64_t percentage;

				(void) cv_timedwait(&wrc_data->wrc_cv,
				    &wrc_data->wrc_lock,
				    ddi_get_lbolt() + 1000);

				if (wrc_data->wrc_thr_exit) {
					mutex_exit(&wrc_data->wrc_lock);
					return (EINTR);
				}

				/*
				 * WRC-windowd has been opened automatically
				 */
				if (wrc_data->wrc_walk)
					break;

				/*
				 * Wait-timeout has exceeded, there is no
				 * Write-I/O.
				 */
				percentage = spa_class_alloc_percentage(
				    spa_special_class(spa));
				if (wrc_data->wrc_blocks_mv_last != 0 ||
				    (percentage > 5 &&
				    wrc_data->wrc_first_move)) {
					/*
					 * To be sure that special
					 * does not contain non-moved
					 * data we forcefully open WRC-window
					 */
					mutex_exit(&wrc_data->wrc_lock);
					autosnap_force_snap(
					    wrc_data->wrc_autosnap_hdl,
					    B_FALSE);
					mutex_enter(&wrc_data->wrc_lock);
				}
			}

			mutex_exit(&wrc_data->wrc_lock);

			dsl_sync_task(spa->spa_name, NULL,
			    wrc_write_update_window, spa,
			    ZFS_SPACE_CHECK_NONE, 0);
		} else {
			mutex_exit(&wrc_data->wrc_lock);
		}


	} else if (err == ERESTART) {
		/*
		 * We were interrupted, the iteration will be
		 * resumed later.
		 */
		DTRACE_PROBE2(traverse__intr, spa_t *, spa,
		    wrc_parseblock_cb_t *, &cb_data);
	}

	dp->dp_spec_rtime = gethrtime();

	return (err);
}

/*
 * This function checks if write cache migration i/o is
 * affecting the normal user i/o traffic. We determine this
 * by checking if total data in current txg > zfs_wrc_data_max
 * and migration i/o is more than zfs_wrc_io_perc_max % of total
 * data in this txg. If total data in this txg < zfs_dirty_data_sync/4,
 * we assume not much of user traffic is happening..
 */
static boolean_t
dsl_pool_wrcio_limit(dsl_pool_t *dp, uint64_t txg)
{
	boolean_t ret = B_FALSE;
	if (mutex_tryenter(&dp->dp_lock)) {
		if (dp->dp_dirty_pertxg[txg & TXG_MASK] !=
		    dp->dp_wrcio_towrite[txg & TXG_MASK] &&
		    dp->dp_dirty_pertxg[txg & TXG_MASK] >
		    zfs_wrc_data_max &&
		    dp->dp_wrcio_towrite[txg & TXG_MASK] > ((WRCIO_PERC_MIN *
		    dp->dp_dirty_pertxg[txg & TXG_MASK]) / 100) &&
		    dp->dp_wrcio_towrite[txg & TXG_MASK] <
		    ((WRCIO_PERC_MAX * dp->dp_dirty_pertxg[txg & TXG_MASK]) /
		    100))
			ret = B_TRUE;
		mutex_exit(&dp->dp_lock);
	}
	return (ret);

}

/* WRC-THREAD_CONTROL */

/* Starts wrc threads and set associated structures */
void
wrc_start_thread(spa_t *spa)
{
	wrc_data_t *wrc_data = &spa->spa_wrc;

	ASSERT(strcmp(spa->spa_name, TRYIMPORT_NAME) != 0);
	ASSERT(wrc_data->wrc_isvalid);

	mutex_enter(&wrc_data->wrc_lock);

	if (wrc_data->wrc_thread == NULL && wrc_data->wrc_walk_thread == NULL) {
		wrc_data->wrc_thr_exit = B_FALSE;
#ifdef _KERNEL
		wrc_data->wrc_thread = thread_create(NULL, 0,
		    spa_wrc_thread, spa, 0, &p0, TS_RUN, maxclsyspri);
		wrc_data->wrc_walk_thread = thread_create(NULL, 0,
		    spa_wrc_walk_thread, spa, 0, &p0, TS_RUN, maxclsyspri);
		spa_start_perfmon_thread(spa);
#endif
	}
	mutex_exit(&wrc_data->wrc_lock);
}

/* Disables wrc thread and reset associated data structures */
boolean_t
wrc_stop_thread(spa_t *spa)
{
	wrc_data_t *wrc_data = &spa->spa_wrc;
	boolean_t stop = B_FALSE;

	stop |= spa_stop_perfmon_thread(spa);
	mutex_enter(&wrc_data->wrc_lock);
	if (wrc_data->wrc_thread != NULL || wrc_data->wrc_walk_thread != NULL) {
		wrc_data->wrc_thr_exit = B_TRUE;
		cv_broadcast(&wrc_data->wrc_cv);
		mutex_exit(&wrc_data->wrc_lock);
#ifdef _KERNEL
		if (wrc_data->wrc_thread)
			thread_join(wrc_data->wrc_thread->t_did);
		if (wrc_data->wrc_walk_thread)
			thread_join(wrc_data->wrc_walk_thread->t_did);
#endif
		mutex_enter(&wrc_data->wrc_lock);
		wrc_data->wrc_thread = NULL;
		wrc_data->wrc_walk_thread = NULL;
		stop |= B_TRUE;
	}

	mutex_exit(&wrc_data->wrc_lock);

	return (stop);
}

/* WRC-WND routines */

#define	DMU_POOL_WRC_START_TXG "wrc_start_txg"
#define	DMU_POOL_WRC_FINISH_TXG "wrc_finish_txg"
#define	DMU_POOL_WRC_TO_RELE_TXG "wrc_to_rele_txg"
#define	DMU_POOL_WRC_STATE_DELETE "wrc_state_delete"

/* On-disk wrc parameters alternation */

static void
wrc_set_state_delete(void *void_spa, dmu_tx_t *tx)
{
	uint64_t upd = 1;
	spa_t *spa = void_spa;

	(void) zap_update(spa->spa_dsl_pool->dp_meta_objset,
	    DMU_POOL_DIRECTORY_OBJECT,
	    DMU_POOL_WRC_STATE_DELETE, sizeof (uint64_t), 1, &upd, tx);
}

static void
wrc_clean_state_delete(void *void_spa, dmu_tx_t *tx)
{
	uint64_t upd = 0;
	spa_t *spa = void_spa;

	(void) zap_update(spa->spa_dsl_pool->dp_meta_objset,
	    DMU_POOL_DIRECTORY_OBJECT,
	    DMU_POOL_WRC_STATE_DELETE, sizeof (uint64_t), 1, &upd, tx);
}

static void
wrc_write_update_window(void *void_spa, dmu_tx_t *tx)
{
	spa_t *spa = void_spa;
	wrc_data_t *wrc_data = &spa->spa_wrc;

	(void) zap_update(spa->spa_dsl_pool->dp_meta_objset,
	    DMU_POOL_DIRECTORY_OBJECT,
	    DMU_POOL_WRC_START_TXG, sizeof (uint64_t), 1,
	    &wrc_data->wrc_start_txg, tx);
	(void) zap_update(spa->spa_dsl_pool->dp_meta_objset,
	    DMU_POOL_DIRECTORY_OBJECT,
	    DMU_POOL_WRC_FINISH_TXG, sizeof (uint64_t), 1,
	    &wrc_data->wrc_finish_txg, tx);
	(void) zap_update(spa->spa_dsl_pool->dp_meta_objset,
	    DMU_POOL_DIRECTORY_OBJECT,
	    DMU_POOL_WRC_TO_RELE_TXG, sizeof (uint64_t), 1,
	    &wrc_data->wrc_txg_to_rele, tx);
}

static void
wrc_close_window_impl(spa_t *spa, avl_tree_t *tree)
{
	wrc_block_t *node;
	wrc_data_t *wrc_data = &spa->spa_wrc;
	dmu_tx_t *tx;
	int err;
	uint64_t txg;
	void *cookie = NULL;

	ASSERT(MUTEX_HELD(&wrc_data->wrc_lock));

	wrc_data->wrc_delete = B_TRUE;

	mutex_exit(&wrc_data->wrc_lock);
	/*
	 * Set flag that wrc has finished moving the window and
	 * freeing special dvas now
	 */
	dsl_sync_task(spa->spa_name, NULL,
	    wrc_set_state_delete, spa, 0, ZFS_SPACE_CHECK_NONE);

	tx = dmu_tx_create_dd(spa->spa_dsl_pool->dp_mos_dir);
	err = dmu_tx_assign(tx, TXG_WAIT);

	VERIFY(err == 0);

	txg = tx->tx_txg;

	mutex_enter(&wrc_data->wrc_lock);

	/*
	 * There was a purge while delete state was being written
	 * Everything is reset so no frees are required or allowed
	 */
	if (wrc_data->wrc_delete == B_FALSE) {
		dmu_tx_commit(tx);
		return;
	}

	/*
	 * Clean the tree of moved blocks, free special dva and
	 * wrc_block structure of every block in the tree
	 */
	spa_config_enter(spa, SCL_VDEV, FTAG, RW_READER);
	while ((node = avl_destroy_nodes(tree, &cookie)) != NULL) {
		if (!WRCBP_IS_DELETED(node)) {
			metaslab_free_dva(spa, &node->dva[0],
			    tx->tx_txg, B_FALSE);
		}

		wrc_free_block(node);
	}
	spa_config_exit(spa, SCL_VDEV, FTAG);

	/* Move left boundary of the window and reset the right one */
	wrc_data->wrc_start_txg = wrc_data->wrc_finish_txg + 1;
	wrc_data->wrc_finish_txg = 0;
	wrc_data->wrc_txg_to_rele = 0;
	wrc_data->wrc_roll_threshold = wrc_mv_cancel_threshold_initial;
	wrc_data->wrc_delete = B_FALSE;

	wrc_data->wrc_blocks_mv_last = wrc_data->wrc_blocks_mv;

	wrc_data->wrc_blocks_in = 0;
	wrc_data->wrc_blocks_out = 0;
	wrc_data->wrc_blocks_mv = 0;

	/* Clean deletion-state flag and write down new boundaries */
	dsl_sync_task_nowait(spa->spa_dsl_pool,
	    wrc_clean_state_delete, spa, 0, ZFS_SPACE_CHECK_NONE, tx);
	dsl_sync_task_nowait(spa->spa_dsl_pool,
	    wrc_write_update_window, spa, 0, ZFS_SPACE_CHECK_NONE, tx);
	dmu_tx_commit(tx);

	mutex_exit(&wrc_data->wrc_lock);

	/* Wait frees and wrc parameters to be synced to disk */
	txg_wait_synced(spa->spa_dsl_pool, txg);

	mutex_enter(&wrc_data->wrc_lock);
}

/* Close the wrc window and release the snapshot of its right boundary */
static void
wrc_close_window(spa_t *spa)
{
	wrc_data_t *wrc_data = &spa->spa_wrc;
	uint64_t tmp = wrc_data->wrc_txg_to_rele;

	ASSERT(MUTEX_HELD(&wrc_data->wrc_lock));

	ASSERT0(wrc_data->wrc_block_count);
	ASSERT(avl_is_empty(&wrc_data->wrc_blocks));

	VERIFY(wrc_data->wrc_finish_txg != 0);

	if (krrp_debug) {
		cmn_err(CE_NOTE, "WRC: window (%llu; %llu) has been completed\n"
		    "WRC: %llu blocks moved",
		    (unsigned long long)wrc_data->wrc_start_txg,
		    (unsigned long long)wrc_data->wrc_finish_txg,
		    (unsigned long long)wrc_data->wrc_blocks_mv);
		VERIFY(wrc_data->wrc_blocks_mv == wrc_data->wrc_blocks_in);
		VERIFY(wrc_data->wrc_blocks_mv == wrc_data->wrc_blocks_out);
	}

	wrc_close_window_impl(spa, &wrc_data->wrc_moved_blocks);

	autosnap_release_snapshots_by_txg(
	    wrc_data->wrc_autosnap_hdl, tmp, AUTOSNAP_NO_SNAP);
}

/*
 * Purge pending blocks and reset right boundary.
 * It is used when dataset is deleted or an error
 * occured during traversing. If called in the
 * context of the sync thread, then syncing tx must
 * be passed. Outside the syncing thread NULL is
 * expected instead.
 */
void
wrc_purge_window(spa_t *spa, dmu_tx_t *tx)
{
	wrc_data_t *wrc_data = &spa->spa_wrc;
	uint64_t snap_txg;

	ASSERT(MUTEX_HELD(&wrc_data->wrc_lock));

	/*
	 * Clean tree with blocks which are not queued
	 * to be moved yet
	 */
	wrc_clean_plan_tree(spa);

	/*
	 * Set purge on to notify move workers to skip all
	 * blocks that are left in queue not to waste time
	 * moving data which will be required to move again.
	 * Wait until all queued blocks are processed.
	 */
	wrc_data->wrc_purge = B_TRUE;
	while (wrc_data->wrc_blocks_out !=
	    wrc_data->wrc_blocks_mv &&
	    !wrc_data->wrc_thr_exit) {
		(void) cv_timedwait(&wrc_data->wrc_cv,
		    &wrc_data->wrc_lock,
		    ddi_get_lbolt() + 1000);
	}
	wrc_data->wrc_purge = B_FALSE;

	/*
	 * Reset the deletion flag to make sure
	 * that the purge is appreciated by
	 * dva[0] deleter
	 */
	wrc_data->wrc_delete = B_FALSE;

	/*
	 * Clean the tree of moved blocks
	 */
	wrc_clean_moved_tree(spa);

	wrc_data->wrc_blocks_in = 0;
	wrc_data->wrc_blocks_out = 0;
	wrc_data->wrc_blocks_mv = 0;

	/* Reset bookmark */
	bzero(&spa->spa_lszb, sizeof (spa->spa_lszb));

	snap_txg = wrc_data->wrc_txg_to_rele;

	/*
	 * Reset right boundary and time of latest window
	 * start to catch the closest snapshot which will be
	 * created
	 */
	wrc_data->wrc_finish_txg = 0;
	wrc_data->wrc_txg_to_rele = 0;
	wrc_data->wrc_latest_window_time = 0;
	wrc_data->wrc_roll_threshold =
	    MIN(wrc_data->wrc_roll_threshold + wrc_mv_cancel_threshold_step,
	    wrc_mv_cancel_threshold_cap);

	if (krrp_debug)
		cmn_err(CE_NOTE, "WRC: Right boundary will be moved forward");

	if (tx) {
		/*
		 * After purge from sync context, delete state isn't valid,
		 * so reset it
		 */
		dsl_sync_task_nowait(spa->spa_dsl_pool,
		    wrc_clean_state_delete, spa, 0, ZFS_SPACE_CHECK_NONE, tx);
		dsl_sync_task_nowait(spa->spa_dsl_pool,
		    wrc_write_update_window, spa, 0, ZFS_SPACE_CHECK_NONE, tx);
	} else {
		/*
		 * It is safe to drop the lock as the function has already
		 * set everything it wanted up to the moment and only need
		 * to update on-disk format
		 */
		mutex_exit(&wrc_data->wrc_lock);

		dsl_sync_task(spa->spa_name, NULL,
		    wrc_write_update_window, spa, 0, ZFS_SPACE_CHECK_NONE);
		mutex_enter(&wrc_data->wrc_lock);
	}

	if (wrc_data->wrc_isvalid)
		autosnap_release_snapshots_by_txg(
		    wrc_data->wrc_autosnap_hdl, snap_txg,
		    AUTOSNAP_NO_SNAP);
}

/* Finalize interrupted with power cycle window */
static void
wrc_free_restore(spa_t *spa)
{
	uint64_t ret;
	wrc_data_t *wrc_data = &spa->spa_wrc;
	int err = zap_lookup(spa->spa_dsl_pool->dp_meta_objset,
	    DMU_POOL_DIRECTORY_OBJECT,
	    DMU_POOL_WRC_STATE_DELETE, sizeof (uint64_t), 1, &ret);
	boolean_t need_restore = err ? B_FALSE : (!!ret);
	wrc_parseblock_cb_t cb_data = { 0 };

	if (!need_restore) {
		wrc_data->wrc_finish_txg = 0;
		wrc_data->wrc_txg_to_rele = 0;
		return;
	}

	/*
	 * The mutex must be dropped to prevent recursive entry
	 * It is safe as we are the only user of the wrc structures
	 * at the point
	 */
	mutex_exit(&wrc_data->wrc_lock);
	cb_data.wrc_data = wrc_data;
	err = traverse_pool(spa, wrc_data->wrc_start_txg - 1,
	    wrc_data->wrc_finish_txg + 1,
	    TRAVERSE_PREFETCH_METADATA | TRAVERSE_POST,
	    wrc_traverse_ds_cb, &cb_data, &cb_data.zb);

	mutex_enter(&wrc_data->wrc_lock);

	wrc_close_window_impl(spa, &wrc_data->wrc_blocks);
	wrc_data->wrc_block_count = 0;
}

/* Autosnap confirmation callback */
/*ARGSUSED*/
static boolean_t
wrc_confirm_cv(const char *name, boolean_t recursive, uint64_t txg, void *arg)
{
	spa_t *spa = arg;
	wrc_data_t *wrc_data = &spa->spa_wrc;
	return (wrc_data->wrc_finish_txg == 0);
}

uint64_t wrc_window_roll_delay = 0;

static boolean_t
wrc_check_time(wrc_data_t *wrc_data)
{
#ifdef _KERNEL
	uint64_t time_spent =
	    ddi_get_lbolt() - wrc_data->wrc_latest_window_time;
	return (time_spent < drv_usectohz(wrc_window_roll_delay));
#else
	return (B_FALSE);
#endif
}

static boolean_t
wrc_check_space(spa_t *spa)
{
	uint64_t percentage =
	    spa_class_alloc_percentage(spa_special_class(spa));

	return (percentage < spa->spa_lowat);
}

/* Autosnap notification callback */
/*ARGSUSED*/
static boolean_t
wrc_nc_cb(const char *name, boolean_t recursive, boolean_t autosnap,
    uint64_t txg, uint64_t etxg, void *arg)
{
	spa_t *spa = arg;
	wrc_data_t *wrc_data = &spa->spa_wrc;

	if (wrc_data->wrc_finish_txg != 0 ||
	    !wrc_data->wrc_isvalid || wrc_data->wrc_isfault) {
		/* Either wrc has an active window or is faulted */
		return (B_FALSE);
	} else if (wrc_window_roll_delay &&
	    wrc_check_time(wrc_data) &&
	    wrc_check_space(spa)) {
		/* To soon to start a new window */
		return (B_FALSE);
	} else {
		/* Accept new windows */
		mutex_enter(&wrc_data->wrc_lock);
		VERIFY0(wrc_data->wrc_block_count);
		VERIFY(avl_is_empty(&wrc_data->wrc_blocks));
		wrc_data->wrc_latest_window_time = ddi_get_lbolt();
		wrc_data->wrc_first_move = B_FALSE;
		wrc_data->wrc_walk = B_TRUE;
		wrc_data->wrc_finish_txg = etxg;
		wrc_data->wrc_txg_to_rele = txg;
		wrc_data->wrc_altered_limit = 0;
		wrc_data->wrc_altered_bytes = 0;
		wrc_data->wrc_window_bytes = 0;
		cv_broadcast(&wrc_data->wrc_cv);
		mutex_exit(&wrc_data->wrc_lock);
	}
	return (B_TRUE);
}

static void
wrc_err_cb(const char *name, int err, uint64_t txg, void *arg)
{
	spa_t *spa = arg;

	cmn_err(CE_WARN, "Autosnap can not create a snapshot for writecache at "
	    "txg %llu [%d] of pool '%s'\n", (unsigned long long)txg, err, name);
	wrc_enter_fault_state(spa);
}

void
wrc_add_bytes(spa_t *spa, uint64_t txg, uint64_t bytes)
{
	wrc_data_t *wrc_data = &spa->spa_wrc;

	mutex_enter(&wrc_data->wrc_lock);

	if (wrc_data->wrc_finish_txg == txg) {
		wrc_data->wrc_window_bytes += bytes;
		wrc_data->wrc_altered_limit =
		    wrc_data->wrc_window_bytes *
		    wrc_data->wrc_roll_threshold / 100;

		DTRACE_PROBE3(wrc_window_size, uint64_t, txg,
		    uint64_t, wrc_data->wrc_window_bytes,
		    uint64_t, wrc_data->wrc_altered_limit);
	}

	mutex_exit(&wrc_data->wrc_lock);
}

/* WRC-INIT routines */

boolean_t
wrc_activate(spa_t *spa)
{
	boolean_t result = B_FALSE;

	if (spa_has_special(spa) && spa->spa_wrc_mode != WRC_MODE_OFF) {
		if (wrc_activate_impl(spa)) {
			wrc_start_thread(spa);
			result = B_TRUE;
		}
	}

	return (result);
}

#define	ZFS_PROP_WRC_PASSIVE_MODE_DS "syskrrp:wrc_passive_mode_ds"
/*
 * Initialize wrc properties for the given pool.
 */
static boolean_t
wrc_activate_impl(spa_t *spa)
{
	wrc_data_t *wrc_data = &spa->spa_wrc;
	int err = 0;
	char name[MAXPATHLEN];
	boolean_t hold = B_FALSE;

	mutex_enter(&spa->spa_wrc.wrc_lock);

	if (wrc_data->wrc_thr_exit) {
		mutex_exit(&spa->spa_wrc.wrc_lock);
		return (B_FALSE);
	}

	if (wrc_data->wrc_isvalid) {
		mutex_exit(&spa->spa_wrc.wrc_lock);
		return (B_TRUE);
	}

	(void) strcpy(name, spa->spa_name);
	(void) strcat(name, "_wrc_move");

	DTRACE_PROBE2(wrc_spa_add, char *, spa->spa_name,
	    spa_t *, spa);

	/* Reset bookmerk */
	bzero(&spa->spa_lszb, sizeof (spa->spa_lszb));

	wrc_data->wrc_roll_threshold = wrc_mv_cancel_threshold_initial;
	wrc_data->wrc_altered_limit = 0;
	wrc_data->wrc_altered_bytes = 0;
	wrc_data->wrc_window_bytes = 0;

	/* Set up autosnap handler */
#ifdef _KERNEL
	if (spa->spa_wrc_mode == WRC_MODE_ACTIVE) {
		wrc_data->wrc_autosnap_hdl =
		    autosnap_register_handler(spa->spa_name,
		    AUTOSNAP_CREATOR | AUTOSNAP_DESTROYER |
		    AUTOSNAP_GLOBAL,
		    wrc_confirm_cv, wrc_nc_cb, wrc_err_cb, spa);
	} else {
		wrc_data->wrc_autosnap_hdl =
		    autosnap_register_handler(spa->spa_name,
		    AUTOSNAP_DESTROYER | AUTOSNAP_GLOBAL,
		    wrc_confirm_cv, wrc_nc_cb, wrc_err_cb, spa);
	}

	if (wrc_data->wrc_autosnap_hdl == NULL) {
		mutex_exit(&spa->spa_wrc.wrc_lock);
		return (B_FALSE);
	}

#endif

	/*
	 * Read wrc parameters to restore
	 * latest wrc window's boundaries
	 */
	if (!rrw_held(&spa->spa_dsl_pool->dp_config_rwlock,
	    RW_WRITER)) {
		rrw_enter(&spa->spa_dsl_pool->dp_config_rwlock,
		    RW_READER, FTAG);
		hold = B_TRUE;
	}
	err = zap_lookup(spa->spa_dsl_pool->dp_meta_objset,
	    DMU_POOL_DIRECTORY_OBJECT,
	    DMU_POOL_WRC_START_TXG, sizeof (uint64_t), 1,
	    &wrc_data->wrc_start_txg);
	if (err)
		wrc_data->wrc_start_txg = 4;
	err = zap_lookup(spa->spa_dsl_pool->dp_meta_objset,
	    DMU_POOL_DIRECTORY_OBJECT,
	    DMU_POOL_WRC_FINISH_TXG, sizeof (uint64_t), 1,
	    &wrc_data->wrc_finish_txg);
	if (!err) {
		err = zap_lookup(spa->spa_dsl_pool->dp_meta_objset,
		    DMU_POOL_DIRECTORY_OBJECT,
		    DMU_POOL_WRC_TO_RELE_TXG, sizeof (uint64_t), 1,
		    &wrc_data->wrc_txg_to_rele);
	}
	if (hold)
		rrw_exit(&spa->spa_dsl_pool->dp_config_rwlock, FTAG);
	if (err) {
		wrc_data->wrc_finish_txg = 0;
		wrc_data->wrc_txg_to_rele = 0;
	}
	wrc_data->wrc_latest_window_time = ddi_get_lbolt();

	/* Prepare move queue and make the wrc active */
	wrc_data->wrc_move_taskq = taskq_create(name, 10, maxclsyspri,
	    50, INT_MAX, TASKQ_PREPOPULATE);

	wrc_data->wrc_purge = B_FALSE;
	wrc_data->wrc_walk = B_TRUE;
	wrc_data->wrc_spa = spa;
	wrc_data->wrc_isvalid = B_TRUE;

	/* Finalize window interrupted with power cycle */
	wrc_free_restore(spa);

	mutex_exit(&spa->spa_wrc.wrc_lock);

	return (B_TRUE);
}

void
wrc_switch_mode(spa_t *spa)
{
	autosnap_toggle_global_mode(spa,
	    (spa->spa_wrc_mode == WRC_MODE_ACTIVE));
}

/*
 * Caller should hold the wrc_lock.
 */
void
wrc_deactivate(spa_t *spa)
{
	wrc_data_t *wrc_data = &spa->spa_wrc;

	mutex_enter(&spa->spa_wrc.wrc_lock);

	if (!spa_has_special(spa) || !wrc_data->wrc_isvalid) {
		mutex_exit(&spa->spa_wrc.wrc_lock);
		return;
	}

	DTRACE_PROBE1(wrc_deactiv_start, char *, spa->spa_name);

#ifdef _KERNEL
	autosnap_unregister_handler(wrc_data->wrc_autosnap_hdl);
#endif
	wrc_data->wrc_isvalid = B_FALSE;

	/*
	 * There can be active queue threads performing actions
	 * taskq_destroy is a blocking function, so we need to drop
	 * the lock to prevent deadlock. It is safe as the queued
	 * tasks do not alter global state and there are no more
	 * other users but the current thread at the time
	 */
	mutex_exit(&wrc_data->wrc_lock);
	taskq_destroy(wrc_data->wrc_move_taskq);
	mutex_enter(&wrc_data->wrc_lock);

	VERIFY(avl_is_empty(&wrc_data->wrc_blocks));
	VERIFY(avl_is_empty(&wrc_data->wrc_moved_blocks));

	DTRACE_PROBE1(wrc_deactiv_done, char *, spa->spa_name);

	mutex_exit(&spa->spa_wrc.wrc_lock);
}

static int
wrc_blocks_compare(const void *arg1, const void *arg2)
{
	wrc_block_t *b1 = (wrc_block_t *)arg1;
	wrc_block_t *b2 = (wrc_block_t *)arg2;

	uint64_t d11 = b1->dva[0].dva_word[0];
	uint64_t d12 = b1->dva[0].dva_word[1];
	uint64_t d21 = b2->dva[0].dva_word[0];
	uint64_t d22 = b2->dva[0].dva_word[1];
	int cmp1 = (d11 < d21) ? (-1) : (d11 == d21 ? 0 : 1);
	int cmp2 = (d12 < d22) ? (-1) : (d12 == d22 ? 0 : 1);
	int cmp = (cmp1 == 0) ? cmp2 : cmp1;

	return (cmp);
}

static int
wrc_io(wrc_io_type_t type, wrc_block_t *block, void *data)
{
	zio_t *zio;
	zio_type_t zio_type;
	vdev_t *vd;
	uint64_t bias;
	size_t dva_num;

	if (type == WRC_READ_FROM_SPECIAL) {
		zio_type = ZIO_TYPE_READ;
		dva_num = 0;
	} else {
		ASSERT(type == WRC_WRITE_TO_NORMAL);
		zio_type = ZIO_TYPE_WRITE;
		dva_num = 1;
	}

	vd = vdev_lookup_top(block->data->wrc_spa,
	    DVA_GET_VDEV(&block->dva[dva_num]));
	bias = vd->vdev_children == 0 ? VDEV_LABEL_START_SIZE : 0;
	zio = zio_wrc(zio_type, vd, data, WRCBP_GET_PSIZE(block),
	    DVA_GET_OFFSET(&block->dva[dva_num]) + bias);

	return (zio_wait(zio));
}

/*
 * if birth_txg is less than windows, then block is on
 * normal device only otherwise it can be found on
 * special, because deletion goes under lock and until
 * deletion is done, the block is accessible on special
 */
int
wrc_select_dva(wrc_data_t *wrc_data, blkptr_t *bp)
{
	uint64_t stxg;
	uint64_t ftxg;
	int c;

	mutex_enter(&wrc_data->wrc_lock);

	stxg = wrc_data->wrc_start_txg;
	ftxg = wrc_data->wrc_finish_txg;

	if (ftxg && BP_PHYSICAL_BIRTH(bp) > ftxg) {
		DTRACE_PROBE(wrc_read_special);
		c = WRC_SPECIAL_DVA;
	} else if (BP_PHYSICAL_BIRTH(bp) >= stxg) {
		if (!ftxg && wrc_data->wrc_delete) {
			DTRACE_PROBE(wrc_read_normal);
			c = WRC_NORMAL_DVA;
		} else {
			DTRACE_PROBE(wrc_read_special);
			c = WRC_SPECIAL_DVA;
		}
	} else {
		DTRACE_PROBE(wrc_read_normal);
		c = WRC_NORMAL_DVA;
	}

	mutex_exit(&wrc_data->wrc_lock);

	return (c);
}
