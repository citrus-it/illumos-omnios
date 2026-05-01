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
 * Copyright 2026 Oxide Computer Company
 */

/*
 * This file contains the spawn(2) implementation. A private system call used
 * to implement posix_spawn(3C).
 */

#include <sys/cmn_err.h>
#include <sys/ddi.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/fork.h>
#include <sys/param.h>
#include <sys/proc.h>
#include <sys/spawn.h>
#include <sys/spawn_impl.h>
#include <sys/sunddi.h>
#include <sys/sysmacros.h>
#include <sys/types.h>

// Where can we get this from given <limits.h> is not visible
#define	ARG_MAX 2096640

extern int64_t cfork(int, int, kspawn_param_t *, int);

void
spawn_main(void *arg)
{
	kspawn_param_t *ksp = arg;

	for (;;) {
		cmn_err(CE_NOTE, "I'm a spawn child!");
		delay((clock_t)drv_usectohz(30 * MICROSEC));
	}

	// spawn_attr_apply(ksp);
	// 	signal handlers
	// 	real UID/GID
	// 	process group
	// 	setsid
	// 	scheduling class/priority
	// spawn_fileaction_apply(ksp);
	// 	flist_fork() has already done a selective copy?
	// 	apply file actions
	// spawn_exec(...);
	// 	handle PATH for p variant, shell, etc.

#if 0
	mutex_enter(&ksp->ksp_lock);
	ksp->ksp_complete = true;
	cv_signal(&ksp->ksp_cv);
	mutex_exit(&ksp->ksp_lock);
#endif
}

static int
spawn_forkflags(spawn_param_t *sp)
{
	int flags = 0;

	if (sp != NULL && sp->sp_attr_len != 0) {
		const spawn_attr_t *spa =
		    (const spawn_attr_t *)&sp->sp_data[sp->sp_attr_off];

		if ((spa->sa_psflags & POSIX_SPAWN_NOSIGCHLD_NP) != 0)
			flags |= FORK_NOSIGCHLD;
		if ((spa->sa_psflags & POSIX_SPAWN_WAITPID_NP) != 0)
			flags |= FORK_WAITPID;
	}

	return (flags);
}

int64_t
spawn(void *path, void *sparam, uint32_t spsize, void *sargs, uint32_t sasize)
{
	kspawn_param_t *ksp = NULL;
	spawn_param_t *sp = NULL;
	spawn_args_t *sa = NULL;
	int64_t ret = -1;
	int err = 0;

	if (path == NULL || sargs == NULL || sasize < sizeof (*sa))
		return ((int64_t)set_errno(EINVAL));

	if (spsize > ARG_MAX || sasize > ARG_MAX)
		return ((int64_t)set_errno(E2BIG));

	if (spsize > 0) {
		if (spsize < sizeof (spawn_param_t))
			return ((int64_t)set_errno(EINVAL));

		sp = kmem_alloc(spsize, KM_NOSLEEP);
		if (sp == NULL) {
			err = ENOMEM;
			goto out;
		}
		if (copyin(sparam, sp, spsize) != 0) {
			err = EFAULT;
			goto out;
		}
	}

	sa = kmem_alloc(sasize, KM_NOSLEEP);
	if (sa == NULL) {
		err = ENOMEM;
		goto out;
	}
	if (copyin(sargs, sa, sasize) != 0) {
		err = EFAULT;
		goto out;
	}

	ksp = kmem_zalloc(sizeof (*ksp), KM_NOSLEEP);
	if (ksp == NULL) {
		err = ENOMEM;
		goto out;
	}

	err = copyinstr(path, ksp->ksp_path, sizeof (ksp->ksp_path), NULL);
	if (err != 0)
		goto out;

	// PATH, shell, etc...

	mutex_init(&ksp->ksp_lock, NULL, MUTEX_DEFAULT, NULL);
	cv_init(&ksp->ksp_cv, NULL, CV_DEFAULT, NULL);
	ksp->ksp_param = sp;
	ksp->ksp_args = sa;

	// Pre-scan file actions to set limits on fd table copying.
	// spawn_fdtable_optimiser(ksp)?
	// Separate function. We can:
	//  - skip anything above any closefrom() action (after accounting
	//    for dup2 actions)
	//  - skip anything with O_CLOEXEC (after accounting for actions)
	//
	//  Here or in flist_fork()? Or do we have an flist_spawn() variant?

	mutex_enter(&ksp->ksp_lock);
	ret = cfork(0, 0, ksp, spawn_forkflags(sp));
	if (ttolwp(curthread)->lwp_errno == 0) {
		while (!ksp->ksp_complete)
			cv_wait(&ksp->ksp_cv, &ksp->ksp_lock);
	}
	mutex_exit(&ksp->ksp_lock);

	mutex_destroy(&ksp->ksp_lock);
	cv_destroy(&ksp->ksp_cv);

out:
	if (sp != NULL)
		kmem_free(sp, spsize);
	if (sa != NULL)
		kmem_free(sa, sasize);
	if (ksp != NULL)
		kmem_free(ksp, sizeof (*ksp));

	if (err != 0)
		return ((int64_t)set_errno(err));
	return (ret);
}
