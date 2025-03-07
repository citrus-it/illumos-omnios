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
 * Copyright (c) 1988, 2010, Oracle and/or its affiliates. All rights reserved.
 */

/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/
/*
 * Copyright 2015 Garrett D'Amore <garrett@damore.org>
 * Copyright 2019 Joyent, Inc.
 * Copyright 2024 Oxide Computer Company
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/signal.h>
#include <sys/cred_impl.h>
#include <sys/policy.h>
#include <sys/user.h>
#include <sys/errno.h>
#include <sys/file.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/mman.h>
#include <sys/acct.h>
#include <sys/cpuvar.h>
#include <sys/proc.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/pathname.h>
#include <sys/vm.h>
#include <sys/lgrp.h>
#include <sys/vtrace.h>
#include <sys/exec.h>
#include <sys/execx.h>
#include <sys/exechdr.h>
#include <sys/kmem.h>
#include <sys/prsystm.h>
#include <sys/modctl.h>
#include <sys/vmparam.h>
#include <sys/door.h>
#include <sys/schedctl.h>
#include <sys/utrap.h>
#include <sys/systeminfo.h>
#include <sys/stack.h>
#include <sys/rctl.h>
#include <sys/dtrace.h>
#include <sys/lwpchan_impl.h>
#include <sys/pool.h>
#include <sys/sdt.h>
#include <sys/brand.h>
#include <sys/klpd.h>
#include <sys/random.h>

#include <c2/audit.h>

#include <vm/hat.h>
#include <vm/anon.h>
#include <vm/as.h>
#include <vm/seg.h>
#include <vm/seg_vn.h>
#include <vm/seg_hole.h>

#define	PRIV_RESET		0x01	/* needs to reset privs */
#define	PRIV_SETID		0x02	/* needs to change uids */
#define	PRIV_SETUGID		0x04	/* is setuid/setgid/forced privs */
#define	PRIV_INCREASE		0x08	/* child runs with more privs */
#define	MAC_FLAGS		0x10	/* need to adjust MAC flags */
#define	PRIV_FORCED		0x20	/* has forced privileges */

static int execsetid(struct vnode *, struct vattr *, uid_t *, uid_t *,
    priv_set_t *, cred_t *, const char *);
static int hold_execsw(struct execsw *);

uint_t auxv_hwcap = 0;	/* auxv AT_SUN_HWCAP value; determined on the fly */
uint_t auxv_hwcap_2 = 0;	/* AT_SUN_HWCAP2 */
uint_t auxv_hwcap_3 = 0;	/* AT_SUN_HWCAP3 */
#if defined(_SYSCALL32_IMPL)
uint_t auxv_hwcap32 = 0;	/* 32-bit version of auxv_hwcap */
uint_t auxv_hwcap32_2 = 0;	/* 32-bit version of auxv_hwcap2 */
uint_t auxv_hwcap32_3 = 0;	/* 32-bit version of auxv_hwcap3 */
#endif

#define	PSUIDFLAGS		(SNOCD|SUGID)
#define	RANDOM_LEN	16	/* 16 bytes for AT_RANDOM aux entry */

/*
 * These are consumed within the specific exec modules, but are defined here
 * because
 *
 * 1) The exec modules are unloadable, which would make this near useless.
 *
 * 2) We want them to be common across all of them, should more than ELF come
 *    to support them.
 *
 * All must be powers of 2.
 */
size_t aslr_max_brk_skew = 16 * 1024 * 1024; /* 16MB */
#pragma weak exec_stackgap = aslr_max_stack_skew /* Old, compatible name */
size_t aslr_max_stack_skew = 64 * 1024; /* 64KB */

/*
 * Size of guard segment for 64-bit processes and minimum size it can be shrunk
 * to in the case of grow() operations.  These are kept as variables in case
 * they need to be tuned in an emergency.
 */
size_t stack_guard_seg_sz = 256 * 1024 * 1024;
size_t stack_guard_min_sz = 64 * 1024 * 1024;

/*
 * exece() - system call wrapper around exec_common()
 */
int
exece(uintptr_t file, const char **argp, const char **envp, int flags)
{
	int error;

	if ((flags & ~EXEC_DESCRIPTOR) != 0)
		return (set_errno(EINVAL));

	if ((flags & EXEC_DESCRIPTOR) != 0) {
		/*
		 * If EXEC_DESCRIPTOR is specified, then the `file`
		 * parameter is the number of a file descriptor in the current
		 * process.
		 */
		char *path = NULL;
		size_t allocsize;
		int fd = (int)file;
		vnode_t *vp = NULL;

		if ((error = fgetstartvp(fd, NULL, &vp)) != 0)
			return (set_errno(error));

		mutex_enter(&vp->v_lock);
		if (vp->v_path != NULL && vp->v_path != vn_vpath_empty) {
			allocsize = strlen(vp->v_path) + 1;
			path = kmem_alloc(allocsize, KM_NOSLEEP);
			if (path == NULL) {
				mutex_exit(&vp->v_lock);
				VN_RELE(vp);
				return (set_errno(ENOMEM));
			}
			bcopy(vp->v_path, path, allocsize);
		}
		mutex_exit(&vp->v_lock);

		/*
		 * In the unlikely event that the descriptor's path is not
		 * cached, we fall back to using a constructed one.
		 */
		if (path == NULL) {
			/* 8 for "/dev/fd/", 10 for %d, + \0 == 19 */
			allocsize = 20;
			path = kmem_alloc(allocsize, KM_NOSLEEP);
			if (path == NULL) {
				VN_RELE(vp);
				return (set_errno(ENOMEM));
			}
			(void) snprintf(path, allocsize, "/dev/fd/%d", fd);
		}

		error = exec_common(path, argp, envp, vp, EBA_NONE);
		VN_RELE(vp);
		kmem_free(path, allocsize);
	} else {
		const char *fname = (const char *)file;

		error = exec_common(fname, argp, envp, NULL, EBA_NONE);
	}

	return (error ? (set_errno(error)) : 0);
}

int
exec_common(const char *fname, const char **argp, const char **envp,
    vnode_t *vp, int brand_action)
{
	vnode_t *dir = NULL, *tmpvp = NULL;
	proc_t *p = ttoproc(curthread);
	klwp_t *lwp = ttolwp(curthread);
	struct user *up = PTOU(p);
	size_t execsz;		/* temporary count of exec size */
	int i;
	int error;
	char exec_file[MAXCOMLEN+1];
	struct pathname pn;
	struct pathname resolvepn;
	struct uarg args;
	struct execa ua;
	k_sigset_t savedmask;
	lwpdir_t *lwpdir = NULL;
	tidhash_t *tidhash;
	lwpdir_t *old_lwpdir = NULL;
	uint_t old_lwpdir_sz;
	tidhash_t *old_tidhash;
	uint_t old_tidhash_sz;
	ret_tidhash_t *ret_tidhash;
	lwpent_t *lep;
	boolean_t brandme = B_FALSE;

	/*
	 * exec() is not supported for the /proc agent lwp.
	 */
	if (curthread == p->p_agenttp)
		return (ENOTSUP);

	if (brand_action != EBA_NONE) {
		/*
		 * Brand actions are not supported for processes that are not
		 * running in a branded zone.
		 */
		if (!ZONE_IS_BRANDED(p->p_zone))
			return (ENOTSUP);

		if (brand_action == EBA_NATIVE) {
			/* Only branded processes can be unbranded */
			if (!PROC_IS_BRANDED(p))
				return (ENOTSUP);
		} else {
			/* Only unbranded processes can be branded */
			if (PROC_IS_BRANDED(p))
				return (ENOTSUP);
			brandme = B_TRUE;
		}
	} else {
		/*
		 * If this is a native zone, or if the process is already
		 * branded, then we don't need to do anything.  If this is
		 * a native process in a branded zone, we need to brand the
		 * process as it exec()s the new binary.
		 */
		if (ZONE_IS_BRANDED(p->p_zone) && !PROC_IS_BRANDED(p))
			brandme = B_TRUE;
	}

	/*
	 * Inform /proc that an exec() has started.
	 * Hold signals that are ignored by default so that we will
	 * not be interrupted by a signal that will be ignored after
	 * successful completion of gexec().
	 */
	mutex_enter(&p->p_lock);
	prexecstart();
	schedctl_finish_sigblock(curthread);
	savedmask = curthread->t_hold;
	sigorset(&curthread->t_hold, &ignoredefault);
	mutex_exit(&p->p_lock);

	if (vp != NULL) {
		/*
		 * When a vnode is passed in we take an extra hold here and
		 * release it before returning. This means that callers don't
		 * need to account for the reference changing over the call.
		 */
		VN_HOLD(vp);
		pn_alloc(&pn);
		pn_alloc(&resolvepn);
		VERIFY0(pn_set(&pn, fname));
		VERIFY0(pn_set(&resolvepn, fname));
	} else {
		/*
		 * Look up path name and remember last component for later.
		 * To help coreadm expand its %d token, we attempt to save
		 * the directory containing the executable in p_execdir. The
		 * first call to lookuppn() may fail and return EINVAL because
		 * dirvpp is non-NULL. In that case, we make a second call to
		 * lookuppn() with dirvpp set to NULL; p_execdir will be NULL,
		 * but coreadm is allowed to expand %d to the empty string and
		 * there are other cases in which that failure may occur.
		 */
		if ((error = pn_get((char *)fname, UIO_USERSPACE, &pn)) != 0)
			goto out;
		pn_alloc(&resolvepn);
		error = lookuppn(&pn, &resolvepn, FOLLOW, &dir, &vp);
		if (error != 0) {
			pn_free(&resolvepn);
			pn_free(&pn);
			if (error != EINVAL)
				goto out;

			dir = NULL;
			if ((error = pn_get((char *)fname, UIO_USERSPACE,
			    &pn)) != 0) {
				goto out;
			}
			pn_alloc(&resolvepn);
			if ((error = lookuppn(&pn, &resolvepn, FOLLOW, NULLVPP,
			    &vp)) != 0) {
				pn_free(&resolvepn);
				pn_free(&pn);
				goto out;
			}
		}
	}

	if (vp == NULL) {
		if (dir != NULL)
			VN_RELE(dir);
		error = ENOENT;
		pn_free(&resolvepn);
		pn_free(&pn);
		goto out;
	}

	if ((error = secpolicy_basic_exec(CRED(), vp)) != 0) {
		if (dir != NULL)
			VN_RELE(dir);
		pn_free(&resolvepn);
		pn_free(&pn);
		VN_RELE(vp);
		goto out;
	}

	/*
	 * We do not allow executing files in attribute directories.
	 * We test this by determining whether the resolved path
	 * contains a "/" when we're in an attribute directory;
	 * only if the pathname does not contain a "/" the resolved path
	 * points to a file in the current working (attribute) directory.
	 */
	mutex_enter(&p->p_lock);
	if ((PTOU(p)->u_cdir->v_flag & V_XATTRDIR) != 0 &&
	    strchr(resolvepn.pn_path, '/') == NULL) {
		mutex_exit(&p->p_lock);
		if (dir != NULL)
			VN_RELE(dir);
		error = EACCES;
		pn_free(&resolvepn);
		pn_free(&pn);
		VN_RELE(vp);
		goto out;
	}
	mutex_exit(&p->p_lock);

	bzero(exec_file, MAXCOMLEN+1);
	(void) strncpy(exec_file, pn.pn_path, MAXCOMLEN);
	bzero(&args, sizeof (args));
	args.pathname = resolvepn.pn_path;
	/* don't free resolvepn until we are done with args */
	pn_free(&pn);

	/*
	 * If we're running in a profile shell, then call pfexecd.
	 */
	if ((CR_FLAGS(p->p_cred) & PRIV_PFEXEC) != 0) {
		error = pfexec_call(p->p_cred, &resolvepn, &args.pfcred,
		    &args.scrubenv);

		/* Returning errno in case we're not allowed to execute. */
		if (error > 0) {
			if (dir != NULL)
				VN_RELE(dir);
			pn_free(&resolvepn);
			VN_RELE(vp);
			goto out;
		}

		/* Don't change the credentials when using old ptrace. */
		if (args.pfcred != NULL &&
		    (p->p_proc_flag & P_PR_PTRACE) != 0) {
			crfree(args.pfcred);
			args.pfcred = NULL;
			args.scrubenv = B_FALSE;
		}
	}

	/*
	 * Specific exec handlers, or policies determined via
	 * /etc/system may override the historical default.
	 */
	args.stk_prot = PROT_ZFOD;
	args.dat_prot = PROT_ZFOD;

	CPU_STATS_ADD_K(sys, sysexec, 1);
	DTRACE_PROC1(exec, char *, args.pathname);

	ua.fname = fname;
	ua.argp = argp;
	ua.envp = envp;

	/* If necessary, brand this process/lwp before we start the exec. */
	if (brandme) {
		void *brand_data = NULL;

		/*
		 * Process branding may fail if multiple LWPs are present and
		 * holdlwps() cannot complete successfully.
		 */
		error = brand_setbrand(p, B_TRUE);

		if (error == 0 && BROP(p)->b_lwpdata_alloc != NULL) {
			brand_data = BROP(p)->b_lwpdata_alloc(p);
			if (brand_data == NULL) {
				error = 1;
			}
		}

		if (error == 0) {
			mutex_enter(&p->p_lock);
			BROP(p)->b_initlwp(lwp, brand_data);
			mutex_exit(&p->p_lock);
		} else {
			VN_RELE(vp);
			if (dir != NULL) {
				VN_RELE(dir);
			}
			pn_free(&resolvepn);
			goto fail;
		}
	}

	if ((error = gexec(&vp, &ua, &args, NULL, 0, &execsz,
	    exec_file, p->p_cred, &brand_action)) != 0) {
		if (brandme) {
			BROP(p)->b_freelwp(lwp);
			brand_clearbrand(p, B_TRUE);
		}
		VN_RELE(vp);
		if (dir != NULL)
			VN_RELE(dir);
		pn_free(&resolvepn);
		goto fail;
	}

	/*
	 * Free floating point registers (sun4u only)
	 */
	ASSERT(lwp != NULL);
	lwp_freeregs(lwp, 1);

	/*
	 * Free thread and process context ops.
	 */
	if (curthread->t_ctx)
		freectx(curthread, 1);
	if (p->p_pctx)
		freepctx(p, 1);

	/*
	 * Remember file name for accounting; clear any cached DTrace predicate.
	 */
	up->u_acflag &= ~AFORK;
	bcopy(exec_file, up->u_comm, MAXCOMLEN+1);
	curthread->t_predcache = 0;

	/*
	 * Clear contract template state
	 */
	lwp_ctmpl_clear(lwp, B_TRUE);

	/*
	 * Save the directory in which we found the executable for expanding
	 * the %d token used in core file patterns.
	 */
	mutex_enter(&p->p_lock);
	tmpvp = p->p_execdir;
	p->p_execdir = dir;
	if (p->p_execdir != NULL)
		VN_HOLD(p->p_execdir);
	mutex_exit(&p->p_lock);

	if (tmpvp != NULL)
		VN_RELE(tmpvp);

	/*
	 * Reset stack state to the user stack, clear set of signals
	 * caught on the signal stack, and reset list of signals that
	 * restart system calls; the new program's environment should
	 * not be affected by detritus from the old program.  Any
	 * pending held signals remain held, so don't clear t_hold.
	 */
	mutex_enter(&p->p_lock);
	DTRACE_PROBE3(oldcontext__set, klwp_t *, lwp,
	    uintptr_t, lwp->lwp_oldcontext, uintptr_t, 0);
	lwp->lwp_oldcontext = 0;
	lwp->lwp_ustack = 0;
	lwp->lwp_old_stk_ctl = 0;
	sigemptyset(&up->u_signodefer);
	sigemptyset(&up->u_sigonstack);
	sigemptyset(&up->u_sigresethand);
	lwp->lwp_sigaltstack.ss_sp = 0;
	lwp->lwp_sigaltstack.ss_size = 0;
	lwp->lwp_sigaltstack.ss_flags = SS_DISABLE;

	/*
	 * Make saved resource limit == current resource limit.
	 */
	for (i = 0; i < RLIM_NLIMITS; i++) {
		/*CONSTCOND*/
		if (RLIM_SAVED(i)) {
			(void) rctl_rlimit_get(rctlproc_legacy[i], p,
			    &up->u_saved_rlimit[i]);
		}
	}

	/*
	 * If the action was to catch the signal, then the action
	 * must be reset to SIG_DFL.
	 */
	sigdefault(p);
	p->p_flag &= ~(SNOWAIT|SJCTL);
	p->p_flag |= (SEXECED|SMSACCT|SMSFORK);
	up->u_signal[SIGCLD - 1] = SIG_DFL;

	/*
	 * Delete the dot4 sigqueues/signotifies.
	 */
	sigqfree(p);

	mutex_exit(&p->p_lock);

	mutex_enter(&p->p_pflock);
	p->p_prof.pr_base = NULL;
	p->p_prof.pr_size = 0;
	p->p_prof.pr_off = 0;
	p->p_prof.pr_scale = 0;
	p->p_prof.pr_samples = 0;
	mutex_exit(&p->p_pflock);

	ASSERT(curthread->t_schedctl == NULL);

#if defined(__sparc)
	if (p->p_utraps != NULL)
		utrap_free(p);
#endif	/* __sparc */

	/*
	 * Close all close-on-exec files.
	 */
	close_exec(P_FINFO(p));
	TRACE_2(TR_FAC_PROC, TR_PROC_EXEC, "proc_exec:p %p up %p", p, up);

	/* Unbrand ourself if necessary. */
	if (PROC_IS_BRANDED(p) && (brand_action == EBA_NATIVE)) {
		BROP(p)->b_freelwp(lwp);
		brand_clearbrand(p, B_FALSE);
	}

	setregs(&args);

	/* Mark this as an executable vnode */
	mutex_enter(&vp->v_lock);
	vp->v_flag |= VVMEXEC;
	mutex_exit(&vp->v_lock);

	VN_RELE(vp);
	if (dir != NULL)
		VN_RELE(dir);
	pn_free(&resolvepn);

	/*
	 * Allocate a new lwp directory and lwpid hash table if necessary.
	 */
	if (curthread->t_tid != 1 || p->p_lwpdir_sz != 2) {
		lwpdir = kmem_zalloc(2 * sizeof (lwpdir_t), KM_SLEEP);
		lwpdir->ld_next = lwpdir + 1;
		tidhash = kmem_zalloc(2 * sizeof (tidhash_t), KM_SLEEP);
		if (p->p_lwpdir != NULL)
			lep = p->p_lwpdir[curthread->t_dslot].ld_entry;
		else
			lep = kmem_zalloc(sizeof (*lep), KM_SLEEP);
	}

	if (PROC_IS_BRANDED(p))
		BROP(p)->b_exec();

	mutex_enter(&p->p_lock);
	prbarrier(p);

	/*
	 * Reset lwp id to the default value of 1.
	 * This is a single-threaded process now
	 * and lwp #1 is lwp_wait()able by default.
	 * The t_unpark flag should not be inherited.
	 */
	ASSERT(p->p_lwpcnt == 1 && p->p_zombcnt == 0);
	curthread->t_tid = 1;
	kpreempt_disable();
	ASSERT(curthread->t_lpl != NULL);
	p->p_t1_lgrpid = curthread->t_lpl->lpl_lgrpid;
	kpreempt_enable();
	if (p->p_tr_lgrpid != LGRP_NONE && p->p_tr_lgrpid != p->p_t1_lgrpid) {
		lgrp_update_trthr_migrations(1);
	}
	curthread->t_unpark = 0;
	curthread->t_proc_flag |= TP_TWAIT;
	curthread->t_proc_flag &= ~TP_DAEMON;	/* daemons shouldn't exec */
	p->p_lwpdaemon = 0;			/* but oh well ... */
	p->p_lwpid = 1;

	/*
	 * Install the newly-allocated lwp directory and lwpid hash table
	 * and insert the current thread into the new hash table.
	 */
	if (lwpdir != NULL) {
		old_lwpdir = p->p_lwpdir;
		old_lwpdir_sz = p->p_lwpdir_sz;
		old_tidhash = p->p_tidhash;
		old_tidhash_sz = p->p_tidhash_sz;
		p->p_lwpdir = p->p_lwpfree = lwpdir;
		p->p_lwpdir_sz = 2;
		lep->le_thread = curthread;
		lep->le_lwpid = curthread->t_tid;
		lep->le_start = curthread->t_start;
		lwp_hash_in(p, lep, tidhash, 2, 0);
		p->p_tidhash = tidhash;
		p->p_tidhash_sz = 2;
	}
	ret_tidhash = p->p_ret_tidhash;
	p->p_ret_tidhash = NULL;

	/*
	 * Restore the saved signal mask and
	 * inform /proc that the exec() has finished.
	 */
	curthread->t_hold = savedmask;
	prexecend();
	mutex_exit(&p->p_lock);
	if (old_lwpdir) {
		kmem_free(old_lwpdir, old_lwpdir_sz * sizeof (lwpdir_t));
		kmem_free(old_tidhash, old_tidhash_sz * sizeof (tidhash_t));
	}
	while (ret_tidhash != NULL) {
		ret_tidhash_t *next = ret_tidhash->rth_next;
		kmem_free(ret_tidhash->rth_tidhash,
		    ret_tidhash->rth_tidhash_sz * sizeof (tidhash_t));
		kmem_free(ret_tidhash, sizeof (*ret_tidhash));
		ret_tidhash = next;
	}

	ASSERT(error == 0);
	DTRACE_PROC(exec__success);
	return (0);

fail:
	DTRACE_PROC1(exec__failure, int, error);
out:		/* error return */
	mutex_enter(&p->p_lock);
	curthread->t_hold = savedmask;
	prexecend();
	mutex_exit(&p->p_lock);
	ASSERT(error != 0);
	return (error);
}


/*
 * Perform generic exec duties and switchout to object-file specific
 * handler.
 */
int
gexec(
	struct vnode **vpp,
	struct execa *uap,
	struct uarg *args,
	struct intpdata *idatap,
	int level,
	size_t *execsz,
	caddr_t exec_file,
	struct cred *cred,
	int *brand_action)
{
	struct vnode *vp, *execvp = NULL;
	proc_t *pp = ttoproc(curthread);
	struct execsw *eswp;
	int error = 0;
	int suidflags = 0;
	ssize_t resid;
	uid_t uid, gid;
	struct vattr vattr;
	char magbuf[MAGIC_BYTES];
	int setid;
	cred_t *oldcred, *newcred = NULL;
	int privflags = 0;
	int setidfl;
	priv_set_t fset;
	secflagset_t old_secflags;

	secflags_copy(&old_secflags, &pp->p_secflags.psf_effective);

	/*
	 * If the SNOCD or SUGID flag is set, turn it off and remember the
	 * previous setting so we can restore it if we encounter an error.
	 */
	if (level == 0 && (pp->p_flag & PSUIDFLAGS)) {
		mutex_enter(&pp->p_lock);
		suidflags = pp->p_flag & PSUIDFLAGS;
		pp->p_flag &= ~PSUIDFLAGS;
		mutex_exit(&pp->p_lock);
	}

	if ((error = execpermissions(*vpp, &vattr, args)) != 0)
		goto bad_noclose;

	/* need to open vnode for stateful file systems */
	if ((error = VOP_OPEN(vpp, FREAD, CRED(), NULL)) != 0)
		goto bad_noclose;
	vp = *vpp;

	/*
	 * Note: to support binary compatibility with SunOS a.out
	 * executables, we read in the first four bytes, as the
	 * magic number is in bytes 2-3.
	 */
	if (error = vn_rdwr(UIO_READ, vp, magbuf, sizeof (magbuf),
	    (offset_t)0, UIO_SYSSPACE, 0, (rlim64_t)0, CRED(), &resid))
		goto bad;
	if (resid != 0)
		goto bad;

	if ((eswp = findexec_by_hdr(magbuf)) == NULL)
		goto bad;

	if (level == 0 &&
	    (privflags = execsetid(vp, &vattr, &uid, &gid, &fset,
	    args->pfcred == NULL ? cred : args->pfcred, args->pathname)) != 0) {

		/* Pfcred is a credential with a ref count of 1 */

		if (args->pfcred != NULL) {
			privflags |= PRIV_INCREASE|PRIV_RESET;
			newcred = cred = args->pfcred;
		} else {
			newcred = cred = crdup(cred);
		}

		/* If we can, drop the PA bit */
		if ((privflags & PRIV_RESET) != 0)
			priv_adjust_PA(cred);

		if (privflags & PRIV_SETID) {
			cred->cr_uid = uid;
			cred->cr_gid = gid;
			cred->cr_suid = uid;
			cred->cr_sgid = gid;
		}

		if (privflags & MAC_FLAGS) {
			if (!(CR_FLAGS(cred) & NET_MAC_AWARE_INHERIT))
				CR_FLAGS(cred) &= ~NET_MAC_AWARE;
			CR_FLAGS(cred) &= ~NET_MAC_AWARE_INHERIT;
		}

		/*
		 * Implement the privilege updates:
		 *
		 * Restrict with L:
		 *
		 *	I' = I & L
		 *
		 *	E' = P' = (I' + F) & A
		 *
		 * But if running under ptrace, we cap I and F with P.
		 */
		if ((privflags & (PRIV_RESET|PRIV_FORCED)) != 0) {
			if ((privflags & PRIV_INCREASE) != 0 &&
			    (pp->p_proc_flag & P_PR_PTRACE) != 0) {
				priv_intersect(&CR_OPPRIV(cred),
				    &CR_IPRIV(cred));
				priv_intersect(&CR_OPPRIV(cred), &fset);
			}
			priv_intersect(&CR_LPRIV(cred), &CR_IPRIV(cred));
			CR_EPRIV(cred) = CR_PPRIV(cred) = CR_IPRIV(cred);
			if (privflags & PRIV_FORCED) {
				priv_set_PA(cred);
				priv_union(&fset, &CR_EPRIV(cred));
				priv_union(&fset, &CR_PPRIV(cred));
			}
			priv_adjust_PA(cred);
		}
	} else if (level == 0 && args->pfcred != NULL) {
		newcred = cred = args->pfcred;
		privflags |= PRIV_INCREASE;
		/* pfcred is not forced to adhere to these settings */
		priv_intersect(&CR_LPRIV(cred), &CR_IPRIV(cred));
		CR_EPRIV(cred) = CR_PPRIV(cred) = CR_IPRIV(cred);
		priv_adjust_PA(cred);
	}

	/* The new image gets the inheritable secflags as its secflags */
	secflags_promote(pp);

	/* SunOS 4.x buy-back */
	if ((vp->v_vfsp->vfs_flag & VFS_NOSETUID) &&
	    (vattr.va_mode & (VSUID|VSGID))) {
		char path[MAXNAMELEN];
		refstr_t *mntpt = NULL;
		int ret = -1;

		bzero(path, sizeof (path));
		zone_hold(pp->p_zone);

		ret = vnodetopath(pp->p_zone->zone_rootvp, vp, path,
		    sizeof (path), cred);

		/* fallback to mountpoint if a path can't be found */
		if ((ret != 0) || (ret == 0 && path[0] == '\0'))
			mntpt = vfs_getmntpoint(vp->v_vfsp);

		if (mntpt == NULL)
			zcmn_err(pp->p_zone->zone_id, CE_NOTE,
			    "!uid %d: setuid execution not allowed, "
			    "file=%s", cred->cr_uid, path);
		else
			zcmn_err(pp->p_zone->zone_id, CE_NOTE,
			    "!uid %d: setuid execution not allowed, "
			    "fs=%s, file=%s", cred->cr_uid,
			    ZONE_PATH_TRANSLATE(refstr_value(mntpt),
			    pp->p_zone), exec_file);

		if (!INGLOBALZONE(pp)) {
			/* zone_rootpath always has trailing / */
			if (mntpt == NULL)
				cmn_err(CE_NOTE, "!zone: %s, uid: %d "
				    "setuid execution not allowed, file=%s%s",
				    pp->p_zone->zone_name, cred->cr_uid,
				    pp->p_zone->zone_rootpath, path + 1);
			else
				cmn_err(CE_NOTE, "!zone: %s, uid: %d "
				    "setuid execution not allowed, fs=%s, "
				    "file=%s", pp->p_zone->zone_name,
				    cred->cr_uid, refstr_value(mntpt),
				    exec_file);
		}

		if (mntpt != NULL)
			refstr_rele(mntpt);

		zone_rele(pp->p_zone);
	}

	/*
	 * execsetid() told us whether or not we had to change the
	 * credentials of the process.  In privflags, it told us
	 * whether we gained any privileges or executed a set-uid executable.
	 */
	setid = (privflags & (PRIV_SETUGID|PRIV_INCREASE|PRIV_FORCED));

	/*
	 * Use /etc/system variable to determine if the stack
	 * should be marked as executable by default.
	 */
	if ((noexec_user_stack != 0) ||
	    secflag_enabled(pp, PROC_SEC_NOEXECSTACK))
		args->stk_prot &= ~PROT_EXEC;

	args->execswp = eswp; /* Save execsw pointer in uarg for exec_func */
	args->ex_vp = vp;

	/*
	 * Traditionally, the setid flags told the sub processes whether
	 * the file just executed was set-uid or set-gid; this caused
	 * some confusion as the 'setid' flag did not match the SUGID
	 * process flag which is only set when the uids/gids do not match.
	 * A script set-gid/set-uid to the real uid/gid would start with
	 * /dev/fd/X but an executable would happily trust LD_LIBRARY_PATH.
	 * Now we flag those cases where the calling process cannot
	 * be trusted to influence the newly exec'ed process, either
	 * because it runs with more privileges or when the uids/gids
	 * do in fact not match.
	 * This also makes the runtime linker agree with the on exec
	 * values of SNOCD and SUGID.
	 */
	setidfl = 0;
	if (cred->cr_uid != cred->cr_ruid || (cred->cr_rgid != cred->cr_gid &&
	    !supgroupmember(cred->cr_gid, cred))) {
		setidfl |= EXECSETID_UGIDS;
	}
	if (setid & PRIV_SETUGID)
		setidfl |= EXECSETID_SETID;
	if (setid & PRIV_FORCED)
		setidfl |= EXECSETID_PRIVS;

	execvp = pp->p_exec;
	if (execvp)
		VN_HOLD(execvp);

	error = (*eswp->exec_func)(vp, uap, args, idatap, level, execsz,
	    setidfl, exec_file, cred, brand_action);
	rw_exit(eswp->exec_lock);
	if (error != 0) {
		if (execvp)
			VN_RELE(execvp);
		/*
		 * If this process's p_exec has been set to the vp of
		 * the executable by exec_func, we will return without
		 * calling VOP_CLOSE because proc_exit will close it
		 * on exit.
		 */
		if (pp->p_exec == vp)
			goto bad_noclose;
		else
			goto bad;
	}

	if (level == 0) {
		uid_t oruid;

		if (execvp != NULL) {
			/*
			 * Close the previous executable only if we are
			 * at level 0.
			 */
			(void) VOP_CLOSE(execvp, FREAD, 1, (offset_t)0,
			    cred, NULL);
		}

		mutex_enter(&pp->p_crlock);

		oruid = pp->p_cred->cr_ruid;

		if (newcred != NULL) {
			/*
			 * Free the old credentials, and set the new ones.
			 * Do this for both the process and the (single) thread.
			 */
			crfree(pp->p_cred);
			pp->p_cred = cred;	/* cred already held for proc */
			crhold(cred);		/* hold new cred for thread */
			/*
			 * DTrace accesses t_cred in probe context.  t_cred
			 * must always be either NULL, or point to a valid,
			 * allocated cred structure.
			 */
			oldcred = curthread->t_cred;
			curthread->t_cred = cred;
			crfree(oldcred);

			if (priv_basic_test >= 0 &&
			    !PRIV_ISASSERT(&CR_IPRIV(newcred),
			    priv_basic_test)) {
				pid_t pid = pp->p_pid;
				char *fn = PTOU(pp)->u_comm;

				cmn_err(CE_WARN, "%s[%d]: exec: basic_test "
				    "privilege removed from E/I", fn, pid);
			}
		}
		/*
		 * On emerging from a successful exec(), the saved
		 * uid and gid equal the effective uid and gid.
		 */
		cred->cr_suid = cred->cr_uid;
		cred->cr_sgid = cred->cr_gid;

		/*
		 * If the real and effective ids do not match, this
		 * is a setuid process that should not dump core.
		 * The group comparison is tricky; we prevent the code
		 * from flagging SNOCD when executing with an effective gid
		 * which is a supplementary group.
		 */
		if (cred->cr_ruid != cred->cr_uid ||
		    (cred->cr_rgid != cred->cr_gid &&
		    !supgroupmember(cred->cr_gid, cred)) ||
		    (privflags & PRIV_INCREASE) != 0)
			suidflags = PSUIDFLAGS;
		else
			suidflags = 0;

		mutex_exit(&pp->p_crlock);
		if (newcred != NULL && oruid != newcred->cr_ruid) {
			/* Note that the process remains in the same zone. */
			mutex_enter(&pidlock);
			upcount_dec(oruid, crgetzoneid(newcred));
			upcount_inc(newcred->cr_ruid, crgetzoneid(newcred));
			mutex_exit(&pidlock);
		}
		if (suidflags) {
			mutex_enter(&pp->p_lock);
			pp->p_flag |= suidflags;
			mutex_exit(&pp->p_lock);
		}
		if (setid && (pp->p_proc_flag & P_PR_PTRACE) == 0) {
			/*
			 * If process is traced via /proc, arrange to
			 * invalidate the associated /proc vnode.
			 */
			if (pp->p_plist || (pp->p_proc_flag & P_PR_TRACE))
				args->traceinval = 1;
		}

		/*
		 * If legacy ptrace is enabled, generate the SIGTRAP.
		 */
		if (pp->p_proc_flag & P_PR_PTRACE) {
			psignal(pp, SIGTRAP);
		}

		if (args->traceinval)
			prinvalidate(&pp->p_user);
	}
	if (execvp)
		VN_RELE(execvp);
	return (0);

bad:
	(void) VOP_CLOSE(vp, FREAD, 1, (offset_t)0, cred, NULL);

bad_noclose:
	if (newcred != NULL)
		crfree(newcred);
	if (error == 0)
		error = ENOEXEC;

	mutex_enter(&pp->p_lock);
	if (suidflags) {
		pp->p_flag |= suidflags;
	}
	/*
	 * Restore the effective secflags, to maintain the invariant they
	 * never change for a given process
	 */
	secflags_copy(&pp->p_secflags.psf_effective, &old_secflags);
	mutex_exit(&pp->p_lock);

	return (error);
}

extern char *execswnames[];

struct execsw *
allocate_execsw(char *name, char *magic, size_t magic_size)
{
	int i, j;
	char *ename;
	char *magicp;

	mutex_enter(&execsw_lock);
	for (i = 0; i < nexectype; i++) {
		if (execswnames[i] == NULL) {
			ename = kmem_alloc(strlen(name) + 1, KM_SLEEP);
			(void) strcpy(ename, name);
			execswnames[i] = ename;
			/*
			 * Set the magic number last so that we
			 * don't need to hold the execsw_lock in
			 * findexectype().
			 */
			magicp = kmem_alloc(magic_size, KM_SLEEP);
			for (j = 0; j < magic_size; j++)
				magicp[j] = magic[j];
			execsw[i].exec_magic = magicp;
			mutex_exit(&execsw_lock);
			return (&execsw[i]);
		}
	}
	mutex_exit(&execsw_lock);
	return (NULL);
}

/*
 * Find the exec switch table entry with the corresponding magic string.
 */
struct execsw *
findexecsw(char *magic)
{
	struct execsw *eswp;

	for (eswp = execsw; eswp < &execsw[nexectype]; eswp++) {
		ASSERT(eswp->exec_maglen <= MAGIC_BYTES);
		if (magic && eswp->exec_maglen != 0 &&
		    bcmp(magic, eswp->exec_magic, eswp->exec_maglen) == 0)
			return (eswp);
	}
	return (NULL);
}

/*
 * Find the execsw[] index for the given exec header string by looking for the
 * magic string at a specified offset and length for each kind of executable
 * file format until one matches.  If no execsw[] entry is found, try to
 * autoload a module for this magic string.
 */
struct execsw *
findexec_by_hdr(char *header)
{
	struct execsw *eswp;

	for (eswp = execsw; eswp < &execsw[nexectype]; eswp++) {
		ASSERT(eswp->exec_maglen <= MAGIC_BYTES);
		if (header && eswp->exec_maglen != 0 &&
		    bcmp(&header[eswp->exec_magoff], eswp->exec_magic,
		    eswp->exec_maglen) == 0) {
			if (hold_execsw(eswp) != 0)
				return (NULL);
			return (eswp);
		}
	}
	return (NULL);	/* couldn't find the type */
}

/*
 * Find the execsw[] index for the given magic string.  If no execsw[] entry
 * is found, try to autoload a module for this magic string.
 */
struct execsw *
findexec_by_magic(char *magic)
{
	struct execsw *eswp;

	for (eswp = execsw; eswp < &execsw[nexectype]; eswp++) {
		ASSERT(eswp->exec_maglen <= MAGIC_BYTES);
		if (magic && eswp->exec_maglen != 0 &&
		    bcmp(magic, eswp->exec_magic, eswp->exec_maglen) == 0) {
			if (hold_execsw(eswp) != 0)
				return (NULL);
			return (eswp);
		}
	}
	return (NULL);	/* couldn't find the type */
}

static int
hold_execsw(struct execsw *eswp)
{
	char *name;

	rw_enter(eswp->exec_lock, RW_READER);
	while (!LOADED_EXEC(eswp)) {
		rw_exit(eswp->exec_lock);
		name = execswnames[eswp-execsw];
		ASSERT(name);
		if (modload("exec", name) == -1)
			return (-1);
		rw_enter(eswp->exec_lock, RW_READER);
	}
	return (0);
}

static int
execsetid(struct vnode *vp, struct vattr *vattrp, uid_t *uidp, uid_t *gidp,
    priv_set_t *fset, cred_t *cr, const char *pathname)
{
	proc_t *pp = ttoproc(curthread);
	uid_t uid, gid;
	int privflags = 0;

	/*
	 * Remember credentials.
	 */
	uid = cr->cr_uid;
	gid = cr->cr_gid;

	/* Will try to reset the PRIV_AWARE bit later. */
	if ((CR_FLAGS(cr) & (PRIV_AWARE|PRIV_AWARE_INHERIT)) == PRIV_AWARE)
		privflags |= PRIV_RESET;

	if ((vp->v_vfsp->vfs_flag & VFS_NOSETUID) == 0) {
		/*
		 * If it's a set-uid root program we perform the
		 * forced privilege look-aside. This has three possible
		 * outcomes:
		 *	no look aside information -> treat as before
		 *	look aside in Limit set -> apply forced privs
		 *	look aside not in Limit set -> ignore set-uid root
		 *
		 * Ordinary set-uid root execution only allowed if the limit
		 * set holds all unsafe privileges.
		 */
		if (vattrp->va_mode & VSUID) {
			if (vattrp->va_uid == 0) {
				int res = get_forced_privs(cr, pathname, fset);

				switch (res) {
				case -1:
					if (priv_issubset(&priv_unsafe,
					    &CR_LPRIV(cr))) {
						uid = vattrp->va_uid;
						privflags |= PRIV_SETUGID;
					}
					break;
				case 0:
					privflags |= PRIV_FORCED|PRIV_INCREASE;
					break;
				default:
					break;
				}
			} else {
				uid = vattrp->va_uid;
				privflags |= PRIV_SETUGID;
			}
		}
		if (vattrp->va_mode & VSGID) {
			gid = vattrp->va_gid;
			privflags |= PRIV_SETUGID;
		}
	}

	/*
	 * Do we need to change our credential anyway?
	 * This is the case when E != I or P != I, as
	 * we need to do the assignments (with F empty and A full)
	 * Or when I is not a subset of L; in that case we need to
	 * enforce L.
	 *
	 *		I' = L & I
	 *
	 *		E' = P' = (I' + F) & A
	 * or
	 *		E' = P' = I'
	 */
	if (!priv_isequalset(&CR_EPRIV(cr), &CR_IPRIV(cr)) ||
	    !priv_issubset(&CR_IPRIV(cr), &CR_LPRIV(cr)) ||
	    !priv_isequalset(&CR_PPRIV(cr), &CR_IPRIV(cr)))
		privflags |= PRIV_RESET;

	/* Child has more privileges than parent */
	if (!priv_issubset(&CR_IPRIV(cr), &CR_PPRIV(cr)))
		privflags |= PRIV_INCREASE;

	/* If MAC-aware flag(s) are on, need to update cred to remove. */
	if ((CR_FLAGS(cr) & NET_MAC_AWARE) ||
	    (CR_FLAGS(cr) & NET_MAC_AWARE_INHERIT))
		privflags |= MAC_FLAGS;
	/*
	 * Set setuid/setgid protections if no ptrace() compatibility.
	 * For privileged processes, honor setuid/setgid even in
	 * the presence of ptrace() compatibility.
	 */
	if (((pp->p_proc_flag & P_PR_PTRACE) == 0 ||
	    PRIV_POLICY_ONLY(cr, PRIV_PROC_OWNER, (uid == 0))) &&
	    (cr->cr_uid != uid ||
	    cr->cr_gid != gid ||
	    cr->cr_suid != uid ||
	    cr->cr_sgid != gid)) {
		*uidp = uid;
		*gidp = gid;
		privflags |= PRIV_SETID;
	}
	return (privflags);
}

int
execpermissions(struct vnode *vp, struct vattr *vattrp, struct uarg *args)
{
	int error;
	proc_t *p = ttoproc(curthread);

	vattrp->va_mask = AT_MODE | AT_UID | AT_GID | AT_SIZE;
	if (error = VOP_GETATTR(vp, vattrp, ATTR_EXEC, p->p_cred, NULL))
		return (error);
	/*
	 * Check the access mode.
	 * If VPROC, ask /proc if the file is an object file.
	 */
	if ((error = VOP_ACCESS(vp, VEXEC, 0, p->p_cred, NULL)) != 0 ||
	    !(vp->v_type == VREG || (vp->v_type == VPROC && pr_isobject(vp))) ||
	    (vp->v_vfsp->vfs_flag & VFS_NOEXEC) != 0 ||
	    (vattrp->va_mode & (VEXEC|(VEXEC>>3)|(VEXEC>>6))) == 0) {
		if (error == 0)
			error = EACCES;
		return (error);
	}

	if ((p->p_plist || (p->p_proc_flag & (P_PR_PTRACE|P_PR_TRACE))) &&
	    (error = VOP_ACCESS(vp, VREAD, 0, p->p_cred, NULL))) {
		/*
		 * If process is under ptrace(3C) compatibility,
		 * fail the exec(2).
		 */
		if (p->p_proc_flag & P_PR_PTRACE)
			goto bad;
		/*
		 * Process is traced via /proc.
		 * Arrange to invalidate the /proc vnode.
		 */
		args->traceinval = 1;
	}
	return (0);
bad:
	if (error == 0)
		error = ENOEXEC;
	return (error);
}

/*
 * Map a section of an executable file into the user's
 * address space.
 */
int
execmap(struct vnode *vp, caddr_t addr, size_t len, size_t zfodlen,
    off_t offset, int prot, int page, uint_t szc)
{
	int error = 0;
	off_t oldoffset;
	caddr_t zfodbase, oldaddr;
	size_t end, oldlen;
	size_t zfoddiff;
	label_t ljb;
	proc_t *p = ttoproc(curthread);

	oldaddr = addr;
	addr = (caddr_t)((uintptr_t)addr & (uintptr_t)PAGEMASK);
	if (len) {
		oldlen = len;
		len += ((size_t)oldaddr - (size_t)addr);
		oldoffset = offset;
		offset = (off_t)((uintptr_t)offset & PAGEMASK);
		if (page) {
			spgcnt_t  prefltmem, availm, npages;
			int preread;
			uint_t mflag = MAP_PRIVATE | MAP_FIXED;

			if ((prot & (PROT_WRITE | PROT_EXEC)) == PROT_EXEC) {
				mflag |= MAP_TEXT;
			} else {
				mflag |= MAP_INITDATA;
			}

			if (valid_usr_range(addr, len, prot, p->p_as,
			    p->p_as->a_userlimit) != RANGE_OKAY) {
				error = ENOMEM;
				goto bad;
			}
			if (error = VOP_MAP(vp, (offset_t)offset,
			    p->p_as, &addr, len, prot, PROT_ALL,
			    mflag, CRED(), NULL))
				goto bad;

			/*
			 * If the segment can fit, then we prefault
			 * the entire segment in.  This is based on the
			 * model that says the best working set of a
			 * small program is all of its pages.
			 */
			npages = (spgcnt_t)btopr(len);
			prefltmem = freemem - desfree;
			preread =
			    (npages < prefltmem && len < PGTHRESH) ? 1 : 0;

			/*
			 * If we aren't prefaulting the segment,
			 * increment "deficit", if necessary to ensure
			 * that pages will become available when this
			 * process starts executing.
			 */
			availm = freemem - lotsfree;
			if (preread == 0 && npages > availm &&
			    deficit < lotsfree) {
				deficit += MIN((pgcnt_t)(npages - availm),
				    lotsfree - deficit);
			}

			if (preread) {
				TRACE_2(TR_FAC_PROC, TR_EXECMAP_PREREAD,
				    "execmap preread:freemem %d size %lu",
				    freemem, len);
				(void) as_fault(p->p_as->a_hat, p->p_as,
				    (caddr_t)addr, len, F_INVAL, S_READ);
			}
		} else {
			if (valid_usr_range(addr, len, prot, p->p_as,
			    p->p_as->a_userlimit) != RANGE_OKAY) {
				error = ENOMEM;
				goto bad;
			}

			if (error = as_map(p->p_as, addr, len,
			    segvn_create, zfod_argsp))
				goto bad;
			/*
			 * Read in the segment in one big chunk.
			 */
			if (error = vn_rdwr(UIO_READ, vp, (caddr_t)oldaddr,
			    oldlen, (offset_t)oldoffset, UIO_USERSPACE, 0,
			    (rlim64_t)0, CRED(), (ssize_t *)0))
				goto bad;
			/*
			 * Now set protections.
			 */
			if (prot != PROT_ZFOD) {
				(void) as_setprot(p->p_as, (caddr_t)addr,
				    len, prot);
			}
		}
	}

	if (zfodlen) {
		struct as *as = curproc->p_as;
		struct seg *seg;
		uint_t zprot = 0;

		end = (size_t)addr + len;
		zfodbase = (caddr_t)roundup(end, PAGESIZE);
		zfoddiff = (uintptr_t)zfodbase - end;
		if (zfoddiff) {
			/*
			 * Before we go to zero the remaining space on the last
			 * page, make sure we have write permission.
			 *
			 * Normal illumos binaries don't even hit the case
			 * where we have to change permission on the last page
			 * since their protection is typically either
			 *    PROT_USER | PROT_WRITE | PROT_READ
			 * or
			 *    PROT_ZFOD (same as PROT_ALL).
			 *
			 * We need to be careful how we zero-fill the last page
			 * if the segment protection does not include
			 * PROT_WRITE. Using as_setprot() can cause the VM
			 * segment code to call segvn_vpage(), which must
			 * allocate a page struct for each page in the segment.
			 * If we have a very large segment, this may fail, so
			 * we have to check for that, even though we ignore
			 * other return values from as_setprot.
			 */

			AS_LOCK_ENTER(as, RW_READER);
			seg = as_segat(curproc->p_as, (caddr_t)end);
			if (seg != NULL)
				SEGOP_GETPROT(seg, (caddr_t)end, zfoddiff - 1,
				    &zprot);
			AS_LOCK_EXIT(as);

			if (seg != NULL && (zprot & PROT_WRITE) == 0) {
				if (as_setprot(as, (caddr_t)end, zfoddiff - 1,
				    zprot | PROT_WRITE) == ENOMEM) {
					error = ENOMEM;
					goto bad;
				}
			}

			if (on_fault(&ljb)) {
				no_fault();
				if (seg != NULL && (zprot & PROT_WRITE) == 0)
					(void) as_setprot(as, (caddr_t)end,
					    zfoddiff - 1, zprot);
				error = EFAULT;
				goto bad;
			}
			uzero((void *)end, zfoddiff);
			no_fault();
			if (seg != NULL && (zprot & PROT_WRITE) == 0)
				(void) as_setprot(as, (caddr_t)end,
				    zfoddiff - 1, zprot);
		}
		if (zfodlen > zfoddiff) {
			struct segvn_crargs crargs =
			    SEGVN_ZFOD_ARGS(PROT_ZFOD, PROT_ALL);

			zfodlen -= zfoddiff;
			if (valid_usr_range(zfodbase, zfodlen, prot, p->p_as,
			    p->p_as->a_userlimit) != RANGE_OKAY) {
				error = ENOMEM;
				goto bad;
			}
			if (szc > 0) {
				/*
				 * ASSERT alignment because the mapelfexec()
				 * caller for the szc > 0 case extended zfod
				 * so it's end is pgsz aligned.
				 */
				size_t pgsz = page_get_pagesize(szc);
				ASSERT(IS_P2ALIGNED(zfodbase + zfodlen, pgsz));

				if (IS_P2ALIGNED(zfodbase, pgsz)) {
					crargs.szc = szc;
				} else {
					crargs.szc = AS_MAP_HEAP;
				}
			} else {
				crargs.szc = AS_MAP_NO_LPOOB;
			}
			if (error = as_map(p->p_as, (caddr_t)zfodbase,
			    zfodlen, segvn_create, &crargs))
				goto bad;
			if (prot != PROT_ZFOD) {
				(void) as_setprot(p->p_as, (caddr_t)zfodbase,
				    zfodlen, prot);
			}
		}
	}
	return (0);
bad:
	return (error);
}

void
setexecenv(struct execenv *ep)
{
	proc_t *p = ttoproc(curthread);
	klwp_t *lwp = ttolwp(curthread);
	struct vnode *vp;

	p->p_bssbase = ep->ex_bssbase;
	p->p_brkbase = ep->ex_brkbase;
	p->p_brksize = ep->ex_brksize;
	if (p->p_exec)
		VN_RELE(p->p_exec);	/* out with the old */
	vp = p->p_exec = ep->ex_vp;
	if (vp != NULL)
		VN_HOLD(vp);		/* in with the new */

	lwp->lwp_sigaltstack.ss_sp = 0;
	lwp->lwp_sigaltstack.ss_size = 0;
	lwp->lwp_sigaltstack.ss_flags = SS_DISABLE;
}

int
execopen(struct vnode **vpp, int *fdp)
{
	struct vnode *vp = *vpp;
	file_t *fp;
	int error = 0;
	int filemode = FREAD;

	VN_HOLD(vp);		/* open reference */
	if (error = falloc(NULL, filemode, &fp, fdp)) {
		VN_RELE(vp);
		*fdp = -1;	/* just in case falloc changed value */
		return (error);
	}
	if (error = VOP_OPEN(&vp, filemode, CRED(), NULL)) {
		VN_RELE(vp);
		setf(*fdp, NULL);
		unfalloc(fp);
		*fdp = -1;
		return (error);
	}
	*vpp = vp;		/* vnode should not have changed */
	fp->f_vnode = vp;
	mutex_exit(&fp->f_tlock);
	setf(*fdp, fp);
	return (0);
}

int
execclose(int fd)
{
	return (closeandsetf(fd, NULL));
}


/*
 * noexec stub function.
 */
/*ARGSUSED*/
int
noexec(
    struct vnode *vp,
    struct execa *uap,
    struct uarg *args,
    struct intpdata *idatap,
    int level,
    size_t *execsz,
    int setid,
    caddr_t exec_file,
    struct cred *cred)
{
	cmn_err(CE_WARN, "missing exec capability for %s", uap->fname);
	return (ENOEXEC);
}

/*
 * Support routines for building a user stack.
 *
 * execve(path, argv, envp) must construct a new stack with the specified
 * arguments and environment variables (see exec_args() for a description
 * of the user stack layout).  To do this, we copy the arguments and
 * environment variables from the old user address space into the kernel,
 * free the old as, create the new as, and copy our buffered information
 * to the new stack.  Our kernel buffer has the following structure:
 *
 *	+-----------------------+ <--- stk_base + stk_size
 *	| string offsets	|
 *	+-----------------------+ <--- stk_offp
 *	|			|
 *	| STK_AVAIL() space	|
 *	|			|
 *	+-----------------------+ <--- stk_strp
 *	| strings		|
 *	+-----------------------+ <--- stk_base
 *
 * When we add a string, we store the string's contents (including the null
 * terminator) at stk_strp, and we store the offset of the string relative to
 * stk_base at --stk_offp.  At strings are added, stk_strp increases and
 * stk_offp decreases.  The amount of space remaining, STK_AVAIL(), is just
 * the difference between these pointers.  If we run out of space, we return
 * an error and exec_args() starts all over again with a buffer twice as large.
 * When we're all done, the kernel buffer looks like this:
 *
 *	+-----------------------+ <--- stk_base + stk_size
 *	| argv[0] offset	|
 *	+-----------------------+
 *	| ...			|
 *	+-----------------------+
 *	| argv[argc-1] offset	|
 *	+-----------------------+
 *	| envp[0] offset	|
 *	+-----------------------+
 *	| ...			|
 *	+-----------------------+
 *	| envp[envc-1] offset	|
 *	+-----------------------+
 *	| AT_SUN_PLATFORM offset|
 *	+-----------------------+
 *	| AT_SUN_EXECNAME offset|
 *	+-----------------------+ <--- stk_offp
 *	|			|
 *	| STK_AVAIL() space	|
 *	|			|
 *	+-----------------------+ <--- stk_strp
 *	| AT_SUN_EXECNAME offset|
 *	+-----------------------+
 *	| AT_SUN_PLATFORM offset|
 *	+-----------------------+
 *	| envp[envc-1] string	|
 *	+-----------------------+
 *	| ...			|
 *	+-----------------------+
 *	| envp[0] string	|
 *	+-----------------------+
 *	| argv[argc-1] string	|
 *	+-----------------------+
 *	| ...			|
 *	+-----------------------+
 *	| argv[0] string	|
 *	+-----------------------+ <--- stk_base
 */

#define	STK_AVAIL(args)		((char *)(args)->stk_offp - (args)->stk_strp)

/*
 * Add a string to the stack.
 */
static int
stk_add(uarg_t *args, const char *sp, enum uio_seg segflg)
{
	int error;
	size_t len;

	if (STK_AVAIL(args) < sizeof (int))
		return (E2BIG);
	*--args->stk_offp = args->stk_strp - args->stk_base;

	if (segflg == UIO_USERSPACE) {
		error = copyinstr(sp, args->stk_strp, STK_AVAIL(args), &len);
		if (error != 0)
			return (error);
	} else {
		len = strlen(sp) + 1;
		if (len > STK_AVAIL(args))
			return (E2BIG);
		bcopy(sp, args->stk_strp, len);
	}

	args->stk_strp += len;

	return (0);
}

/*
 * Add a fixed size byte array to the stack (only from kernel space).
 */
static int
stk_byte_add(uarg_t *args, const uint8_t *sp, size_t len)
{
	if (STK_AVAIL(args) < sizeof (int))
		return (E2BIG);
	*--args->stk_offp = args->stk_strp - args->stk_base;

	if (len > STK_AVAIL(args))
		return (E2BIG);
	bcopy(sp, args->stk_strp, len);

	args->stk_strp += len;

	return (0);
}

static int
stk_getptr(uarg_t *args, char *src, char **dst)
{
	int error;

	if (args->from_model == DATAMODEL_NATIVE) {
		ulong_t ptr;
		error = fulword(src, &ptr);
		*dst = (caddr_t)ptr;
	} else {
		uint32_t ptr;
		error = fuword32(src, &ptr);
		*dst = (caddr_t)(uintptr_t)ptr;
	}
	return (error);
}

static int
stk_putptr(uarg_t *args, char *addr, char *value)
{
	if (args->to_model == DATAMODEL_NATIVE)
		return (sulword(addr, (ulong_t)value));
	else
		return (suword32(addr, (uint32_t)(uintptr_t)value));
}

static int
stk_copyin(execa_t *uap, uarg_t *args, intpdata_t *intp, void **auxvpp)
{
	char *sp;
	int argc, error;
	int argv_empty = 0;
	size_t ptrsize = args->from_ptrsize;
	size_t size, pad;
	char *argv = (char *)uap->argp;
	char *envp = (char *)uap->envp;
	uint8_t rdata[RANDOM_LEN];

	/*
	 * Copy interpreter's name and argument to argv[0] and argv[1].
	 * In the rare case that we have nested interpreters then those names
	 * and arguments are also copied to the subsequent slots in argv.
	 */
	if (intp != NULL && intp->intp_name[0] != NULL) {
		int i;

		for (i = 0; i < INTP_MAXDEPTH; i++) {
			if (intp->intp_name[i] == NULL)
				break;
			error = stk_add(args, intp->intp_name[i], UIO_SYSSPACE);
			if (error != 0)
				return (error);
			if (intp->intp_arg[i] != NULL) {
				error = stk_add(args, intp->intp_arg[i],
				    UIO_SYSSPACE);
				if (error != 0)
					return (error);
			}
		}

		if (args->fname != NULL)
			error = stk_add(args, args->fname, UIO_SYSSPACE);
		else
			error = stk_add(args, uap->fname, UIO_USERSPACE);
		if (error)
			return (error);

		/*
		 * Check for an empty argv[].
		 */
		if (stk_getptr(args, argv, &sp))
			return (EFAULT);
		if (sp == NULL)
			argv_empty = 1;

		argv += ptrsize;		/* ignore original argv[0] */
	}

	if (argv_empty == 0) {
		/*
		 * Add argv[] strings to the stack.
		 */
		for (;;) {
			if (stk_getptr(args, argv, &sp))
				return (EFAULT);
			if (sp == NULL)
				break;
			if ((error = stk_add(args, sp, UIO_USERSPACE)) != 0)
				return (error);
			argv += ptrsize;
		}
	}
	argc = (int *)(args->stk_base + args->stk_size) - args->stk_offp;
	args->argstrlen = args->stk_strp - args->stk_base;

	const char *envstr = args->stk_strp;

	/*
	 * Add environ[] strings to the stack.
	 */
	if (envp != NULL) {
		for (;;) {
			char *tmp = args->stk_strp;
			if (stk_getptr(args, envp, &sp))
				return (EFAULT);
			if (sp == NULL)
				break;
			if ((error = stk_add(args, sp, UIO_USERSPACE)) != 0)
				return (error);
			if (args->scrubenv && strncmp(tmp, "LD_", 3) == 0) {
				/* Undo the copied string */
				args->stk_strp = tmp;
				*(args->stk_offp++) = 0;
			}
			envp += ptrsize;
		}
	}

	args->envstrlen = args->stk_strp - envstr;
	args->na = (int *)(args->stk_base + args->stk_size) - args->stk_offp;
	args->ne = args->na - argc;

	/*
	 * Add AT_SUN_PLATFORM, AT_SUN_EXECNAME, AT_SUN_BRANDNAME,
	 * AT_SUN_BRAND_NROOT, and AT_SUN_EMULATOR strings, as well as AT_RANDOM
	 * array, to the stack.
	 */
	if (auxvpp != NULL && *auxvpp != NULL) {
		if ((error = stk_add(args, platform, UIO_SYSSPACE)) != 0)
			return (error);
		if ((error = stk_add(args, args->pathname, UIO_SYSSPACE)) != 0)
			return (error);
		if (args->brandname != NULL &&
		    (error = stk_add(args, args->brandname, UIO_SYSSPACE)) != 0)
			return (error);
		if (args->emulator != NULL &&
		    (error = stk_add(args, args->emulator, UIO_SYSSPACE)) != 0)
			return (error);

		/*
		 * For the AT_RANDOM aux vector we provide 16 bytes of random
		 * data.
		 */
		(void) random_get_pseudo_bytes(rdata, sizeof (rdata));

		if ((error = stk_byte_add(args, rdata, sizeof (rdata))) != 0)
			return (error);

		if (args->brand_nroot != NULL &&
		    (error = stk_add(args, args->brand_nroot,
		    UIO_SYSSPACE)) != 0)
			return (error);
	}

	/*
	 * Compute the size of the stack.  This includes all the pointers,
	 * the space reserved for the aux vector, and all the strings.
	 * The total number of pointers is args->na (which is argc + envc)
	 * plus 4 more: (1) a pointer's worth of space for argc; (2) the NULL
	 * after the last argument (i.e. argv[argc]); (3) the NULL after the
	 * last environment variable (i.e. envp[envc]); and (4) the NULL after
	 * all the strings, at the very top of the stack.
	 */
	size = (args->na + 4) * args->to_ptrsize + args->auxsize +
	    (args->stk_strp - args->stk_base);

	/*
	 * Pad the string section with zeroes to align the stack size.
	 */
	pad = P2NPHASE(size, args->stk_align);

	if (STK_AVAIL(args) < pad)
		return (E2BIG);

	args->usrstack_size = size + pad;

	while (pad-- != 0)
		*args->stk_strp++ = 0;

	args->nc = args->stk_strp - args->stk_base;

	return (0);
}

static int
stk_copyout(uarg_t *args, char *usrstack, void **auxvpp, user_t *up)
{
	size_t ptrsize = args->to_ptrsize;
	ssize_t pslen;
	char *kstrp = args->stk_base;
	char *ustrp = usrstack - args->nc - ptrsize;
	char *usp = usrstack - args->usrstack_size;
	int *offp = (int *)(args->stk_base + args->stk_size);
	int envc = args->ne;
	int argc = args->na - envc;
	int i;

	/*
	 * Record argc for /proc.
	 */
	up->u_argc = argc;

	/*
	 * Put argc on the stack.  Note that even though it's an int,
	 * it always consumes ptrsize bytes (for alignment).
	 */
	if (stk_putptr(args, usp, (char *)(uintptr_t)argc))
		return (-1);
	usp += ptrsize;

	/*
	 * For the benefit of /proc, record the user address of the argv[] array
	 * as well as the start of the argv string space (argv[0]).
	 */
	up->u_argv = (uintptr_t)usp;
	up->u_argvstrs = (uintptr_t)(&ustrp[*(offp - 1)]);
	up->u_argvstrsize = args->argstrlen;

	/*
	 * Put the argv[] pointers on the stack, including a NULL terminator.
	 */
	for (i = 0; i < argc; i++, usp += ptrsize)
		if (stk_putptr(args, usp, &ustrp[*--offp]))
			return (-1);
	usp += ptrsize;

	/*
	 * Copy arguments to u_psargs.
	 */
	pslen = MIN(args->argstrlen, PSARGSZ) - 1;
	for (i = 0; i < pslen; i++)
		up->u_psargs[i] = (kstrp[i] == '\0' ? ' ' : kstrp[i]);
	while (i < PSARGSZ)
		up->u_psargs[i++] = '\0';

	/*
	 * For the benefit of /proc, record the user address of the envp[] array
	 * as well as the start of the envp string space (envp[0]).
	 */
	up->u_envp = (uintptr_t)usp;
	up->u_envstrs = (uintptr_t)(&ustrp[*(offp - 1)]);
	up->u_envstrsize = args->envstrlen;

	/*
	 * Put the envp[] pointers on the stack, including a NULL terminator.
	 */
	for (i = 0; i < envc; i++, usp += ptrsize)
		if (stk_putptr(args, usp, &ustrp[*--offp]))
			return (-1);
	usp += ptrsize;

	/*
	 * Remember where the stack ends, which is also where auxv begins.
	 */
	args->stackend = usp;

	/*
	 * Put all the argv[], envp[], and auxv strings on the stack.
	 */
	if (copyout(args->stk_base, ustrp, args->nc))
		return (-1);

	/*
	 * Fill in the aux vector now that we know the user stack addresses
	 * for the AT_SUN_PLATFORM, AT_SUN_EXECNAME, AT_SUN_BRANDNAME and
	 * AT_SUN_EMULATOR strings, as well as the AT_RANDOM array.
	 */
	if (auxvpp != NULL && *auxvpp != NULL) {
		if (args->to_model == DATAMODEL_NATIVE) {
			auxv_t **a = (auxv_t **)auxvpp;
			ADDAUX(*a, AT_SUN_PLATFORM, (long)&ustrp[*--offp])
			ADDAUX(*a, AT_SUN_EXECNAME, (long)&ustrp[*--offp])
			if (args->brandname != NULL)
				ADDAUX(*a,
				    AT_SUN_BRANDNAME, (long)&ustrp[*--offp])
			if (args->emulator != NULL)
				ADDAUX(*a,
				    AT_SUN_EMULATOR, (long)&ustrp[*--offp])
			ADDAUX(*a, AT_RANDOM, (long)&ustrp[*--offp])
			if (args->brand_nroot != NULL) {
				ADDAUX(*a,
				    AT_SUN_BRAND_NROOT, (long)&ustrp[*--offp])
			}
		} else {
			auxv32_t **a = (auxv32_t **)auxvpp;
			ADDAUX(*a,
			    AT_SUN_PLATFORM, (int)(uintptr_t)&ustrp[*--offp])
			ADDAUX(*a,
			    AT_SUN_EXECNAME, (int)(uintptr_t)&ustrp[*--offp])
			if (args->brandname != NULL)
				ADDAUX(*a, AT_SUN_BRANDNAME,
				    (int)(uintptr_t)&ustrp[*--offp])
			if (args->emulator != NULL)
				ADDAUX(*a, AT_SUN_EMULATOR,
				    (int)(uintptr_t)&ustrp[*--offp])
			ADDAUX(*a, AT_RANDOM, (int)(uintptr_t)&ustrp[*--offp])
			if (args->brand_nroot != NULL) {
				ADDAUX(*a, AT_SUN_BRAND_NROOT,
				    (int)(uintptr_t)&ustrp[*--offp])
			}
		}
	}

	return (0);
}

/*
 * Though the actual stack base is constant, slew the %sp by a random aligned
 * amount in [0,aslr_max_stack_skew).  Mostly, this makes life slightly more
 * complicated for buffer overflows hoping to overwrite the return address.
 *
 * On some platforms this helps avoid cache thrashing when identical processes
 * simultaneously share caches that don't provide enough associativity
 * (e.g. sun4v systems). In this case stack slewing makes the same hot stack
 * variables in different processes live in different cache sets increasing
 * effective associativity.
 */
size_t
exec_get_spslew(void)
{
#ifdef sun4v
	static uint_t sp_color_stride = 16;
	static uint_t sp_color_mask = 0x1f;
	static uint_t sp_current_color = (uint_t)-1;
#endif
	size_t off;

	ASSERT(ISP2(aslr_max_stack_skew));

	if ((aslr_max_stack_skew == 0) ||
	    !secflag_enabled(curproc, PROC_SEC_ASLR)) {
#ifdef sun4v
		uint_t spcolor = atomic_inc_32_nv(&sp_current_color);
		return ((size_t)((spcolor & sp_color_mask) *
		    SA(sp_color_stride)));
#else
		return (0);
#endif
	}

	(void) random_get_pseudo_bytes((uint8_t *)&off, sizeof (off));
	return (SA(P2PHASE(off, aslr_max_stack_skew)));
}

/*
 * Initialize a new user stack with the specified arguments and environment.
 * The initial user stack layout is as follows:
 *
 *	User Stack
 *	+---------------+
 *	|		|
 *	| stack guard	|
 *	| (64-bit only)	|
 *	|		|
 *	+...............+ <--- stack limit (base - curproc->p_stk_ctl)
 *	.		.
 *	.		.
 *	.		.
 *	+---------------+ <--- curproc->p_usrstack
 *	|		|
 *	| slew		|
 *	|		|
 *	+---------------+
 *	| NULL		|
 *	+---------------+
 *	|		|
 *	| auxv strings	|
 *	|		|
 *	+---------------+
 *	|		|
 *	| envp strings	|
 *	|		|
 *	+---------------+
 *	|		|
 *	| argv strings	|
 *	|		|
 *	+---------------+ <--- ustrp
 *	|		|
 *	| aux vector	|
 *	|		|
 *	+---------------+ <--- auxv
 *	| NULL		|
 *	+---------------+
 *	| envp[envc-1]	|
 *	+---------------+
 *	| ...		|
 *	+---------------+
 *	| envp[0]	|
 *	+---------------+ <--- envp[]
 *	| NULL		|
 *	+---------------+
 *	| argv[argc-1]	|
 *	+---------------+
 *	| ...		|
 *	+---------------+
 *	| argv[0]	|
 *	+---------------+ <--- argv[]
 *	| argc		|
 *	+---------------+ <--- stack base
 *
 * In 64-bit processes, a stack guard segment is allocated at the address
 * immediately below where the stack limit ends.  This protects new library
 * mappings (such as the linker) from being placed in relatively dangerous
 * proximity to the stack.
 */
int
exec_args(execa_t *uap, uarg_t *args, intpdata_t *intp, void **auxvpp)
{
	size_t size;
	int error;
	proc_t *p = ttoproc(curthread);
	user_t *up = PTOU(p);
	char *usrstack;
	rctl_entity_p_t e;
	struct as *as;
	extern int use_stk_lpg;
	size_t sp_slew;
#if defined(_LP64)
	const size_t sg_sz = (stack_guard_seg_sz & PAGEMASK);
#endif /* defined(_LP64) */

	args->from_model = p->p_model;
	if (p->p_model == DATAMODEL_NATIVE) {
		args->from_ptrsize = sizeof (long);
	} else {
		args->from_ptrsize = sizeof (int32_t);
	}

	if (args->to_model == DATAMODEL_NATIVE) {
		args->to_ptrsize = sizeof (long);
		args->ncargs = NCARGS;
		args->stk_align = STACK_ALIGN;
		if (args->addr32)
			usrstack = (char *)USRSTACK64_32;
		else
			usrstack = (char *)USRSTACK;
	} else {
		args->to_ptrsize = sizeof (int32_t);
		args->ncargs = NCARGS32;
		args->stk_align = STACK_ALIGN32;
		usrstack = (char *)USRSTACK32;
	}

	if (args->maxstack != 0 && (uintptr_t)usrstack > args->maxstack)
		usrstack = (char *)args->maxstack;

	ASSERT(P2PHASE((uintptr_t)usrstack, args->stk_align) == 0);

#if defined(__sparc)
	/*
	 * Make sure user register windows are empty before
	 * attempting to make a new stack.
	 */
	(void) flush_user_windows_to_stack(NULL);
#endif

	for (size = PAGESIZE; ; size *= 2) {
		args->stk_size = size;
		args->stk_base = kmem_alloc(size, KM_SLEEP);
		args->stk_strp = args->stk_base;
		args->stk_offp = (int *)(args->stk_base + size);
		error = stk_copyin(uap, args, intp, auxvpp);
		if (error == 0)
			break;
		kmem_free(args->stk_base, size);
		if (error != E2BIG && error != ENAMETOOLONG)
			return (error);
		if (size >= args->ncargs)
			return (E2BIG);
	}

	size = args->usrstack_size;

	ASSERT(error == 0);
	ASSERT(P2PHASE(size, args->stk_align) == 0);
	ASSERT((ssize_t)STK_AVAIL(args) >= 0);

	if (size > args->ncargs) {
		kmem_free(args->stk_base, args->stk_size);
		return (E2BIG);
	}

	/*
	 * Leave only the current lwp and force the other lwps to exit.
	 * If another lwp beat us to the punch by calling exit(), bail out.
	 */
	if ((error = exitlwps(0)) != 0) {
		kmem_free(args->stk_base, args->stk_size);
		return (error);
	}

	/*
	 * Revoke any doors created by the process.
	 */
	if (p->p_door_list)
		door_exit();

	/*
	 * Release schedctl data structures.
	 */
	if (p->p_pagep)
		schedctl_proc_cleanup();

	/*
	 * Clean up any DTrace helpers for the process.
	 */
	if (p->p_dtrace_helpers != NULL) {
		ASSERT(dtrace_helpers_cleanup != NULL);
		(*dtrace_helpers_cleanup)(p);
	}

	mutex_enter(&p->p_lock);
	/*
	 * Cleanup the DTrace provider associated with this process.
	 */
	if (p->p_dtrace_probes) {
		ASSERT(dtrace_fasttrap_exec_ptr != NULL);
		dtrace_fasttrap_exec_ptr(p);
	}
	mutex_exit(&p->p_lock);

	/*
	 * discard the lwpchan cache.
	 */
	if (p->p_lcp != NULL)
		lwpchan_destroy_cache(1);

	/*
	 * Delete the POSIX timers.
	 */
	if (p->p_itimer != NULL)
		timer_exit();

	/*
	 * Delete the ITIMER_REALPROF interval timer.
	 * The other ITIMER_* interval timers are specified
	 * to be inherited across exec().
	 */
	delete_itimer_realprof();

	if (AU_AUDITING())
		audit_exec(args->stk_base, args->stk_base + args->argstrlen,
		    args->na - args->ne, args->ne, args->pfcred);

	/*
	 * Ensure that we don't change resource associations while we
	 * change address spaces.
	 */
	mutex_enter(&p->p_lock);
	pool_barrier_enter();
	mutex_exit(&p->p_lock);

	/*
	 * Destroy the old address space and create a new one.
	 * From here on, any errors are fatal to the exec()ing process.
	 * On error we return -1, which means the caller must SIGKILL
	 * the process.
	 */
	relvm();

	mutex_enter(&p->p_lock);
	pool_barrier_exit();
	mutex_exit(&p->p_lock);

	up->u_execsw = args->execswp;

	p->p_brkbase = NULL;
	p->p_brksize = 0;
	p->p_brkpageszc = 0;
	p->p_stksize = 0;
	p->p_stkpageszc = 0;
	p->p_stkg_start = 0;
	p->p_stkg_end = 0;
	p->p_model = args->to_model;
	p->p_usrstack = usrstack;
	p->p_stkprot = args->stk_prot;
	p->p_datprot = args->dat_prot;

	/*
	 * Reset resource controls such that all controls are again active as
	 * well as appropriate to the potentially new address model for the
	 * process.
	 */
	e.rcep_p.proc = p;
	e.rcep_t = RCENTITY_PROCESS;
	rctl_set_reset(p->p_rctls, p, &e);

	/* Too early to call map_pgsz for the heap */
	if (use_stk_lpg) {
		p->p_stkpageszc = page_szc(map_pgsz(MAPPGSZ_STK, p, 0, 0, 0));
	}

	mutex_enter(&p->p_lock);
	p->p_flag |= SAUTOLPG;	/* kernel controls page sizes */
	mutex_exit(&p->p_lock);

	sp_slew = exec_get_spslew();
	ASSERT(P2PHASE(sp_slew, args->stk_align) == 0);
	/* Be certain we don't underflow */
	VERIFY((curproc->p_usrstack - (size + sp_slew)) < curproc->p_usrstack);
	exec_set_sp(size + sp_slew);

	as = as_alloc();
	p->p_as = as;
	as->a_proc = p;
	if (p->p_model == DATAMODEL_ILP32 || args->addr32)
		as->a_userlimit = (caddr_t)USERLIMIT32;
	(void) hat_setup(as->a_hat, HAT_ALLOC);
	hat_join_srd(as->a_hat, args->ex_vp);

	/* Write out the contents of the new stack. */
	error = stk_copyout(args, usrstack - sp_slew, auxvpp, up);
	kmem_free(args->stk_base, args->stk_size);

#if defined(_LP64)
	/* Add stack guard segment (if needed) after successful copyout */
	if (error == 0 && p->p_model == DATAMODEL_LP64 && sg_sz != 0) {
		seghole_crargs_t sca;
		caddr_t addr_end = (caddr_t)(((uintptr_t)usrstack -
		    p->p_stk_ctl) & PAGEMASK);
		caddr_t addr_start = addr_end - sg_sz;

		DTRACE_PROBE4(stack__guard__chk, proc_t *, p,
		    caddr_t, addr_start, caddr_t, addr_end, size_t, sg_sz);

		if (addr_end >= usrstack || addr_start >= addr_end ||
		    valid_usr_range(addr_start, sg_sz, PROT_NONE, as,
		    as->a_userlimit) != RANGE_OKAY) {
			return (E2BIG);
		}

		/* Create un-mappable area in AS with seg_hole */
		sca.name = "stack_guard";
		error = as_map(as, addr_start, sg_sz, seghole_create, &sca);
		if (error == 0) {
			p->p_stkg_start = (uintptr_t)addr_start;
			p->p_stkg_end = (uintptr_t)addr_start + sg_sz;
		}
	}
#endif /* defined(_LP64) */

	return (error);
}
