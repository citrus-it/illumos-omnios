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
 * This file contains the spawn(2) implementation. A private system call
 * used to implement posix_spawn(3C).
 *
 * The parent marshals everything that the child needs into flat structures
 * which are copied into the kernel here and fully validated before use.
 * cfork() then creates a child process with a single kernel-resident LWP.
 * That LWP runs spawn_main(), which applies the attributes and file
 * actions, execs the target and reports the result back through
 * kspawn_param_t handshake before the parent's spawn(2) call returns.
 */

#include <sys/class.h>
#include <sys/cmn_err.h>
#include <sys/cred.h>
#include <sys/ddi.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/exec.h>
#include <sys/fcntl.h>
#include <sys/file.h>
#include <sys/fork.h>
#include <sys/kmem.h>
#include <sys/param.h>
#include <sys/pgrpsys.h>
#include <sys/proc.h>
#include <sys/sdt.h>
#include <sys/signal.h>
#include <sys/spawn.h>
#include <sys/spawn_impl.h>
#include <sys/sunddi.h>
#include <sys/syscall.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/types.h>

#include <c2/audit.h>

extern int64_t cfork(int, int, kspawn_param_t *, int);

extern int setpgrp(int, int, int);
extern int setuid(uid_t);
extern int setgid(gid_t);
extern int fchdir(int);
extern int kchdir(const char *);
extern int64_t lwp_sigmask(int, uint_t, uint_t, uint_t, uint_t);
extern int setthreadprio(pcprio_t *, kthread_t *);

static const spawn_attr_t *
spawn_param_attr(const spawn_param_t *sp)
{
	if (sp == NULL || sp->sp_attr_len == 0)
		return (NULL);
	return ((const spawn_attr_t *)&sp->sp_data[sp->sp_attr_off]);
}

/*
 * Signal completion to the parent which is waiting in spawn(2).
 */
void
spawn_complete(kspawn_param_t *ksp, int err)
{
	curproc->p_spawn_ksp = NULL;

	mutex_enter(&ksp->ksp_lock);
	ksp->ksp_error = err;
	ksp->ksp_complete = true;
	cv_signal(&ksp->ksp_cv);
	mutex_exit(&ksp->ksp_lock);
}

/*
 * Apply the spawn attributes in the child.
 */
static int
spawn_attrs_apply(const spawn_param_t *sp)
{
	const spawn_attr_t *spa = spawn_param_attr(sp);
	klwp_t *lwp = ttolwp(curthread);
	proc_t *p = curproc;
	int sig;

	if (spa == NULL)
		return (0);

	if (spa->sa_psflags & POSIX_SPAWN_SETSIGMASK) {
		(void) lwp_sigmask(SIG_SETMASK,
		    spa->sa_sigmask.__sigbits[0],
		    spa->sa_sigmask.__sigbits[1],
		    spa->sa_sigmask.__sigbits[2],
		    spa->sa_sigmask.__sigbits[3]);
	}

	if (spa->sa_psflags & POSIX_SPAWN_SETSIGIGN_NP) {
		k_sigset_t kset;

		sigutok(&spa->sa_sigignore, &kset);
		for (sig = 1; sig < NSIG; sig++) {
			if (sigismember(&kset, sig) &&
			    !sigismember(&cantmask, sig)) {
				mutex_enter(&p->p_lock);
				setsigact(sig, SIG_IGN, &nullsmask, 0);
				mutex_exit(&p->p_lock);
			}
		}
	}

	if (spa->sa_psflags & POSIX_SPAWN_SETSIGDEF) {
		k_sigset_t kset;

		sigutok(&spa->sa_sigdefault, &kset);
		for (sig = 1; sig < NSIG; sig++) {
			if (sigismember(&kset, sig) &&
			    !sigismember(&cantmask, sig)) {
				mutex_enter(&p->p_lock);
				setsigact(sig, SIG_DFL, &nullsmask, 0);
				mutex_exit(&p->p_lock);
			}
		}
	}

	if (spa->sa_psflags & POSIX_SPAWN_RESETIDS) {
		lwp->lwp_errno = 0;
		if (setgid(crgetrgid(CRED())) != 0 ||
		    setuid(crgetruid(CRED())) != 0) {
			return (lwp->lwp_errno);
		}
	}

	if (spa->sa_psflags & POSIX_SPAWN_SETSID) {
		/*
		 * setpgrp() reports failure through lwp_errno. Its return
		 * value with the SETSID subcommand is a session ID.
		 */
		lwp->lwp_errno = 0;
		(void) setpgrp(PGRPSYS_SETSID, 0, 0);
		if (lwp->lwp_errno != 0)
			return (lwp->lwp_errno);
	}

	if (spa->sa_psflags & POSIX_SPAWN_SETPGROUP) {
		lwp->lwp_errno = 0;
		if (setpgrp(PGRPSYS_SETPGID, 0, spa->sa_pgroup) != 0)
			return (lwp->lwp_errno);
	}

	/*
	 * The scheduling attributes are applied last, once any RESETIDS,
	 * SETSID and SETPGROUP changes are in place.
	 */
	if ((spa->sa_psflags &
	    (POSIX_SPAWN_SETSCHEDULER | POSIX_SPAWN_SETSCHEDPARAM)) != 0) {
		kspawn_sched_t ks;
		int err = 0;

		bcopy(&sp->sp_data[sp->sp_sched_off], &ks, sizeof (ks));

		switch (ks.ksched_op) {
		case KSCHED_PARMS:
			err = parmsin(&ks.ksched_parms, NULL);
			break;
		case KSCHED_PRIO:
			/* The same check that doprio() applies */
			if (ks.ksched_prio.pc_cid >= loaded_classes ||
			    ks.ksched_prio.pc_cid < 1) {
				err = EINVAL;
			}
			break;
		}

		if (err != 0)
			return (err);

		/*
		 * Entering a new scheduling class allocates the class data
		 * with KM_NOSLEEP, since the locks are held. Retry on ENOMEM.
		 */
		do {
			mutex_enter(&pidlock);
			mutex_enter(&p->p_lock);
			if (ks.ksched_op == KSCHED_PARMS)
				err = parmsset(&ks.ksched_parms, curthread);
			else
				err = setthreadprio(&ks.ksched_prio, curthread);
			mutex_exit(&p->p_lock);
			mutex_exit(&pidlock);
		} while (err == ENOMEM);

		if (err != 0)
			return (err);
	}

	return (0);
}

static int
spawn_factions_apply(const kspawn_param_t *ksp)
{
	const spawn_param_t *sp = ksp->ksp_param;
	klwp_t *lwp = ttolwp(curthread);
	uint32_t off;

	if (sp == NULL || sp->sp_fattr_cnt == 0)
		return (0);

	off = sp->sp_fattr_off;
	for (uint32_t i = 0; i < sp->sp_fattr_cnt; i++) {
		const kfile_attr_t *kfa =
		    (const kfile_attr_t *)&sp->sp_data[off];
		int err = 0;
		int fd;

		switch (kfa->kfa_type) {
		case FA_OPEN:
			fd = kopenat(AT_FDCWD, (char *)kfa->kfa_path,
			    kfa->kfa_oflag, kfa->kfa_mode,
			    ksp->ksp_parent_model);
			if (fd < 0) {
				err = lwp->lwp_errno;
			} else if (fd != kfa->kfa_filedes) {
				err = fdup2(fd, kfa->kfa_filedes);
				(void) closeandsetf(fd, NULL);
			}
			break;
		case FA_CLOSE:
			err = closeandsetf(kfa->kfa_filedes, NULL);
			/* An already-closed descriptor is not an error */
			if (err == EBADF)
				err = 0;
			break;
		case FA_DUP2:
			err = fdup2(kfa->kfa_filedes, kfa->kfa_newfiledes);
			break;
		case FA_CLOSEFROM:
			closefrom_all(kfa->kfa_filedes);
			break;
		case FA_CHDIR:
			err = kchdir((const char *)kfa->kfa_path);
			break;
		case FA_FCHDIR:
			lwp->lwp_errno = 0;
			if (fchdir(kfa->kfa_filedes) != 0)
				err = lwp->lwp_errno;
			break;
		}

		if (err != 0)
			return (err);

		off += kfa->kfa_len;
	}

	return (0);
}

/*
 * Build a NULL-terminated vector of pointers to the packed, NUL-terminated
 * strings in the spawn args data area.
 */
static char **
spawn_vector(const spawn_args_t *sa, uint32_t off, uint32_t cnt)
{
	char **vec = kmem_alloc(((size_t)cnt + 1) * sizeof (char *), KM_SLEEP);

	for (uint32_t i = 0; i < cnt; i++) {
		vec[i] = (char *)&sa->sa_data[off];
		off += strlen(vec[i]) + 1;
	}
	vec[cnt] = NULL;

	return (vec);
}

/*
 * Build the path name for the next attempt in a PATH search by joining the
 * leading component of the search path with the program name. Returns the
 * remainder of the search path or NULL if we're done. Sets *fits to false
 * if the joined name would not fit in buf, in which case buf is not filled
 * and the caller must skip this candidate rather than exec a truncated path.
 */
static const char *
spawn_execat(const char *path, const char *name, char *buf, size_t bufl,
    bool *fits)
{
	const char *sep = strchr(path, ':');
	size_t dirlen = (sep == NULL) ? strlen(path) : (size_t)(sep - path);
	size_t namelen = strlen(name);
	size_t need = dirlen + namelen + 1;
	char *s = buf;

	if (dirlen > 0)
		need++;		/* for the '/' separator */

	*fits = (need <= bufl);
	if (*fits) {
		bcopy(path, s, dirlen);
		s += dirlen;
		if (dirlen > 0)
			*s++ = '/';
		bcopy(name, s, namelen);
		s[namelen] = '\0';
	}

	return (sep != NULL ? sep + 1 : NULL);
}

/*
 * Exec the target program. For posix_spawn() this is a single attempt at the
 * given path. For posix_spawnp(), libc supplies the search path and shell in
 * the spawn parameters, and we need to walk the path.
 *
 * On success the process is running the new image and this returns 0.
 */
static int
spawn_exec(kspawn_param_t *ksp)
{
	const spawn_args_t *sa = ksp->ksp_args;
	const spawn_param_t *sp = ksp->ksp_param;
	const char *pathstr = NULL, *shell = NULL, *cp;
	/*
	 * Allow for the terminating NUL and for prepending "./" below, should
	 * the resulting filename begin with a '-'.
	 */
	const size_t pathl = MAXPATHLEN + 1 + sizeof ("./");
	char **argv, **envp;
	char *path = NULL;
	int err = ENOENT;
	int saved_err = 0;

	argv = spawn_vector(sa, sa->sa_arg_off, sa->sa_arg_cnt);
	envp = spawn_vector(sa, sa->sa_env_off, sa->sa_env_cnt);

	if (sp != NULL && sp->sp_path_len != 0) {
		pathstr = (const char *)&sp->sp_data[sp->sp_path_off];
		if (sp->sp_shell_len != 0)
			shell = (const char *)&sp->sp_data[sp->sp_shell_off];
	}

	if (pathstr == NULL) {
		/* posix_spawn() - the simple case with the given path */
		err = exec_common(ksp->ksp_path, (const char **)argv,
		    (const char **)envp, NULL, EBA_NONE, UIO_SYSSPACE);
		goto out;
	}

	path = kmem_alloc(pathl, KM_SLEEP);

	cp = pathstr;
	do {
		bool fits;

		cp = spawn_execat(cp, ksp->ksp_path, path, MAXPATHLEN + 1,
		    &fits);
		if (!fits) {
			/*
			 * This candidate does not fit in the buffer. Skip it
			 * rather than exec a truncated path, remembering the
			 * error in case the search finds nothing better.
			 */
			err = ENAMETOOLONG;
			if (saved_err == 0)
				saved_err = ENAMETOOLONG;
			continue;
		}

		/*
		 * If the resulting filename begins with a '-', prepend "./"
		 * so that the shell cannot interpret it as an option.
		 */
		if (*path == '-') {
			memmove(path + 2, path, strlen(path) + 1);
			path[0] = '.';
			path[1] = '/';
		}

		err = exec_common(path, (const char **)argv,
		    (const char **)envp, NULL, EBA_NONE, UIO_SYSSPACE);
		if (err == 0) {
			/*
			 * Record the path to which the search resolved. It is
			 * reported back to the parent for auditing.
			 */
			(void) strlcpy(ksp->ksp_path, path,
			    sizeof (ksp->ksp_path));
			goto out;
		}

		/*
		 * Remember the most meaningful error seen during the search
		 * (matching execvp). A candidate that existed but could not be
		 * executed (EACCES) outranks both a later "not found" and an
		 * over-long candidate that we had to skip.
		 */
		if (err == EACCES)
			saved_err = EACCES;

		if (err == ENOEXEC) {
			/*
			 * The file exists and is executable but is not in a
			 * recognised format. Execute it as a shell script and
			 * stop the search here.
			 */
			size_t nargs = (size_t)sa->sa_arg_cnt + 3;
			char **newargs;
			uint32_t i;

			if (shell == NULL)
				goto out;

			/*
			 * The zeroed allocation guarantees a terminating
			 * NULL entry even if the caller supplied an empty
			 * argument vector.
			 */
			newargs = kmem_zalloc(nargs * sizeof (char *),
			    KM_SLEEP);
			/*
			 * argv[0] is always the literal "sh", regardless of
			 * the shell path supplied by libc, matching the
			 * behaviour of execvp().
			 */
			newargs[0] = "sh";
			newargs[1] = path;
			for (i = 1; i < sa->sa_arg_cnt; i++)
				newargs[i + 1] = argv[i];

			err = exec_common(shell, (const char **)newargs,
			    (const char **)envp, NULL, EBA_NONE,
			    UIO_SYSSPACE);
			if (err == 0) {
				(void) strlcpy(ksp->ksp_path, shell,
				    sizeof (ksp->ksp_path));
			}

			kmem_free(newargs, nargs * sizeof (char *));
			goto out;
		}
	} while (cp != NULL);

	/*
	 * The search is exhausted without an exec. Prefer the most
	 * meaningful error we saw over whichever happened to be last.
	 */
	if (saved_err != 0)
		err = saved_err;

out:
	if (path != NULL)
		kmem_free(path, pathl);
	kmem_free(argv, ((size_t)sa->sa_arg_cnt + 1) * sizeof (char *));
	kmem_free(envp, ((size_t)sa->sa_env_cnt + 1) * sizeof (char *));

	return (err);
}

/*
 * The entry point for the single LWP of a spawned child, which begins life
 * here in the kernel. Apply the spawn attributes and file actions, exec the
 * target program, report the outcome to the waiting parent and, if
 * everything's ok, enter userland via lwp_rtt_initial().
 */
void
spawn_main(void *arg)
{
	kspawn_param_t *ksp = arg;
	klwp_t *lwp = ttolwp(curthread);
	proc_t *p = curproc;
	const spawn_attr_t *spa = spawn_param_attr(ksp->ksp_param);
	bool execfail = false;
	int err;

	ASSERT(p->p_spawn_ksp == ksp);

	/*
	 * Make this LWP look as if it is completing an execve() system
	 * call. /proc and post_syscall() rely on this.
	 */
	bzero(lwp->lwp_arg, sizeof (lwp->lwp_arg));
	lwp->lwp_ap = lwp->lwp_arg;
	curthread->t_sysnum = SYS_execve;
	curthread->t_post_sys = 1;

	/*
	 * The spawn-error probes identify the spawn parameters, the stage
	 * at which the spawn failed and the error. A failed spawn child
	 * usually evaporates without ever running in userland, and its
	 * image is still the parent's, so these probes are the observable
	 * record of what went wrong inside it.
	 */
	if ((err = spawn_attrs_apply(ksp->ksp_param)) != 0) {
		DTRACE_PROBE3(spawn__error, kspawn_param_t *, ksp,
		    char *, "attributes", int, err);
	} else if ((err = spawn_factions_apply(ksp)) != 0) {
		DTRACE_PROBE3(spawn__error, kspawn_param_t *, ksp,
		    char *, "file-actions", int, err);
	} else if ((err = spawn_exec(ksp)) != 0) {
		DTRACE_PROBE3(spawn__error, kspawn_param_t *, ksp,
		    char *, "exec", int, err);
		execfail = true;
	}

	if (err == 0) {
		/*
		 * The exec succeeded. Release the parent and enter userland
		 * in the new program.
		 */
		spawn_complete(ksp, 0);
		lwp_rtt_initial();
		/* NOTREACHED */
	}

	if (execfail && spa != NULL &&
	    (spa->sa_psflags & POSIX_SPAWN_NOEXECERR_NP) != 0) {
		/*
		 * POSIX_SPAWN_NOEXECERR_NP: an exec failure is not reported
		 * to the parent. It is told that the spawn succeeded, and
		 * the child exits with status 127 for the parent to observe
		 * via wait().
		 */
		spawn_complete(ksp, 0);
		exit(CLD_EXITED, SPAWN_NOEXECERR_STATUS);
		/* NOTREACHED */
	}

	/*
	 * The error is reported to the parent and the parent never learns
	 * this child's pid - it disappears without a trace and without
	 * raising SIGCHLD.
	 */
	mutex_enter(&pidlock);
	p->p_pidflag |= CLDEVAPORATE;
	mutex_exit(&pidlock);

	spawn_complete(ksp, err);
	exit(CLD_EXITED, 0);
	/* NOTREACHED */
}

static bool
spawn_region_ok(const spawn_param_t *sp, uint32_t off, uint32_t len)
{
	return (off <= sp->sp_datalen && len <= sp->sp_datalen - off);
}

/*
 * As spawn_region_ok(), additionally requiring that the region holds a
 * NUL-terminated string.
 */
static bool
spawn_str_ok(const spawn_param_t *sp, uint32_t off, uint32_t len)
{
	return (len != 0 && spawn_region_ok(sp, off, len) &&
	    sp->sp_data[off + len - 1] == '\0');
}

static int
spawn_param_verify(const spawn_param_t *sp, uint32_t spsize)
{
	int schedflags = 0;

	if (sp->sp_size != spsize ||
	    sp->sp_datalen != spsize - offsetof(spawn_param_t, sp_data)) {
		return (EINVAL);
	}

	if (sp->sp_attr_len != 0) {
		const spawn_attr_t *spa;

		if (sp->sp_attr_len != sizeof (spawn_attr_t) ||
		    !IS_P2ALIGNED(sp->sp_attr_off, sizeof (uint32_t)) ||
		    !spawn_region_ok(sp, sp->sp_attr_off, sp->sp_attr_len)) {
			return (EINVAL);
		}

		spa = spawn_param_attr(sp);

		if ((spa->sa_psflags & ~ALL_POSIX_SPAWN_FLAGS) != 0)
			return (EINVAL);
		if (spa->sa_pgroup < 0)
			return (EINVAL);

		schedflags = spa->sa_psflags &
		    (POSIX_SPAWN_SETSCHEDULER | POSIX_SPAWN_SETSCHEDPARAM);
	}

	/*
	 * The resolved scheduling attributes are required when one of the
	 * scheduling flags is set, and must not be present otherwise.
	 */
	if (schedflags != 0) {
		const kspawn_sched_t *ks;

		if (sp->sp_sched_len != sizeof (kspawn_sched_t) ||
		    !IS_P2ALIGNED(sp->sp_sched_off, sizeof (uint32_t)) ||
		    !spawn_region_ok(sp, sp->sp_sched_off, sp->sp_sched_len)) {
			return (EINVAL);
		}

		ks = (const kspawn_sched_t *)&sp->sp_data[sp->sp_sched_off];

		switch (ks->ksched_op) {
		case KSCHED_PARMS:
			break;
		case KSCHED_PRIO:
			if (ks->ksched_prio.pc_op != PC_SETPRIO)
				return (EINVAL);
			break;
		default:
			return (EINVAL);
		}
	} else if (sp->sp_sched_len != 0) {
		return (EINVAL);
	}

	if (sp->sp_fattr_cnt != 0) {
		uint32_t off = sp->sp_fattr_off;

		if (!IS_P2ALIGNED(off, sizeof (uint32_t)))
			return (EINVAL);

		for (uint32_t i = 0; i < sp->sp_fattr_cnt; i++) {
			const kfile_attr_t *kfa;
			uint64_t reclen;

			if (!spawn_region_ok(sp, off, sizeof (kfile_attr_t)))
				return (EINVAL);

			kfa = (const kfile_attr_t *)&sp->sp_data[off];

			/*
			 * Each record is padded so that the next one remains
			 * 32-bit aligned.
			 */
			reclen = P2ROUNDUP((uint64_t)sizeof (kfile_attr_t) +
			    kfa->kfa_pathsize, sizeof (uint32_t));
			if (kfa->kfa_len != reclen ||
			    !spawn_region_ok(sp, off, kfa->kfa_len)) {
				return (EINVAL);
			}

			switch (kfa->kfa_type) {
			case FA_OPEN:
				if (kfa->kfa_filedes < 0)
					return (EINVAL);
				/* FALLTHROUGH */
			case FA_CHDIR:
				if (kfa->kfa_pathsize == 0 ||
				    kfa->kfa_path[kfa->kfa_pathsize - 1] !=
				    '\0') {
					return (EINVAL);
				}
				break;
			case FA_CLOSE:
			case FA_CLOSEFROM:
			case FA_FCHDIR:
				if (kfa->kfa_pathsize != 0 ||
				    kfa->kfa_filedes < 0) {
					return (EINVAL);
				}
				break;
			case FA_DUP2:
				if (kfa->kfa_pathsize != 0 ||
				    kfa->kfa_filedes < 0 ||
				    kfa->kfa_newfiledes < 0) {
					return (EINVAL);
				}
				break;
			default:
				return (EINVAL);
			}

			off += kfa->kfa_len;
		}
	}

	if (sp->sp_shell_len != 0 &&
	    !spawn_str_ok(sp, sp->sp_shell_off, sp->sp_shell_len)) {
		return (EINVAL);
	}

	if (sp->sp_path_len != 0 &&
	    !spawn_str_ok(sp, sp->sp_path_off, sp->sp_path_len)) {
		return (EINVAL);
	}

	return (0);
}

static int
spawn_args_verify(const spawn_args_t *sa, uint32_t sasize)
{
	uint32_t off;

	if (sa->sa_size != sasize ||
	    sa->sa_datalen != sasize - offsetof(spawn_args_t, sa_data)) {
		return (EINVAL);
	}

	if (sa->sa_env_off > sa->sa_datalen ||
	    sa->sa_arg_off > sa->sa_env_off) {
		return (EINVAL);
	}

	off = sa->sa_arg_off;
	for (uint32_t i = 0; i < sa->sa_arg_cnt; i++) {
		const char *s = (const char *)&sa->sa_data[off];
		const char *e = memchr(s, '\0', sa->sa_env_off - off);

		if (e == NULL)
			return (EINVAL);
		off += (uint32_t)(e - s) + 1;
	}
	if (off != sa->sa_env_off)
		return (EINVAL);

	for (uint32_t i = 0; i < sa->sa_env_cnt; i++) {
		const char *s = (const char *)&sa->sa_data[off];
		const char *e = memchr(s, '\0', sa->sa_datalen - off);

		if (e == NULL)
			return (EINVAL);
		off += (uint32_t)(e - s) + 1;
	}
	if (off != sa->sa_datalen)
		return (EINVAL);

	return (0);
}

/*
 * Pre-scan the file actions to determine which of the parent's file
 * descriptors the child actually needs, so that flist_spawn() can limit its
 * copy of the descriptor table:
 *
 *  - ksp_closefrom is the lowest closefrom() bound. Descriptors at or above
 *    it would be closed by the closefrom action anyway, so they need not be
 *    copied unless an action consumes them as a source.
 *  - ksp_reffds lists the descriptors that actions consume as sources -
 *    dup2() and fchdir() - which must be copied even if they carry
 *    FD_CLOEXEC or sit above the closefrom bound.
 *
 * This is purely an optimisation. Copying too much is harmless since
 * the file actions and close_exec() still run in the child.
 */
static void
spawn_prescan(const spawn_param_t *sp, kspawn_param_t *ksp)
{
	const kfile_attr_t *kfa;
	uint32_t off, i, n;

	ksp->ksp_closefrom = INT_MAX;

	if (sp == NULL || sp->sp_fattr_cnt == 0)
		return;

	n = 0;
	off = sp->sp_fattr_off;
	for (i = 0; i < sp->sp_fattr_cnt; i++) {
		kfa = (const kfile_attr_t *)&sp->sp_data[off];
		switch (kfa->kfa_type) {
		case FA_CLOSEFROM:
			ksp->ksp_closefrom =
			    MIN(ksp->ksp_closefrom, kfa->kfa_filedes);
			break;
		case FA_DUP2:
		case FA_FCHDIR:
			n++;
			break;
		default:
			break;
		}
		off += kfa->kfa_len;
	}

	if (n == 0)
		return;

	/* We saw at least one dup2 or chdir. Build a list of source fds */

	ksp->ksp_reffds = kmem_alloc(n * sizeof (int), KM_SLEEP);
	ksp->ksp_nreffds = n;

	n = 0;
	off = sp->sp_fattr_off;
	for (i = 0; i < sp->sp_fattr_cnt; i++) {
		kfa = (const kfile_attr_t *)&sp->sp_data[off];
		if (kfa->kfa_type == FA_DUP2 || kfa->kfa_type == FA_FCHDIR) {
			VERIFY3U(n, <, ksp->ksp_nreffds);
			ksp->ksp_reffds[n++] = kfa->kfa_filedes;
		}
		off += kfa->kfa_len;
	}
}

static int
spawn_forkflags(const spawn_param_t *sp)
{
	const spawn_attr_t *spa = spawn_param_attr(sp);
	int flags = 0;

	if (spa != NULL) {
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

	if (spsize > NCARGS64 || sasize > NCARGS64)
		return ((int64_t)set_errno(E2BIG));

	if (spsize > 0) {
		if (spsize < sizeof (*sp))
			return ((int64_t)set_errno(EINVAL));

		sp = kmem_alloc(spsize, KM_SLEEP);
		if (copyin(sparam, sp, spsize) != 0) {
			err = EFAULT;
			goto out;
		}
		if ((err = spawn_param_verify(sp, spsize)) != 0)
			goto out;
	}

	sa = kmem_alloc(sasize, KM_SLEEP);
	if (copyin(sargs, sa, sasize) != 0) {
		err = EFAULT;
		goto out;
	}
	if ((err = spawn_args_verify(sa, sasize)) != 0)
		goto out;

	ksp = kmem_zalloc(sizeof (*ksp), KM_SLEEP);

	err = copyinstr(path, ksp->ksp_path, sizeof (ksp->ksp_path), NULL);
	if (err != 0)
		goto out;

	ksp->ksp_param = sp;
	ksp->ksp_args = sa;
	ksp->ksp_parent_model = get_udatamodel();
	spawn_prescan(sp, ksp);

	mutex_init(&ksp->ksp_lock, NULL, MUTEX_DEFAULT, NULL);
	cv_init(&ksp->ksp_cv, NULL, CV_DEFAULT, NULL);

	/*
	 * If cfork() succeeds, wait for the child to apply the various spawn
	 * attributes and attempt the exec. Every child exit path signals
	 * completion, including abnormal termination, so the wait is bounded
	 * by the child's lifetime.
	 *
	 * This logic is taken from vfwait(). We wait interruptibly with
	 * cv_wait_sig() for its jobcontrol and /proc side effects. The
	 * spawning thread can then be stopped or examined and does not block a
	 * concurrent holdlwps() from another of the parent's threads while it
	 * waits. Once a signal is pending we must switch to an uninterruptible
	 * cv_wait(), since we cannot return and free ksp while the child may
	 * still reference it, and cv_wait_sig() would otherwise spin returning
	 * immediately.
	 */
	mutex_enter(&ksp->ksp_lock);
	ret = cfork(0, 0, ksp, spawn_forkflags(sp));
	if (ttolwp(curthread)->lwp_errno == 0) {
		bool signalled = false;

		while (!ksp->ksp_complete) {
			if (signalled) {
				cv_wait(&ksp->ksp_cv, &ksp->ksp_lock);
			} else {
				signalled = !cv_wait_sig(&ksp->ksp_cv,
				    &ksp->ksp_lock);
			}
		}
		if (ksp->ksp_error != 0) {
			err = ksp->ksp_error;
			ret = -1;
		}
	}
	mutex_exit(&ksp->ksp_lock);

	mutex_destroy(&ksp->ksp_lock);
	cv_destroy(&ksp->ksp_cv);

	/*
	 * Record the details of the spawn while the marshalled data is still
	 * to hand. On success, ksp_path holds the path that the child
	 * actually exec'd, which for posix_spawnp() may differ from the
	 * caller-supplied name.
	 */
	if (AU_AUDITING()) {
		audit_spawn(ksp->ksp_path,
		    (const char *)&sa->sa_data[sa->sa_arg_off],
		    (const char *)&sa->sa_data[sa->sa_env_off],
		    (ssize_t)sa->sa_arg_cnt, (ssize_t)sa->sa_env_cnt);
	}

out:
	if (sp != NULL)
		kmem_free(sp, spsize);
	if (sa != NULL)
		kmem_free(sa, sasize);
	if (ksp != NULL) {
		if (ksp->ksp_reffds != NULL) {
			kmem_free(ksp->ksp_reffds,
			    ksp->ksp_nreffds * sizeof (int));
		}
		kmem_free(ksp, sizeof (*ksp));
	}

	if (err != 0)
		return ((int64_t)set_errno(err));
	return (ret);
}
