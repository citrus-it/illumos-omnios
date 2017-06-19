/*
 * Copyright 2017 Josef 'Jeff' Sipek <jeffpc@josefsipek.net>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _SYS_VFS_DISPATCH_H
#define _SYS_VFS_DISPATCH_H

#include <sys/vfs.h>
#include <sys/stdbool.h>
#include <sys/fem.h>

/*
 * Ideally these static inlines could be just inline statements in the
 * corresponding fsop_*() functions.  Unfortunately, fem gets in the way and
 * we have to make these to centralize the damage.
 *
 * See comment in sys/vnode_dispatch.h for more details and an example.
 */

#define VFS_DISPATCH(fname, opname, vheadname, args, callargs)		\
static inline int fname args						\
{									\
	if (vfs->vfs_op->opname == NULL)				\
		return ENOSYS;						\
	return vfs->vfs_op->opname callargs;				\
}

VFS_DISPATCH(fsop_mount_dispatch, vfs_mount, fshead_mount,
    (struct vfs *vfs, struct vnode *mvnode, struct mounta *uap, cred_t *cr),
    (vfs, mvnode, uap, cr))
VFS_DISPATCH(fsop_unmount_dispatch, vfs_unmount, fshead_unmount,
    (struct vfs *vfs, int flag, cred_t *cr),
    (vfs, flag, cr))
VFS_DISPATCH(fsop_root_dispatch, vfs_root, fshead_root,
    (struct vfs *vfs, struct vnode **vnode),
    (vfs, vnode))
VFS_DISPATCH(fsop_statfs_dispatch, vfs_statvfs, fshead_statvfs,
    (struct vfs *vfs, statvfs64_t *sp),
    (vfs, sp))
VFS_DISPATCH(fsop_sync_dispatch, vfs_sync, fshead_sync,
    (struct vfs *vfs, short flag, cred_t *cr),
    (vfs, flag, cr))
VFS_DISPATCH(fsop_vget_dispatch, vfs_vget, fshead_vget,
    (struct vfs *vfs, struct vnode **vnode, fid_t *fid),
    (vfs, vnode, fid))
VFS_DISPATCH(fsop_mountroot_dispatch, vfs_mountroot, fshead_mountroot,
    (struct vfs *vfs, enum whymountroot reason),
    (vfs, reason))

static inline void fsop_freefs_dispatch(struct vfs *vfs)
{
	if (vfs->vfs_op->vfs_freevfs != NULL)
		vfs->vfs_op->vfs_freevfs(vfs);
}

VFS_DISPATCH(fsop_vnstate_dispatch, vfs_vnstate, fshead_vnstate,
    (struct vfs *vfs, struct vnode *vnode, vntrans_t nstate),
    (vfs, vnode, nstate))

#undef VFS_DISPATCH

#endif
