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
 */

/*
 * Copyright (c) 2011 by Delphix. All rights reserved.
 */

#ifndef _SPAWN_IMPL__H
#define	_SPAWN_IMPL__H

#include <sys/types.h>

#ifdef	__cplusplus
extern "C" {
#endif

#define	ALL_POSIX_SPAWN_FLAGS			\
		(POSIX_SPAWN_RESETIDS |		\
		POSIX_SPAWN_SETPGROUP |		\
		POSIX_SPAWN_SETSIGDEF |		\
		POSIX_SPAWN_SETSIGMASK |	\
		POSIX_SPAWN_SETSCHEDPARAM |	\
		POSIX_SPAWN_SETSCHEDULER |	\
		POSIX_SPAWN_SETSID |		\
		POSIX_SPAWN_SETSIGIGN_NP |	\
		POSIX_SPAWN_NOSIGCHLD_NP |	\
		POSIX_SPAWN_WAITPID_NP |	\
		POSIX_SPAWN_NOEXECERR_NP)

/*
 * Ensure that this struct retains the same layout in both 32- and 64-bit
 * binaries. It is passed to the kernel via spawn(2).
 */
typedef struct {
	int		sa_psflags;	/* POSIX_SPAWN_* flags */
	int		sa_priority;
	int		sa_schedpolicy;
	pid_t		sa_pgroup;
	sigset_t	sa_sigdefault;
	sigset_t	sa_sigignore;
	sigset_t	sa_sigmask;
} spawn_attr_t;

typedef enum file_action {
	FA_OPEN,
	FA_CLOSE,
	FA_DUP2,
	FA_CLOSEFROM,
	FA_CHDIR,
	FA_FCHDIR
} file_action_t;

typedef struct file_attr {
	struct file_attr *fa_next;	/* circular list of file actions */
	struct file_attr *fa_prev;
	file_action_t	fa_type;	/* type of action */
	int		fa_need_dirbuf;	/* only consulted in the head action */
	char		*fa_path;	/* copied pathname for open() */
	uint_t		fa_pathsize;	/* size of fa_path[] array */
	int		fa_oflag;	/* oflag for open() */
	mode_t		fa_mode;	/* mode for open() */
	int		fa_filedes;	/* file descriptor for open()/close() */
	int		fa_newfiledes;	/* new file descriptor for dup2() */
} file_attr_t;

/*
 * We need to marshall all of the data that spawn(2) needs. We could pass
 * the spawn_attr_t directly but the set of file actions needs to be packed
 * into something that the kernel can quickly copy in and parse. There are
 * additional data items too such as the shell and PATH to use for
 * posix_spawnp(). We therefore pack everything into a new structure -
 * spawn_param_t. The following structures have the same layout in both 32-
 * and 64-bit code.
 */
typedef struct kfile_attr {
	uint32_t	kfa_len;	/* size of this record */
	file_action_t	kfa_type;	/* type of action */
	uint32_t	kfa_pathsize;	/* size of fa_path[] array (can be 0) */
	uint32_t	kfa_oflag;	/* oflag for open() */
	uint32_t	kfa_mode;	/* mode for open() */
	int32_t		kfa_filedes;	/* file descriptor for open()/close() */
	int32_t		kfa_newfiledes;	/* new file descriptor for dup2() */
	char		kfa_path[];	/* pathname for open()/chdir() */
} kfile_attr_t;

typedef struct spawn_param {
	uint32_t	sp_size;
	uint32_t	sp_attr_off;	/* Offset of spawn_attr_t */
	uint32_t	sp_attr_len;	/* Length of spawn_attr_t */
	uint32_t	sp_fattr_off;	/* Offset of the first file attribute */
	uint32_t	sp_fattr_cnt;	/* Number of file attributes */
#if SPAWNP_IN_KERNEL_TBD
	uint32_t	sp_shell_off;	/* Offset of the shell */
	uint32_t	sp_shell_len;	/* Length of the shell */
	uint32_t	sp_path_off;	/* Offset of the PATH */
	uint32_t	sp_path_len;	/* Length of the PATH */
#endif
	uint8_t		sp_data[];
} spawn_param_t;

#ifdef _KERNEL

typedef struct kspawn_param {
	kmutex_t	ksp_lock;
	kcondvar_t	ksp_cv;
} kspawn_param_t;

#endif

#ifdef	__cplusplus
}
#endif

#endif	/* _SPAWN_IMPL__H */
