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
 * Copyright 2009 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 * Copyright 2015 Joyent, Inc.
 * Copyright 2024 MNX Cloud, Inc.
 */

#ifndef _SYS_LX_FCNTL_H
#define	_SYS_LX_FCNTL_H

#include <sys/vnode.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Lx open/fcntl flags
 */
#define	LX_O_RDONLY		00
#define	LX_O_WRONLY		01
#define	LX_O_RDWR		02
#define	LX_O_ACCMODE		(LX_O_RDONLY | LX_O_WRONLY | LX_O_RDWR)
#define	LX_O_CREAT		0100
#define	LX_O_EXCL		0200
#define	LX_O_NOCTTY		0400
#define	LX_O_TRUNC		01000
#define	LX_O_APPEND		02000
#define	LX_O_NONBLOCK		04000
#define	LX_O_NDELAY		LX_O_NONBLOCK
#define	LX_O_SYNC		010000
#define	LX_O_FSYNC		LX_O_SYNC
#define	LX_O_ASYNC		020000
#define	LX_O_DIRECT		040000
#define	LX_O_LARGEFILE		0100000
#define	LX_O_DIRECTORY		0200000
#define	LX_O_NOFOLLOW		0400000
#define	LX_O_CLOEXEC		02000000
#define	LX_O_PATH		010000000

#define	LX_F_DUPFD		0
#define	LX_F_GETFD		1
#define	LX_F_SETFD		2
#define	LX_F_GETFL		3
#define	LX_F_SETFL		4
#define	LX_F_GETLK		5
#define	LX_F_SETLK		6
#define	LX_F_SETLKW		7
#define	LX_F_SETOWN		8
#define	LX_F_GETOWN		9
#define	LX_F_SETSIG		10
#define	LX_F_GETSIG		11

#define	LX_F_GETLK64		12
#define	LX_F_SETLK64		13
#define	LX_F_SETLKW64		14

#define	LX_F_OFD_GETLK		36
#define	LX_F_OFD_SETLK		37
#define	LX_F_OFD_SETLKW		38

#define	LX_F_SETLEASE		1024
#define	LX_F_GETLEASE		1025
#define	LX_F_NOTIFY		1026
#define	LX_F_CANCELLK		1029
#define	LX_F_DUPFD_CLOEXEC	1030
#define	LX_F_SETPIPE_SZ		1031
#define	LX_F_GETPIPE_SZ		1032

#define	LX_F_RDLCK		0
#define	LX_F_WRLCK		1
#define	LX_F_UNLCK		2

/* Test for emulated O_PATH setting in file_t flags */
#define	LX_IS_O_PATH(f)		(((f)->f_flag & (FREAD|FWRITE)) == 0)

extern int lx_vp_at(int, char *, vnode_t **, int);

/*
 * Lx flock codes.
 */
#define	LX_NAME_MAX		255
#define	LX_LOCK_SH		1	/* shared */
#define	LX_LOCK_EX		2	/* exclusive */
#define	LX_LOCK_NB		4	/* non-blocking */
#define	LX_LOCK_UN		8	/* unlock */

/*
 * On Linux the constants AT_REMOVEDIR and AT_EACCESS have the same value.
 * AT_REMOVEDIR is used only by unlinkat and AT_EACCESS is used only by
 * faccessat.
 */
#define	LX_AT_FDCWD		(-100)
#define	LX_AT_SYMLINK_NOFOLLOW	0x100
#define	LX_AT_REMOVEDIR		0x200
#define	LX_AT_EACCESS		0x200
#define	LX_AT_SYMLINK_FOLLOW	0x400
#define	LX_AT_NO_AUTOMOUNT	0x800
#define	LX_AT_EMPTY_PATH	0x1000

typedef struct lx_flock {
	short		l_type;
	short		l_whence;
	long		l_start;
	long		l_len;
	int		l_pid;
} lx_flock_t;

typedef struct lx_flock64 {
	short		l_type;
	short		l_whence;
	long long	l_start;
	long long	l_len;
	int		l_pid;
} lx_flock64_t;

#if defined(_KERNEL)

/*
 * 64-bit kernel view of 32-bit usermode structs.
 */
#pragma pack(4)
typedef struct lx_flock32 {
	int16_t		l_type;
	int16_t		l_whence;
	int32_t		l_start;
	int32_t		l_len;
	int32_t		l_pid;
} lx_flock32_t;

typedef struct lx_flock64_32 {
	int16_t		l_type;
	int16_t		l_whence;
	int64_t		l_start;
	int64_t		l_len;
	int32_t		l_pid;
} lx_flock64_32_t;
#pragma pack()

#endif /* _KERNEL && _SYSCALL32_IMPL */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_LX_FCNTL_H */
