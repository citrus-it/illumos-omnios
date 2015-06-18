/*
 * Copyright 2015 Nexenta Systems, Inc.  All rights reserved.
 */
#include <sys/autosnap.h>
#include <sys/dmu_objset.h>
#include <sys/dmu_send.h>
#include <sys/dmu_tx.h>
#include <sys/dsl_dir.h>
#include <sys/dsl_pool.h>
#include <sys/spa.h>
#include <zfs_fletcher.h>
#include <sys/zap.h>

#define	RECV_BUFFER_SIZE (1 << 20)

int zfs_send_timeout = 5;
uint64_t krrp_debug = 0;

static void dmu_set_send_recv_error(void *krrp_task_void, int err);
static int dmu_krrp_get_buffer(void *krrp_task_void);
static int dmu_krrp_put_buffer(void *krrp_task_void);

typedef struct {
	char name[MAXNAMELEN];
	uint64_t txg;
	uint64_t guid;
	dsl_dataset_t *ds;
	char sguid[64];
	avl_node_t snap_node;
} zfs_snap_avl_node_t;

/*
 * Stream is a sequence of snapshots considered to be related
 * init/fini initialize and deinitialize structures which are persistent for a
 * stream. Currently, only recv buffer is shared. It needs to be shared as
 * constant allocations and frees are expensive
 */
void *
dmu_krrp_stream_init()
{
	dmu_krrp_stream_t *stream =
	    kmem_alloc(sizeof (dmu_krrp_stream_t), KM_SLEEP);

	stream->custom_recv_buffer_size = RECV_BUFFER_SIZE;
	stream->custom_recv_buffer = kmem_alloc(RECV_BUFFER_SIZE, KM_SLEEP);

	return (stream);
}

void
dmu_krrp_stream_fini(void *handler)
{
	dmu_krrp_stream_t *stream = handler;

	if (stream == NULL)
		return;

	kmem_free(stream->custom_recv_buffer, stream->custom_recv_buffer_size);
	kmem_free(stream, sizeof (dmu_krrp_stream_t));
}

/*
 * Arc bypass is supposed to reduce amount of copying inside memory
 * Here os the main callback for krrp usage of arc bypass
 */
int
dmu_krrp_arc_bypass(void *buf, int len, void *arg)
{
	dmu_krrp_arc_bypass_t *bypass = arg;
	if (bypass->krrp_task->buffer_args.force_cksum)
		fletcher_4_incremental_native(buf, len, bypass->zc);
	DTRACE_PROBE(arc_bypass_send);
	return (bypass->cb(buf, len, bypass->krrp_task));
}

/*
 * KRRP-SR-INV
 * Functions used in send/recv functions to pass data to the KRRP transport
 */
int
dmu_krrp_buffer_write(void *buf, int len,
    dmu_krrp_task_t *krrp_task)
{
	int count = 0;
	int err = 0;

	while ((!err) && (count < len)) {
		if (krrp_task->buffer_state == SBS_USED) {
			kreplication_buffer_t *buffer = krrp_task->buffer;
			size_t buf_rem = buffer->buffer_size -
			    buffer->data_size;
			size_t rem = len - count;
			size_t size = MIN(rem, buf_rem);

			(void) memcpy((char *)buffer->data + buffer->data_size,
			    (char *)buf + count, size);
			count += size;
			buffer->data_size += size;

			if (buffer->data_size == buffer->buffer_size) {
				krrp_task->buffer = buffer->next;
				if (!krrp_task->buffer) {
					err = dmu_krrp_put_buffer(
					    krrp_task);
				}
			}
		} else {
			err = dmu_krrp_get_buffer(krrp_task);
		}
	}

	return (err);
}

int
dmu_krrp_buffer_read(void *buf, int len,
    dmu_krrp_task_t *krrp_task)
{
	int done = 0;
	int err = 0;

	while (!err && (done < len)) {
		if (krrp_task->buffer_state == SBS_USED) {
			kreplication_buffer_t *buffer = krrp_task->buffer;
			size_t rem = len - done;
			size_t buf_rem = buffer->data_size -
			    krrp_task->buffer_bytes_read;
			size_t size = MIN(rem, buf_rem);

			(void) memcpy((char *)buf + done,
			    (char *)buffer->data +
			    krrp_task->buffer_bytes_read, size);
			krrp_task->buffer_bytes_read += size;
			done += size;
			krrp_task->is_read = B_TRUE;

			if (krrp_task->buffer_bytes_read ==
			    buffer->data_size) {
				krrp_task->buffer = buffer->next;
				krrp_task->buffer_bytes_read = 0;
				if (!krrp_task->buffer) {
					err = dmu_krrp_put_buffer(
					    krrp_task);
				}
			}
		} else {
			err = dmu_krrp_get_buffer(krrp_task);
		}
	}

	return (err);
}

/*
 * KRRP-SEND routines
 */


#define	STRING_PROP_EL_SIZE 1
#define	UINT64_PROP_EL_SIZE 8

/*
 * Collect list of properties of datasets
 */
static int
zfs_send_collect_properties(list_t *ds_list, nvlist_t *nvp)
{
	zfs_ds_collector_entry_t *traverse_root = list_head(ds_list);
	int error = 0;

	/* Traverse the list of datasetss */
	while (traverse_root != NULL) {
		nvlist_t *nvfs, *nvprops;
		objset_t *mos;
		dsl_dataset_t *pds;
		dsl_pool_t *pdp;
		zap_cursor_t zc;
		zap_attribute_t za;
		uint64_t head_obj;

		if (!strchr(traverse_root->name, '@')) {
			traverse_root = list_next(ds_list, traverse_root);
			continue;
		}

		/* get a dataset */
		error = dsl_pool_hold(traverse_root->name, FTAG, &pdp);
		if (error)
			break;
		error = dsl_dataset_hold(pdp, traverse_root->name, FTAG, &pds);
		if (error) {
			dsl_pool_rele(pdp, FTAG);
			break;
		}
		mos = pds->ds_dir->dd_pool->dp_meta_objset;
		zap_cursor_init(&zc, mos,
		    dsl_dir_phys(pds->ds_dir)->dd_props_zapobj);

		VERIFY(0 == nvlist_alloc(&nvfs, NV_UNIQUE_NAME, KM_SLEEP));
		VERIFY(0 == nvlist_alloc(&nvprops, NV_UNIQUE_NAME, KM_SLEEP));

		/* walk over properties' zap */
		while (zap_cursor_retrieve(&zc, &za) == 0) {
			uint64_t cnt, el;

			(void) zap_length(mos,
			    dsl_dir_phys(pds->ds_dir)->dd_props_zapobj,
			    za.za_name, &el, &cnt);

			if (el == STRING_PROP_EL_SIZE) {
				char val[ZAP_MAXVALUELEN];

				error = zap_lookup(mos,
				    dsl_dir_phys(pds->ds_dir)->dd_props_zapobj,
				    za.za_name, STRING_PROP_EL_SIZE, cnt, val);
				if (error != 0) {
					cmn_err(CE_WARN,
					    "Error while looking up a prop"
					    "zap : %d", error);
				} else {
					error = nvlist_add_string(nvprops,
					    za.za_name, val);
				}
			} else if (el == UINT64_PROP_EL_SIZE) {
				error = nvlist_add_uint64(nvprops,
				    za.za_name, za.za_first_integer);
			}

			if (error)
				break;

			zap_cursor_advance(&zc);
		}

		zap_cursor_fini(&zc);

		if (error) {
			dsl_dataset_rele(pds, FTAG);
			dsl_pool_rele(pdp, FTAG);
			nvlist_free(nvfs);
			nvlist_free(nvprops);
			break;
		}

		head_obj = dsl_dir_phys(pds->ds_dir)->dd_head_dataset_obj;
		dsl_dataset_rele(pds, FTAG);
		error = dsl_dataset_hold_obj(pdp, head_obj, FTAG, &pds);
		if (error) {
			dsl_pool_rele(pdp, FTAG);
			nvlist_free(nvfs);
			nvlist_free(nvprops);
			break;
		}

		dsl_dataset_rele(pds, FTAG);
		dsl_pool_rele(pdp, FTAG);
		if (!error)
			error = nvlist_add_nvlist(nvfs, "props", nvprops);
		if (!error) {
			error = nvlist_add_nvlist(nvp,
			    traverse_root->sguid, nvfs);
		}
		nvlist_free(nvfs);
		nvlist_free(nvprops);
		if (error)
			break;
		traverse_root = list_next(ds_list, traverse_root);
	}

	return (error);
}

/* AVL compare function for snapshots */
static int
zfs_snapshot_compare(const void *arg1, const void *arg2)
{
	const zfs_snap_avl_node_t *s1 = arg1;
	const zfs_snap_avl_node_t *s2 = arg2;

	if (s1->txg > s2->txg) {
		return (+1);
	} else if (s1->txg < s2->txg) {
		return (-1);
	} else {
		return (0);
	}
}

/*
 * Collect snapshots of a given dataset in a g given range
 * Autosnapshots are only allowed to be border snapshots
 */
static int
zfs_send_collect_snaps(char *fds, char *from_snap, char *to_snap,
    boolean_t all, list_t *ds_to_send)
{
	boolean_t cc = B_FALSE;
	uint64_t offp = 0, obj = 0;
	int err = 0;
	objset_t *os;
	dsl_pool_t *pdp;
	dsl_dataset_t *pdss;
	uint64_t txg_first = 0, txg_last = UINT64_MAX;
	char from_full[MAXNAMELEN], to_full[MAXNAMELEN];
	avl_tree_t avl;
	void *cookie = NULL;
	zfs_snap_avl_node_t *node;
	zfs_ds_collector_entry_t *elt = NULL;
	zfs_ds_collector_entry_t *elf = NULL;

	err = dsl_pool_hold(fds, FTAG, &pdp);
	if (err)
		return (err);

	/*
	 * If 'all' flag isn't presented, only given snaps should be in the list
	 */

	/* check right boundary */
	if (to_snap && to_snap[0]) {
		(void) strcpy(to_full, fds);
		(void) strcat(to_full, "@");
		(void) strcat(to_full, to_snap);
		err = dsl_dataset_hold(pdp, to_full, NULL, &pdss);
		if (err) {
			dsl_pool_rele(pdp, FTAG);
			if (err == ENOENT)
				err = 0;
			return (err);
		}
		txg_last = dsl_dataset_phys(pdss)->ds_creation_txg;

		elt = dsl_dataset_collector_cache_alloc();
		(void) strcpy(elt->name, to_full);
		elt->guid = dsl_dataset_phys(pdss)->ds_guid;
		(void) sprintf(elt->sguid, "0x%llx",
		    (unsigned long long)elt->guid);
		elt->ds = pdss;
	} else {
		dsl_pool_rele(pdp, FTAG);
		return (EINVAL);
	}

	/* check left boundary */
	if (from_snap && from_snap[0]) {
		(void) strcpy(from_full, fds);
		(void) strcat(from_full, "@");
		(void) strcat(from_full, from_snap);
		err = dsl_dataset_hold(pdp, from_full, NULL, &pdss);
		if (err) {
			if (err == ENOENT) {
				elf = dsl_dataset_collector_cache_alloc();
				err = 0;
				elf->name[0] = '\0';
				elf->ds = NULL;
			}
		} else {
			txg_first = dsl_dataset_phys(pdss)->ds_creation_txg;

			elf = dsl_dataset_collector_cache_alloc();
			(void) strcpy(elf->name, from_full);
			elf->guid = dsl_dataset_phys(pdss)->ds_guid;
			(void) sprintf(elf->sguid, "0x%llx",
			    (unsigned long long)elf->guid);
			elf->ds = pdss;
		}
	} else {
		elf = dsl_dataset_collector_cache_alloc();
		err = 0;
		elf->name[0] = '\0';
		elf->ds = NULL;
	}

	if (txg_last < txg_first)
		err = EXDEV;

	if (!all && !err) {
		if (elf) {
			if (elf->ds)
				dsl_dataset_long_hold(elf->ds, NULL);
			list_insert_tail(ds_to_send, elf);
		}
		dsl_dataset_long_hold(elt->ds, NULL);
		list_insert_tail(ds_to_send, elt);
		dsl_pool_rele(pdp, FTAG);
		return (0);
	}

	if (elt) {
		if (elt->ds)
			dsl_dataset_rele(elt->ds, NULL);
		dsl_dataset_collector_cache_free(elt);
	}

	if (elf && (elf->name[0] != 0 || err)) {
		if (elf->ds)
			dsl_dataset_rele(elf->ds, NULL);
		dsl_dataset_collector_cache_free(elf);
	} else {
		list_insert_tail(ds_to_send, elf);
	}

	if (err) {
		dsl_pool_rele(pdp, FTAG);
		return (err);
	}

	/*
	 * Snapshots must be sorted in the ascending order by birth_txg
	 */
	avl_create(&avl, zfs_snapshot_compare, sizeof (zfs_snap_avl_node_t),
	    offsetof(zfs_snap_avl_node_t, snap_node));

	dsl_dataset_t *ods = NULL;

	os = NULL;
	err = dsl_dataset_hold(pdp, fds, FTAG, &ods);
	if (!err)
		err = dmu_objset_from_ds(ods, &os);

	/* walk over snapshots and add them to the tree to sort */
	while (!err) {
		dsl_dataset_t *pdss;
		zfs_snap_avl_node_t *el =
		    kmem_zalloc(sizeof (zfs_snap_avl_node_t), KM_SLEEP);

		(void) strcpy(el->name, fds);
		(void) strcat(el->name, "@");

		err = dmu_snapshot_list_next(os, MAXNAMELEN - strlen(el->name),
		    el->name + strlen(el->name), &obj, &offp, &cc);
		if (err == ENOENT) {
			err = 0;
			kmem_free(el, sizeof (zfs_snap_avl_node_t));
			break;
		}

		if (!err)
			err = dsl_dataset_hold(pdp, el->name, NULL, &pdss);
		ASSERT(err != ENOENT);
		if (!err) {
			el->guid = dsl_dataset_phys(pdss)->ds_guid;
			(void) sprintf(el->sguid, "0x%llx",
			    (unsigned long long)el->guid);
			el->txg = dsl_dataset_phys(pdss)->ds_creation_txg;

			if (el->txg < txg_first || el->txg > txg_last) {
				dsl_dataset_rele(pdss, NULL);
				kmem_free(el, sizeof (zfs_snap_avl_node_t));
			} else {
				char *snap = strchr(el->name, '@') + 1;
				int cmp = strncmp(snap, AUTOSNAP_PREFIX,
				    AUTOSNAP_PREFIX_LEN);
				if (cmp == 0 &&
				    el->txg != txg_first &&
				    el->txg != txg_last) {
					dsl_dataset_rele(pdss, NULL);
					kmem_free(el,
					    sizeof (zfs_snap_avl_node_t));
				} else {
					el->ds = pdss;
					dsl_dataset_long_hold(pdss, NULL);
					avl_add(&avl, el);
				}
			}
		} else {
			kmem_free(el, sizeof (zfs_snap_avl_node_t));
		}
	}
	if (ods)
		dsl_dataset_rele(ods, FTAG);
	dsl_pool_rele(pdp, FTAG);

	/* move snapshots from the tree to the list in the sorted order */
	if (!err) {
		for (node = avl_first(&avl);
		    node != NULL;
		    node = AVL_NEXT(&avl, node)) {
			zfs_ds_collector_entry_t *el =
			    dsl_dataset_collector_cache_alloc();
			(void) strcpy(el->name, node->name);
			(void) strcpy(el->sguid, node->sguid);
			el->guid = node->guid;
			el->ds = node->ds;
			list_insert_tail(ds_to_send, el);
		}
	}
	while ((node = avl_destroy_nodes(&avl, &cookie)) != NULL) {
		kmem_free(node, sizeof (zfs_snap_avl_node_t));
	}
	avl_destroy(&avl);

	return (err);
}

/* Collect datasets and snapshots of each dataset */
static int
zfs_send_collect_ds(char *from_ds, char *from_snap, char *to_snap,
    boolean_t all, boolean_t recursive, list_t *ds_to_send)
{
	int err = 0;
	zfs_ds_collector_entry_t *ds_el;
	spa_t *spa;
	dsl_pool_t *dp;

	mutex_enter(&spa_namespace_lock);
	spa = spa_lookup(from_ds);
	mutex_exit(&spa_namespace_lock);

	if (!spa)
		return (ENOENT);
	dp = spa_get_dsl(spa);

	dsl_pool_config_enter(dp, FTAG);
	while ((err = zfs_collect_ds(spa, from_ds,
	    recursive, B_TRUE, ds_to_send)) == EBUSY)
		delay(NSEC_TO_TICK(100));
	dsl_pool_config_exit(dp, FTAG);

	ds_el = list_head(ds_to_send);
	while (!err && ds_el) {
		if (ds_el->name[0] == '\0' || strchr(ds_el->name, '@') != NULL)
			break;
		err = zfs_send_collect_snaps(ds_el->name, from_snap,
		    to_snap, all, ds_to_send);
		ds_el = list_next(ds_to_send, ds_el);
	}

	return (err);
}

/* Send a single dataset, mostly mimic regular send */
static int
zfs_send_one_ds(const char *inc_ds, const char *from_ds,
    dmu_krrp_task_t *krrp_task, boolean_t embedok)
{
	dsl_pool_t *dp = NULL;
	dsl_dataset_t *ds = NULL;
	dsl_dataset_t *fromds = NULL;
	int err = 0;
	offset_t off = 0;

	err = dsl_pool_hold(from_ds, FTAG, &dp);
	if (err)
		goto out;

	err = dsl_dataset_hold(dp, from_ds, FTAG, &ds);
	if (err != 0) {
		dsl_pool_rele(dp, FTAG);
		goto out;
	}

	if (krrp_debug) {
		cmn_err(CE_NOTE, "KRRP SEND INC_BASE: %s -- DS: "
		    "%s -- GUID: %llu", inc_ds,
		    from_ds,
		    (unsigned long long)dsl_dataset_phys(ds)->ds_guid);
	}

	if (inc_ds) {
		zfs_bookmark_phys_t zb;
		boolean_t is_clone;
		err = dsl_dataset_hold(dp, inc_ds, FTAG, &fromds);
		if (err) {
			dsl_dataset_rele(ds, FTAG);
			dsl_pool_rele(dp, FTAG);
			goto out;
		}
		if (err || !dsl_dataset_is_before(ds, fromds, 0)) {
			dsl_dataset_rele(fromds, FTAG);
			dsl_dataset_rele(ds, FTAG);
			dsl_pool_rele(dp, FTAG);
			err = EXDEV;
			goto out;
		}
		zb.zbm_creation_time =
		    dsl_dataset_phys(fromds)->ds_creation_time;
		zb.zbm_creation_txg = dsl_dataset_phys(fromds)->ds_creation_txg;
		zb.zbm_guid = dsl_dataset_phys(fromds)->ds_guid;
		is_clone = (fromds->ds_dir != ds->ds_dir);
		dsl_dataset_rele(fromds, FTAG);
		err = dmu_send_impl(FTAG, dp, ds, &zb,
		    is_clone, embedok, B_FALSE,
		    -1, NULL, &off, krrp_task);
	} else {
		err = dmu_send_impl(FTAG, dp, ds, NULL, B_FALSE,
		    embedok, B_FALSE,
		    -1, NULL, &off, krrp_task);
	}

	dsl_dataset_rele(ds, FTAG);
out:
	return (err);
}

/* Sender thread */
static void
zfs_send_thread(void *krrp_task_void)
{
	dmu_replay_record_t drr = { 0 };
	dmu_krrp_task_t *krrp_task = krrp_task_void;
	list_t ds_to_send;
	zfs_ds_collector_entry_t *traverse_root, *prev_snap;
	int err = 0;
	boolean_t separate_thread, a_locked = B_FALSE;
	spa_t *spa;

	ASSERT(krrp_task != NULL);

	separate_thread = krrp_task->buffer_user_thread != NULL;
	list_create(&ds_to_send, sizeof (zfs_ds_collector_entry_t),
	    offsetof(zfs_ds_collector_entry_t, node));

	mutex_enter(&spa_namespace_lock);
	spa = spa_lookup(krrp_task->buffer_args.from_ds);
	mutex_exit(&spa_namespace_lock);

	if (!spa) {
		err = ENOENT;
		goto out;
	}

	if (spa_wrc_present(spa)) {
		if (strchr(krrp_task->buffer_args.from_ds, '/') != NULL ||
		    krrp_task->buffer_args.recursive == B_FALSE) {
			err = ENOTDIR;
			goto out;
		}
	}

	err = autosnap_lock(spa);
	if (err)
		goto out;
	a_locked = B_TRUE;

	err = zfs_send_collect_ds(krrp_task->buffer_args.from_ds,
	    krrp_task->buffer_args.from_incr_base,
	    krrp_task->buffer_args.from_snap,
	    krrp_task->buffer_args.do_all,
	    krrp_task->buffer_args.recursive,
	    &ds_to_send);
	if (err)
		goto out;

	/*
	 * Recursive stream, stream with properties, or complete-incremental
	 * stream have special header (DMU_COMPOUNDSTREAM)
	 */
	if (krrp_task->buffer_args.recursive ||
	    krrp_task->buffer_args.properties ||
	    krrp_task->buffer_args.do_all) {
		zio_cksum_t zc = { 0 };
		nvlist_t *nva, *nvp;
		char *packbuf = NULL;
		size_t buflen = 0;

		VERIFY(0 == nvlist_alloc(&nvp, NV_UNIQUE_NAME, KM_SLEEP));
		VERIFY(0 == nvlist_alloc(&nva, NV_UNIQUE_NAME, KM_SLEEP));

		if (krrp_task->buffer_args.properties) {
			err = zfs_send_collect_properties(&ds_to_send, nva);
			if (err)
				goto out_nv;

			if (krrp_task->buffer_args.from_incr_base[0]) {
				err = nvlist_add_string(nvp, "fromsnap",
				    krrp_task->buffer_args.from_incr_base);
			}
			if (err)
				goto out_nv;

			err = nvlist_add_string(nvp, "tosnap",
			    krrp_task->buffer_args.from_snap);
			if (err)
				goto out_nv;

			if (!krrp_task->buffer_args.recursive) {
				err =
				    nvlist_add_boolean(nvp, "not_recursive");
				if (err)
					goto out_nv;
			}

			err = nvlist_add_nvlist(nvp, "fss", nva);
			if (err)
				goto out_nv;

			err = nvlist_pack(nvp, &packbuf, &buflen,
			    NV_ENCODE_XDR, 0);
			if (err)
				goto out_nv;
		}

		drr.drr_type = DRR_BEGIN;
		drr.drr_u.drr_begin.drr_magic = DMU_BACKUP_MAGIC;
		DMU_SET_STREAM_HDRTYPE(drr.drr_u.drr_begin.drr_versioninfo,
		    DMU_COMPOUNDSTREAM);
		(void) snprintf(drr.drr_u.drr_begin.drr_toname,
		    sizeof (drr.drr_u.drr_begin.drr_toname),
		    "%s@%s", krrp_task->buffer_args.from_ds,
		    krrp_task->buffer_args.from_snap);
		drr.drr_payloadlen = buflen;
		if (krrp_task->buffer_args.force_cksum)
			fletcher_4_incremental_native(&drr, sizeof (drr), &zc);

		err = dmu_krrp_buffer_write(&drr, sizeof (drr), krrp_task);
		if (err)
			goto out_nv;

		if (buflen) {
			if (krrp_task->buffer_args.force_cksum) {
				fletcher_4_incremental_native(
				    packbuf, buflen, &zc);
			}
			err = dmu_krrp_buffer_write(
			    packbuf, buflen, krrp_task);
			if (err)
				goto out_nv;
		}

		bzero(&drr, sizeof (drr));
		drr.drr_type = DRR_END;
		drr.drr_u.drr_end.drr_checksum = zc;

		err = dmu_krrp_buffer_write(&drr, sizeof (drr), krrp_task);

out_nv:
		if (packbuf)
			kmem_free(packbuf, buflen);
		nvlist_free(nva);
		nvlist_free(nvp);
	}

out:

	prev_snap = NULL;
	traverse_root = list_head(&ds_to_send);
	/* List contains fs datasets as well as snaps. Skip fs'es */
	while (traverse_root && traverse_root->name[0] != '\0' &&
	    strchr(traverse_root->name, '@') == NULL)
		traverse_root = list_next(&ds_to_send, traverse_root);

	if (krrp_task->buffer_args.rep_cookie[0] != '\0') {
		const char *last_snap = krrp_task->buffer_args.rep_cookie;
		while (traverse_root &&
		    strcmp(traverse_root->name, last_snap) != 0)
			traverse_root = list_next(&ds_to_send, traverse_root);

		if (traverse_root == NULL) {
			/* It seems the given cookie is incorrect */
			err = ENOSR;
			goto final;
		}

		prev_snap = traverse_root;
		traverse_root = list_next(&ds_to_send, traverse_root);
	}

	/*
	 * For every dataset there is a chain of snapshots. It may start with
	 * an empty record, which means it is a non-incremental snap, after
	 * that this dataset is considered to be under an incremental stream.
	 * In an incremental stream, first snapshot for every dataset is
	 * an incremental base. After sending, currently sent snapshot
	 * becomes a base for the next one unless the next belongs to
	 * another dataset or is an empty record.
	 */
	while (!err && traverse_root) {
		int cmp = (prev_snap == NULL || prev_snap->name[0] == 0) ? 1 :
		    strncmp(prev_snap->name, traverse_root->name,
		    strchr(prev_snap->name, '@') - prev_snap->name + 1);
		if (traverse_root->name[0] == '\0') {
			traverse_root =
			    list_next(&ds_to_send, traverse_root);
			if (traverse_root == NULL)
				break;
			err = zfs_send_one_ds(NULL, traverse_root->name,
			    krrp_task,
			    krrp_task->buffer_args.embedok);
		} else if (prev_snap && cmp == 0) {
			err = zfs_send_one_ds(prev_snap->name,
			    traverse_root->name, krrp_task,
			    krrp_task->buffer_args.embedok);
		}
		prev_snap = traverse_root;
		traverse_root = list_next(&ds_to_send, traverse_root);
	}

final:
	while ((traverse_root = list_head(&ds_to_send)) != NULL) {
		(void) list_remove_head(&ds_to_send);
		if (traverse_root->ds) {
			dsl_dataset_long_rele(traverse_root->ds, NULL);
			dsl_dataset_rele(traverse_root->ds, NULL);
		}
		dsl_dataset_collector_cache_free(traverse_root);
	}
	list_destroy(&ds_to_send);
	if (!err &&
	    (krrp_task->buffer_args.recursive ||
	    krrp_task->buffer_args.properties ||
	    krrp_task->buffer_args.do_all)) {
		bzero(&drr, sizeof (drr));
		drr.drr_type = DRR_END;
		err = dmu_krrp_buffer_write(&drr, sizeof (drr), krrp_task);
	}

	if (!err)
		err = dmu_krrp_put_buffer(krrp_task);
	if (a_locked)
		autosnap_unlock(spa);
	dmu_set_send_recv_error(krrp_task, err);
	if (err)
		cmn_err(CE_WARN, "Send thread exited with error code %d", err);
	(void) dmu_krrp_fini_task(krrp_task);
	if (separate_thread)
		thread_exit();
}

/* KRRP-RECV routines */

#define	ZPROP_INHERIT_SUFFIX "$inherit"
#define	ZPROP_RECVD_SUFFIX "$recvd"
/* Alternate props from the received steam */
static int
zfs_recv_alter_props(nvlist_t *props, nvlist_t *exclude, nvlist_t *replace)
{
	nvpair_t *element = NULL;
	int error = 0;

	if (props && exclude) {
		while (
		    (element = nvlist_next_nvpair(exclude, element)) != NULL) {
			nvpair_t *pair;
			char *prop = nvpair_name(element);
			char *prop_recv;
			char *prop_inher;

			prop_recv =
			    kmem_asprintf("%s%s", prop, ZPROP_RECVD_SUFFIX);
			prop_inher =
			    kmem_asprintf("%s%s", prop, ZPROP_INHERIT_SUFFIX);

			pair = NULL;
			(void) nvlist_lookup_nvpair(props, prop, &pair);
			if (pair)
				fnvlist_remove_nvpair(props, pair);

			pair = NULL;
			(void) nvlist_lookup_nvpair(props, prop_recv, &pair);
			if (pair)
				fnvlist_remove_nvpair(props, pair);

			pair = NULL;
			(void) nvlist_lookup_nvpair(props, prop_inher, &pair);
			if (pair)
				fnvlist_remove_nvpair(props, pair);

			strfree(prop_recv);
			strfree(prop_inher);
		}
	}

	if (props && replace) {
		while (
		    (element = nvlist_next_nvpair(replace, element)) != NULL) {
			nvpair_t *pair;
			char *prop = nvpair_name(element);
			char *prop_recv;
			char *prop_inher;

			prop_recv =
			    kmem_asprintf("%s%s", prop, ZPROP_RECVD_SUFFIX);
			prop_inher =
			    kmem_asprintf("%s%s", prop, ZPROP_INHERIT_SUFFIX);

			pair = NULL;
			(void) nvlist_lookup_nvpair(props, prop, &pair);
			if (pair)
				fnvlist_remove_nvpair(props, pair);

			pair = NULL;
			(void) nvlist_lookup_nvpair(props, prop_recv, &pair);
			if (pair)
				fnvlist_remove_nvpair(props, pair);

			pair = NULL;
			(void) nvlist_lookup_nvpair(props, prop_inher, &pair);
			if (pair)
				fnvlist_remove_nvpair(props, pair);

			strfree(prop_recv);
			strfree(prop_inher);

			error = nvlist_add_nvpair(props, element);
			if (error)
				break;
		}
	}

	return (error);
}

typedef struct {
	const char *token;
	int err;
} dmu_krrp_token_check_t;

int
dmu_krrp_get_recv_cookie(const char *pool, const char *token, char *cookie,
    size_t len)
{
	spa_t *spa;
	dsl_pool_t *dp;
	int err;
	uint64_t int_size, val_length;

	err = spa_open(pool, &spa, FTAG);
	if (err)
		return (err);

	dp = spa_get_dsl(spa);
	err = zap_length(dp->dp_meta_objset,
	    DMU_POOL_DIRECTORY_OBJECT,
	    token, &int_size, &val_length);

	if (err)
		goto out;

	if (int_size != 1) {
		err = EINVAL;
		goto out;
	}

	if (val_length > len) {
		err = ENAMETOOLONG;
		goto out;
	}

	err = zap_lookup(dp->dp_meta_objset,
	    DMU_POOL_DIRECTORY_OBJECT,
	    token, int_size, val_length, cookie);

out:
	spa_close(spa, FTAG);

	return (err);
}

static int
dmu_krrp_erase_recv_cookie_check(void *arg, dmu_tx_t *tx)
{
	dmu_krrp_token_check_t *arg_tok = arg;
	dsl_pool_t *dp = tx->tx_pool;

	return (zap_contains(dp->dp_meta_objset,
	    DMU_POOL_DIRECTORY_OBJECT, arg_tok->token));
}

static void
dmu_krrp_erase_recv_cookie_sync(void *arg, dmu_tx_t *tx)
{
	dsl_pool_t *dp = tx->tx_pool;
	dmu_krrp_token_check_t *arg_tok = arg;

	arg_tok->err = zap_remove(dp->dp_meta_objset,
	    DMU_POOL_DIRECTORY_OBJECT, arg_tok->token, tx);
}

int
dmu_krrp_erase_recv_cookie(const char *pool, const char *token)
{
	spa_t *spa;
	int err;
	dmu_krrp_token_check_t arg_tok = { 0 };

	err = spa_open(pool, &spa, FTAG);
	if (err)
		return (err);

	arg_tok.token = token;

	err = dsl_sync_task(spa_name(spa),
	    dmu_krrp_erase_recv_cookie_check,
	    dmu_krrp_erase_recv_cookie_sync,
	    &arg_tok, 0, ZFS_SPACE_CHECK_NONE);

	if (!err)
		err = arg_tok.err;

	spa_close(spa, FTAG);

	return (err);
}

static int
dmu_krrp_add_recv_cookie_check(void *arg, dmu_tx_t *tx)
{
	dsl_pool_t *dp = tx->tx_pool;
	dmu_krrp_token_check_t *arg_tok = arg;
	int err;

	err = zap_contains(dp->dp_meta_objset,
	    DMU_POOL_DIRECTORY_OBJECT, arg_tok->token);

	if (err == ENOENT) {
		err = 0;
	} else if (err == 0) {
		err = EEXIST;
	}

	return (err);
}

static void
dmu_krrp_add_recv_cookie_sync(void *arg, dmu_tx_t *tx)
{
	dsl_pool_t *dp = tx->tx_pool;
	dmu_krrp_token_check_t *arg_tok = arg;

	arg_tok->err = zap_add(dp->dp_meta_objset,
	    DMU_POOL_DIRECTORY_OBJECT, arg_tok->token, 1, 1, "", tx);
}

/* Recv a single snapshot. It is a simplified version of recv */
static int
zfs_recv_one_ds(char *ds, struct drr_begin *drrb, nvlist_t *props,
    dmu_krrp_task_t *krrp_task)
{
	int err = 0;
	uint64_t errf = 0;
	uint64_t ahdl = 0;
	uint64_t sz = 0;
	char *tosnap;

	if (krrp_task->buffer_args.to_snap[0]) {
		tosnap = krrp_task->buffer_args.to_snap;
	} else {
		tosnap = strchr(drrb->drr_toname, '@') + 1;
	}

	err = zfs_recv_alter_props(props,
	    krrp_task->buffer_args.ignore_list,
	    krrp_task->buffer_args.replace_list);

	if (err)
		return (err);

	if (krrp_debug) {
		cmn_err(CE_NOTE, "KRRP RECV INC_BASE: %llu -- DS: "
		    "%s -- TO_SNAP:%s",
		    (unsigned long long)drrb->drr_fromguid, ds, tosnap);
	}

	/* hack to avoid adding the symnol to the libzpool export list */
#ifdef _KERNEL
	err = dmu_recv_impl(NULL, ds, tosnap, NULL, drrb, props, NULL,
	    &errf, -1, &ahdl, &sz, krrp_task->buffer_args.force,
	    krrp_task);
#endif

	return (err);
}

/* Recv thread */
static void
zfs_recv_thread(void *krrp_task_void)
{
	dmu_krrp_task_t *krrp_task = krrp_task_void;
	dmu_replay_record_t drr = { 0 };
	struct drr_begin *drrb = &drr.drr_u.drr_begin;
	zio_cksum_t zcksum = { 0 };
	int err;
	int baselen;
	boolean_t separate_thread;
	spa_t *spa;
	char latest_snap[MAXNAMELEN] = { 0 };
	dmu_krrp_token_check_t arg_tok = { 0 };

	ASSERT(krrp_task != NULL);
	separate_thread = krrp_task->buffer_user_thread != NULL;

	mutex_enter(&spa_namespace_lock);
	spa = spa_lookup(krrp_task->buffer_args.to_ds);
	mutex_exit(&spa_namespace_lock);

	if (!spa) {
		err = ENOENT;
		goto out;
	}

	if (spa_wrc_present(spa)) {
		if (strchr(krrp_task->buffer_args.to_ds, '/') != NULL) {
			err = ENOTDIR;
			goto out;
		}
	}

	/* Read leading block */
	err = dmu_krrp_buffer_read(&drr, sizeof (drr), krrp_task);
	if (err)
		goto out;

	if (drr.drr_type != DRR_BEGIN ||
	    (drrb->drr_magic != DMU_BACKUP_MAGIC &&
	    drrb->drr_magic != BSWAP_64(DMU_BACKUP_MAGIC))) {
		err = EBADMSG;
		goto out;
	}

	baselen = strchr(drrb->drr_toname, '@') - drrb->drr_toname;

	/* Process passed arguments */
	if (krrp_task->buffer_args.strip_head) {
		char *pos = strchr(drrb->drr_toname, '/');
		if (pos)
			baselen = pos - drrb->drr_toname;
	}

	if (krrp_task->buffer_args.leave_tail) {
		char *pos = strrchr(drrb->drr_toname, '/');
		if (pos)
			baselen = pos - drrb->drr_toname;
	}

	arg_tok.token = krrp_task->buffer_args.to_ds;

	err = dsl_sync_task(spa_name(spa),
	    dmu_krrp_add_recv_cookie_check,
	    dmu_krrp_add_recv_cookie_sync,
	    &arg_tok, 0, ZFS_SPACE_CHECK_RESERVED);

	if (!err)
		err = arg_tok.err;

	if (err == EEXIST)
		err = 0;

	if (err)
		goto out;

	if (DMU_GET_STREAM_HDRTYPE(drrb->drr_versioninfo) == DMU_SUBSTREAM) {
		/* recv a simple single snapshot */
		char full_ds[MAXNAMELEN];

		if (spa_wrc_present(spa)) {
			err = ENOTDIR;
			goto out;
		}
		(void) strcpy(full_ds, krrp_task->buffer_args.to_ds);
		if (krrp_task->buffer_args.strip_head ||
		    krrp_task->buffer_args.leave_tail) {
			char *pos;
			int len = strlen(full_ds) +
			    strlen(drrb->drr_toname + baselen) + 1;
			if (len < MAXNAMELEN) {
				(void) strcat(full_ds, "/");
				(void) strcat(full_ds,
				    drrb->drr_toname + baselen);
				pos = strchr(full_ds, '@');
				*pos = '\0';
			} else {
				err = ENAMETOOLONG;
				goto out;
			}
		}
		(void) strcpy(latest_snap, full_ds);
		(void) strcat(latest_snap, strchr(drrb->drr_toname, '@'));
		err = zfs_recv_one_ds(full_ds, drrb, NULL, krrp_task);
	} else {
		nvlist_t *nvl = NULL, *nvfs = NULL;

		if (krrp_task->buffer_args.force_cksum) {
			fletcher_4_incremental_native(&drr,
			    sizeof (drr), &zcksum);
		}
		/* Recv properties */
		if (drr.drr_payloadlen > 0) {
			char *buf = kmem_alloc(drr.drr_payloadlen, KM_SLEEP);
			err = dmu_krrp_buffer_read(
			    buf, drr.drr_payloadlen, krrp_task);
			if (err) {
				kmem_free(buf, drr.drr_payloadlen);
				goto out_nvl;
			}

			if (krrp_task->buffer_args.force_cksum) {
				fletcher_4_incremental_native(buf,
				    drr.drr_payloadlen, &zcksum);
			}
			err = nvlist_unpack(buf, drr.drr_payloadlen, &nvl, 0);
			kmem_free(buf, drr.drr_payloadlen);

			if (err)
				goto out_nvl;

			err = nvlist_lookup_nvlist(nvl, "fss", &nvfs);
			if (err)
				goto out_nvl;
		}

		/* Check end of stream marker */
		err = dmu_krrp_buffer_read(&drr, sizeof (drr), krrp_task);
		if (drr.drr_type != DRR_END &&
		    drr.drr_type != BSWAP_32(DRR_END))
			err = EBADMSG;

		if (!err &&
		    !ZIO_CHECKSUM_EQUAL(drr.drr_u.drr_end.drr_checksum, zcksum))
			err = ECKSUM;

		/* process all substeams from stream */
		while (!err) {
			nvlist_t *nvp, *prop = NULL;
			char ds[MAXNAMELEN];
			char *at;

			err = dmu_krrp_buffer_read(&drr,
			    sizeof (drr), krrp_task);
			if (err)
				break;
			if (drr.drr_type == DRR_END ||
			    drr.drr_type == BSWAP_32(DRR_END))
				break;
			if (drr.drr_type != DRR_BEGIN ||
			    (drrb->drr_magic != DMU_BACKUP_MAGIC &&
			    drrb->drr_magic != BSWAP_64(DMU_BACKUP_MAGIC))) {
				err = EBADMSG;
				break;
			}
			if (strlen(krrp_task->buffer_args.to_ds) +
			    strlen(drrb->drr_toname + baselen) >= MAXNAMELEN) {
				err = ENAMETOOLONG;
				break;
			}
			(void) strcpy(ds, krrp_task->buffer_args.to_ds);
			(void) strcat(ds, drrb->drr_toname + baselen);
			if (nvfs) {
				char guid[64];
				(void) sprintf(guid, "0x%llx",
				    (unsigned long long)drrb->drr_toguid);
				err = nvlist_lookup_nvlist(nvfs, guid, &nvp);
				if (err)
					break;
				err = nvlist_lookup_nvlist(nvp, "props", &prop);
				if (err)
					break;
			}
			(void) strcpy(latest_snap, ds);
			at = strrchr(ds, '@');
			*at = '\0';
			(void) strcpy(krrp_task->cookie, drrb->drr_toname);
			err = zfs_recv_one_ds(ds, drrb, prop, krrp_task);
		}

out_nvl:
		if (nvl)
			nvlist_free(nvl);
	}

	/* Put final block */
	if (!err) {
		(void) dmu_krrp_put_buffer(krrp_task);
		if (spa_wrc_present(spa) && latest_snap[0])
			autosnap_notify_received(latest_snap);
		err = dmu_krrp_erase_recv_cookie(
		    krrp_task->buffer_args.to_ds,
		    krrp_task->buffer_args.to_ds);
	}
out:
	dmu_set_send_recv_error(krrp_task_void, err);
	if (err) {
		cmn_err(CE_WARN, "Recv thread exited with "
		    "error code %d", err);
	}
	(void) dmu_krrp_fini_task(krrp_task);
	if (separate_thread)
		thread_exit();
}

/* Common send/recv entry point */
static void *
dmu_krrp_init_send_recv(void (*func)(void *), kreplication_zfs_args_t *args)
{
	dmu_krrp_task_t *krrp_task =
	    kmem_zalloc(sizeof (dmu_krrp_task_t), KM_SLEEP);

	if (krrp_task == NULL) {
		cmn_err(CE_WARN, "Can not allocate send buffer");
		return (NULL);
	}

	krrp_task->stream_handler = args->stream_handler;
	krrp_task->buffer_args = *args;
	cv_init(&krrp_task->buffer_state_cv, NULL, CV_DEFAULT, NULL);
	cv_init(&krrp_task->buffer_destroy_cv, NULL, CV_DEFAULT, NULL);
	mutex_init(&krrp_task->buffer_state_lock, NULL,
	    MUTEX_DEFAULT, NULL);

	if (args->force_thread) {
		krrp_task->buffer_user_thread =
		    thread_create(NULL, 32 << 10, func,
		    krrp_task, 0, &p0, TS_RUN, minclsyspri);
	} else {
		krrp_task->buffer_user_task = spa_dispatch_krrp_task(
		    *args->to_ds ? args->to_ds : args->from_ds,
		    func, krrp_task);
	}

	if (krrp_task->buffer_user_thread == NULL &&
	    krrp_task->buffer_user_task == NULL) {
		cmn_err(CE_WARN, "Can not create send/recv thread");
		mutex_destroy(&krrp_task->buffer_state_lock);
		cv_destroy(&krrp_task->buffer_state_cv);
		cv_destroy(&krrp_task->buffer_destroy_cv);
		kmem_free(krrp_task, sizeof (dmu_krrp_task_t));
		return (NULL);
	}

	return (krrp_task);
}

void *
dmu_krrp_init_send_task(void *args)
{
	kreplication_zfs_args_t *zfs_args = args;
	ASSERT(zfs_args != NULL);
	*zfs_args->to_ds = '\0';
	return (dmu_krrp_init_send_recv(zfs_send_thread, zfs_args));
}

void *
dmu_krrp_init_recv_task(void *args)
{
	kreplication_zfs_args_t *zfs_args = args;
	ASSERT(zfs_args != NULL);
	*zfs_args->from_ds = '\0';
	return (dmu_krrp_init_send_recv(zfs_recv_thread, zfs_args));
}

static void
dmu_set_send_recv_error(void *krrp_task_void, int err)
{
	dmu_krrp_task_t *krrp_task = krrp_task_void;

	ASSERT(krrp_task != NULL);

	mutex_enter(&krrp_task->buffer_state_lock);
	krrp_task->buffer_error = err;
	mutex_exit(&krrp_task->buffer_state_lock);
}

/*
 * Finalize send/recv task
 * Finalization is two step process, both sides should finalize stream in order
 * to proceed. Finalization is an execution barier - a thread which ends first
 * will wait for another
 */
int
dmu_krrp_fini_task(void *krrp_task_void)
{
	dmu_krrp_task_t *krrp_task = krrp_task_void;
	int error;

	ASSERT(krrp_task != NULL);

	mutex_enter(&krrp_task->buffer_state_lock);
	if (krrp_task->buffer_state == SBS_DESTROYED) {
		cv_signal(&krrp_task->buffer_destroy_cv);
		error = krrp_task->buffer_error;
		mutex_exit(&krrp_task->buffer_state_lock);
	} else {
		krrp_task->buffer_state = SBS_DESTROYED;
		cv_signal(&krrp_task->buffer_state_cv);
		cv_wait(&krrp_task->buffer_destroy_cv,
		    &krrp_task->buffer_state_lock);
		error = krrp_task->buffer_error;
		mutex_exit(&krrp_task->buffer_state_lock);
		mutex_destroy(&krrp_task->buffer_state_lock);
		cv_destroy(&krrp_task->buffer_state_cv);
		cv_destroy(&krrp_task->buffer_destroy_cv);
		kmem_free(krrp_task, sizeof (dmu_krrp_task_t));
	}

	return (error);
}

/* Wait for a lent buffer */
static int
dmu_krrp_get_buffer(void *krrp_task_void)
{
	dmu_krrp_task_t *krrp_task = krrp_task_void;

	ASSERT(krrp_task != NULL);

	mutex_enter(&krrp_task->buffer_state_lock);
	while (krrp_task->buffer_state != SBS_AVAIL) {
		if (krrp_task->buffer_state == SBS_DESTROYED) {
			mutex_exit(&krrp_task->buffer_state_lock);
			return (ENOMEM);
		}
		DTRACE_PROBE(wait_for_buffer);
		(void) cv_timedwait(&krrp_task->buffer_state_cv,
		    &krrp_task->buffer_state_lock,
		    ddi_get_lbolt() + zfs_send_timeout * hz);
		DTRACE_PROBE(wait_for_buffer_end);
	}
	krrp_task->buffer_state = SBS_USED;
	mutex_exit(&krrp_task->buffer_state_lock);

	return (0);
}

/* Return buffer to transport */
static int
dmu_krrp_put_buffer(void *krrp_task_void)
{
	dmu_krrp_task_t *krrp_task = krrp_task_void;

	ASSERT(krrp_task != NULL);

	mutex_enter(&krrp_task->buffer_state_lock);
	if (krrp_task->buffer_state != SBS_USED) {
		mutex_exit(&krrp_task->buffer_state_lock);
		return (0);
	}
	krrp_task->buffer_state = SBS_DONE;
	krrp_task->is_full = (krrp_task->buffer == NULL);
	krrp_task->buffer = NULL;
	cv_signal(&krrp_task->buffer_state_cv);
	mutex_exit(&krrp_task->buffer_state_lock);

	return (0);
}

/* Common entry point for lending buffer */
static int
dmu_krrp_lend_buffer(void *krrp_task_void,
    kreplication_buffer_t *buffer, boolean_t recv)
{
	dmu_krrp_task_t *krrp_task = krrp_task_void;
	boolean_t full;

	ASSERT(krrp_task != NULL);
	ASSERT(buffer != NULL);
	ASSERT(krrp_task->buffer == NULL);

	mutex_enter(&krrp_task->buffer_state_lock);
	if (krrp_task->buffer_state == SBS_DESTROYED) {
		int error = krrp_task->buffer_error;
		mutex_exit(&krrp_task->buffer_state_lock);
		if (error)
			return (error);
		if (recv)
			return (E2BIG);
		return (ENODATA);
	}
	krrp_task->buffer = buffer;
	krrp_task->buffer_state = SBS_AVAIL;
	krrp_task->buffer_bytes_read = 0;
	krrp_task->is_read = B_FALSE;
	krrp_task->is_full = B_FALSE;
	cv_signal(&krrp_task->buffer_state_cv);
	while (krrp_task->buffer_state != SBS_DONE) {
		if (krrp_task->buffer_state == SBS_DESTROYED) {
			int error = krrp_task->buffer_error;
			full = krrp_task->is_full;
			mutex_exit(&krrp_task->buffer_state_lock);
			if (error)
				return (error);
			if (recv && !krrp_task->is_read)
				return (E2BIG);
			return ((recv || full) ? 0 : ENODATA);
		}
		DTRACE_PROBE(wait_for_data);
		(void) cv_timedwait(&krrp_task->buffer_state_cv,
		    &krrp_task->buffer_state_lock,
		    ddi_get_lbolt() + zfs_send_timeout * hz);
		DTRACE_PROBE(wait_for_data_end);
	}
	krrp_task->buffer = NULL;
	full = krrp_task->is_full;
	mutex_exit(&krrp_task->buffer_state_lock);

	return ((recv || full) ? 0 : ENODATA);
}

int
dmu_krrp_lend_send_buffer(void *krrp_task_void, kreplication_buffer_t *buffer)
{
	ASSERT(buffer != NULL);
	kreplication_buffer_t *iter;
	for (iter = buffer; iter != NULL; iter = iter->next)
		iter->data_size = 0;
	return (dmu_krrp_lend_buffer(krrp_task_void, buffer, B_FALSE));
}

int
dmu_krrp_lend_recv_buffer(void *krrp_task_void, kreplication_buffer_t *buffer)
{
	ASSERT(buffer != NULL);
	return (dmu_krrp_lend_buffer(krrp_task_void, buffer, B_TRUE));
}
