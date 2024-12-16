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
 *
 * Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved.
 * Copyright 2015, Joyent, Inc.
 * Copyright 2026 Oxide Computer Company
 * Copyright 2026 OmniOS Community Edition (OmniOSce) Association.
 */

#define	_POSIX_PTHREAD_SEMANTICS 1

#include <sys/ccompile.h>
#include <sys/param.h>
#include <sys/fork.h>
#include <sys/klpd.h>
#include <sys/syscall.h>
#include <sys/systeminfo.h>
#include <sys/wait.h>

#include <alloca.h>
#include <atomic.h>
#include <deflt.h>
#include <door.h>
#include <err.h>
#include <errno.h>
#include <grp.h>
#include <priv.h>
#include <pthread.h>
#include <pwd.h>
#include <regex.h>
#include <secdb.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <ucred.h>
#include <unistd.h>

#include <auth_attr.h>
#include <exec_attr.h>
#include <prof_attr.h>
#include <user_attr.h>

#include "isapath.h"

#define	PFAUTH_PATH	"/usr/lib/pfauth"

/*
 * The size of the fixed door server thread pool, and the stack size for
 * each thread; see create_door_thread() below.
 */
#define	DOOR_THREADS		16
#define	DOOR_THREAD_STACKSIZE	(256 * 1024)

/*
 * The maximum number of authentication conversations that may be in
 * progress at any one time. Each one occupies a door server thread and a
 * helper process for up to the helper's timeout, so this bounds what a
 * user can pin by repeatedly executing commands from an authenticated
 * profile and abandoning the prompts. It must be comfortably below
 * DOOR_THREADS, so that pending authentication conversations can never
 * starve the fixed thread pool and block ordinary requests.
 */
#define	PFEXEC_MAX_AUTHS	(DOOR_THREADS / 2)

static int doorfd = -1;

static size_t repsz, setsz;

static uid_t get_uid(const char *, boolean_t *, char *);
static gid_t get_gid(const char *, boolean_t *, char *);
static priv_set_t *get_privset(const char *, boolean_t *, char *);

static int
register_pfexec(int fd)
{
	int ret = syscall(SYS_privsys, PRIVSYS_PFEXEC_REG, fd);

	return (ret);
}

static void
unregister_pfexec(int sig __unused)
{
	if (doorfd != -1)
		(void) syscall(SYS_privsys, PRIVSYS_PFEXEC_UNREG, doorfd);
	_exit(0);
}

static uid_t
get_uid(const char *v, boolean_t *ok, char *path)
{
	struct passwd *pwd, pwdm;
	const char *errstr;
	char buf[1024];
	uid_t uid;

	if (getpwnam_r(v, &pwdm, buf, sizeof (buf), &pwd) == 0 && pwd != NULL)
		return (pwd->pw_uid);

	uid = (uid_t)strtonum(v, 0, MAXUID, &errstr);
	if (errstr == NULL)
		return (uid);

	*ok = B_FALSE;
	syslog(LOG_ERR, "%s: %s: unknown username\n", path, v);
	return ((uid_t)-1);
}

static gid_t
get_gid(const char *v, boolean_t *ok, char *path)
{
	struct group *grp, grpm;
	const char *errstr;
	size_t bufsz = 1024;
	gid_t gid = (gid_t)-1;
	bool found = false;

	/*
	 * The group entry, including the member pointer array which is
	 * constructed inside the caller's buffer, can be arbitrarily
	 * large, so retry with a bigger buffer as necessary.
	 */
	for (;;) {
		char *buf;
		int ret;

		if ((buf = malloc(bufsz)) == NULL)
			break;
		ret = getgrnam_r(v, &grpm, buf, bufsz, &grp);
		if (ret == 0 && grp != NULL) {
			found = true;
			gid = grp->gr_gid;
		}
		free(buf);
		if (ret != ERANGE || (bufsz <<= 1) > (1 << 20))
			break;
	}

	if (found)
		return (gid);

	gid = (gid_t)strtonum(v, 0, MAXUID, &errstr);
	if (errstr == NULL)
		return (gid);

	*ok = B_FALSE;
	syslog(LOG_ERR, "%s: %s: unknown groupname\n", path, v);
	return ((gid_t)-1);
}

static priv_set_t *
get_privset(const char *s, boolean_t *ok, char *path)
{
	priv_set_t *res;

	if ((res = priv_str_to_set(s, ",", NULL)) == NULL) {
		syslog(LOG_ERR, "%s: %s: bad privilege set\n", path, s);
		if (ok != NULL)
			*ok = B_FALSE;
	}
	return (res);
}

static int
ggp_callback(const char *prof __unused, kva_t *attr, void *ctxt __unused,
    void *vres)
{
	priv_set_t *res = vres;
	char *privs;

	if (attr == NULL)
		return (0);

	/* get privs from this profile */
	privs = kva_match(attr, PROFATTR_PRIVS_KW);
	if (privs != NULL) {
		priv_set_t *tmp = priv_str_to_set(privs, ",", NULL);
		if (tmp != NULL) {
			priv_union(tmp, res);
			priv_freeset(tmp);
		}
	}

	return (0);
}

/*
 * This routine exists on failure and returns NULL if no granted privileges
 * are set.
 */
static priv_set_t *
get_granted_privs(uid_t uid, int flags)
{
	priv_set_t *res;
	struct passwd *pwd, pwdm;
	char buf[1024];

	if (getpwuid_r(uid, &pwdm, buf, sizeof (buf), &pwd) != 0 || pwd == NULL)
		return (NULL);

	res = priv_allocset();
	if (res == NULL)
		return (NULL);

	priv_emptyset(res);

	(void) _enum_profs(pwd->pw_name, ggp_callback, NULL, res, flags);

	return (res);
}

/*
 * Answer a request which is being rejected: an unknown version, or an
 * unauthorised caller. The reply is a well-formed denial appropriate to
 * the call type rather than a zero-length answer; older kernels did not
 * reliably detect a zero-length reply and could interpret uninitialised
 * stack as the answer, and a denial is the correct answer regardless.
 */
static void
reject_call(const pfexec_arg_t *pap, size_t asz)
{
	if (asz < sizeof (pfexec_arg_t)) {
		(void) door_return(NULL, 0, NULL, 0);
		return;
	}

	switch (pap->pfa_call) {
	case PFEXEC_EXEC_ATTRS: {
		pfexec_reply_t *res = alloca(repsz);

		(void) memset(res, 0, repsz);
		res->pfr_vers = pap->pfa_vers;
		res->pfr_ruid = PFEXEC_NOTSET;
		res->pfr_euid = PFEXEC_NOTSET;
		res->pfr_rgid = PFEXEC_NOTSET;
		res->pfr_egid = PFEXEC_NOTSET;
		res->pfr_allowed = B_FALSE;

		(void) door_return((char *)res, repsz - 2 * setsz, NULL, 0);
		break;
	}
	case PFEXEC_FORCED_PRIVS: {
		void *res = alloca(setsz);

		/* An empty set signifies no forced privileges. */
		priv_emptyset(res);
		(void) door_return(res, setsz, NULL, 0);
		break;
	}
	case PFEXEC_USER_PRIVS:
	case PFEXEC_AUTH_DROP: {
		uint32_t res = 0;

		(void) door_return((char *)&res, sizeof (res), NULL, 0);
		break;
	}
	default:
		(void) door_return(NULL, 0, NULL, 0);
		break;
	}
}

/*
 * The pfexec door only has one legitimate client, the kernel. Upcalls
 * from the kernel are attributed to pid 0 in the client ucred, and no
 * process can present that pid in a door call, so this cannot be forged.
 */
static bool
caller_is_kernel(void)
{
	ucred_t *uc = NULL;
	bool res;

	if (door_ucred(&uc) != 0) {
		syslog(LOG_DEBUG, "door_ucred failed: %m");
		return (false);
	}
	res = ucred_getpid(uc) == 0;
	ucred_free(uc);

	return (res);
}

static void
close_descs(const door_desc_t *dp, uint_t ndesc)
{
	for (uint_t i = 0; i < ndesc; i++, dp++) {
		if ((dp->d_attributes & DOOR_DESCRIPTOR) != 0)
			(void) close(dp->d_data.d_desc.d_descriptor);
	}
}

/*
 * Run the pfauth helper with the terminal descriptor on the standard file
 * descriptors and wait for it to complete. Returns true if the helper ran
 * and exited successfully.
 */
static bool
run_pfauth(int ttyfd, const char *arg1, const char *arg2, const char *arg3)
{
	pid_t pid;
	int stat = -1;

	pid = forkx(FORK_WAITPID | FORK_NOSIGCHLD);
	switch (pid) {
	case -1:
		syslog(LOG_ERR, "could not fork authentication helper: %m");
		return (false);
	case 0:
		if (dup2(ttyfd, STDIN_FILENO) == -1 ||
		    dup2(ttyfd, STDOUT_FILENO) == -1 ||
		    dup2(ttyfd, STDERR_FILENO) == -1) {
			_exit(127);
		}
		(void) closefrom(STDERR_FILENO + 1);
		(void) execl(PFAUTH_PATH, "pfauth", arg1, arg2, arg3,
		    (char *)NULL);
		_exit(127);
	default:
		break;
	}

	/*
	 * Door server threads run with thread cancellation disabled, and
	 * that is deliberately left alone here. A cancellation posted when
	 * a door client aborts remains pending on the (pooled) server
	 * thread, so acting on cancellation at this wait would let a stale
	 * cancellation from an earlier aborted call terminate an unrelated
	 * authentication. If the client goes away mid-conversation, the
	 * helper notices through its own liveness checks and exits, and
	 * this wait then completes normally.
	 */
	while (waitpid(pid, &stat, 0) == -1) {
		if (errno != EINTR)
			break;
	}

	return (WIFEXITED(stat) && WEXITSTATUS(stat) == 0);
}

/*
 * Run the authentication helper to authenticate the user over the provided
 * terminal descriptor. The helper conducts a PAM conversation on that
 * terminal, in a process which is unrelated to the one being authenticated,
 * and generates the corresponding audit records. Returns true if the user
 * successfully authenticated.
 */
static volatile uint_t auth_inflight;

static bool
authenticate_user(const char *user, const char *profile, pid_t clientpid,
    int ttyfd)
{
	char pidstr[16];
	bool res;

	if (atomic_inc_uint_nv(&auth_inflight) > PFEXEC_MAX_AUTHS) {
		atomic_dec_uint(&auth_inflight);
		syslog(LOG_WARNING,
		    "too many concurrent authentication requests");
		return (false);
	}

	(void) snprintf(pidstr, sizeof (pidstr), "%d", (int)clientpid);
	res = run_pfauth(ttyfd, user, profile, pidstr);

	atomic_dec_uint(&auth_inflight);

	return (res);
}

static void
callback_forced_privs(pfexec_arg_t *pap)
{
	execattr_t *exec;
	char *value;
	priv_set_t *fset;
	void *res = alloca(setsz);

	/* Empty set signifies no forced privileges. */
	priv_emptyset(res);

	exec = getexecprof("Forced Privilege", KV_COMMAND, pap->pfa_path,
	    GET_ONE);

	if (exec == NULL && removeisapath(pap->pfa_path)) {
		exec = getexecprof("Forced Privilege", KV_COMMAND,
		    pap->pfa_path, GET_ONE);
	}

	if (exec == NULL) {
		(void) door_return(res, setsz, NULL, 0);
		return;
	}

	if ((value = kva_match(exec->attr, EXECATTR_IPRIV_KW)) == NULL ||
	    (fset = get_privset(value, NULL, pap->pfa_path)) == NULL) {
		free_execattr(exec);
		(void) door_return(res, setsz, NULL, 0);
		return;
	}

	priv_copyset(fset, res);
	priv_freeset(fset);

	free_execattr(exec);
	(void) door_return(res, setsz, NULL, 0);
}

static void
callback_user_privs(pfexec_arg_t *pap)
{
	priv_set_t *gset, *wset;
	uint32_t res;
	int flags;

	flags = _ENUM_PROFS_PROFILES;

	if ((pap->pfa_flags & PFA_AUTHENTICATED) != 0)
		flags |= _ENUM_PROFS_AUTHPROFILES;

	wset = (priv_set_t *)&pap->pfa_buf;
	gset = get_granted_privs(pap->pfa_uid, flags);

	res = priv_issubset(wset, gset);
	priv_freeset(gset);

	(void) door_return((char *)&res, sizeof (res), NULL, 0);
}

/*
 * Discard cached authentication state, such as pam_timestamp(7) stamps,
 * for the calling user and terminal; this implements pfexec -k. The work
 * happens in the pfauth helper, which runs pam_setcred(PAM_DELETE_CRED)
 * with the terminal on its standard file descriptors.
 */
static void
callback_auth_drop(pfexec_arg_t *pap, door_desc_t *dp, uint_t ndesc)
{
	struct passwd pw, *pwd;
	char buf[1024];
	char pidstr[16];
	uint32_t res = 0;
	int ttyfd = -1;

	if ((pap->pfa_flags & PFA_TTYFD) != 0 && ndesc > 0 &&
	    (dp[0].d_attributes & DOOR_DESCRIPTOR) != 0) {
		ttyfd = dp[0].d_data.d_desc.d_descriptor;
		dp++;
		ndesc--;
	}
	close_descs(dp, ndesc);

	if (ttyfd == -1)
		goto ret;

	if (getpwuid_r(pap->pfa_uid, &pw, buf, sizeof (buf), &pwd) != 0 ||
	    pwd == NULL) {
		goto ret;
	}

	(void) snprintf(pidstr, sizeof (pidstr), "%d", (int)pap->pfa_pid);
	if (run_pfauth(ttyfd, "-k", pwd->pw_name, pidstr))
		res = 1;

ret:
	if (ttyfd != -1)
		(void) close(ttyfd);
	(void) door_return((char *)&res, sizeof (res), NULL, 0);
}

static void
callback_pfexec(pfexec_arg_t *pap, door_desc_t *dp, uint_t ndesc)
{
	pfexec_reply_t *res = alloca(repsz);
	uid_t uid, euid, uuid;
	gid_t gid, egid;
	struct passwd pw, *pwd;
	char buf[1024];
	execattr_t *exec = NULL;
	char *value;
	priv_set_t *lset, *iset;
	size_t mysz = repsz - 2 * setsz;
	char *path = pap->pfa_path;
	char *noisapath;
	bool authed = (pap->pfa_flags & PFA_AUTHENTICATED) != 0;
	int ttyfd = -1;

	/*
	 * If the kernel attached a descriptor for the calling process's
	 * controlling terminal, extract it. Anything else is unexpected
	 * and is just closed.
	 */
	if ((pap->pfa_flags & PFA_TTYFD) != 0 && ndesc > 0 &&
	    (dp[0].d_attributes & DOOR_DESCRIPTOR) != 0) {
		ttyfd = dp[0].d_data.d_desc.d_descriptor;
		dp++;
		ndesc--;
	}
	close_descs(dp, ndesc);

	noisapath = strdup(path);
	if (noisapath != NULL && !removeisapath(noisapath)) {
		free(noisapath);
		noisapath = NULL;
	}

	/*
	 * Initialize the pfexec_reply_t to a sane state.
	 */
	res->pfr_vers = pap->pfa_vers;
	res->pfr_len = 0;
	res->pfr_ruid = PFEXEC_NOTSET;
	res->pfr_euid = PFEXEC_NOTSET;
	res->pfr_rgid = PFEXEC_NOTSET;
	res->pfr_egid = PFEXEC_NOTSET;
	res->pfr_setcred = B_FALSE;
	res->pfr_scrubenv = B_TRUE;
	res->pfr_clearflag = B_FALSE;
	res->pfr_allowed = B_FALSE;
	res->pfr_authreq = B_FALSE;
	res->pfr_setauth = B_FALSE;
	res->pfr_ioff = 0;
	res->pfr_loff = 0;

	uuid = pap->pfa_uid;

	if (getpwuid_r(uuid, &pw, buf, sizeof (buf), &pwd) != 0 || pwd == NULL)
		goto stdexec;

	/*
	 * If the user is not already authenticated, and has a controlling
	 * terminal on which an authentication conversation could be held,
	 * check whether the command matches a profile in the user's
	 * authenticated set. Without a controlling terminal the
	 * authenticated profiles are not considered.
	 */
	if (!authed && (pap->pfa_flags & PFA_HASTTY) != 0) {
		exec = getexecuser(pwd->pw_name, KV_COMMAND, path,
		    GET_ONE | GET_AUTHPROF);
		if (exec == NULL && noisapath != NULL) {
			exec = getexecuser(pwd->pw_name, KV_COMMAND,
			    noisapath, GET_ONE | GET_AUTHPROF);
		}

		if (exec != NULL && exec->name != NULL) {
			char *profname;
			bool ok;

			if (ttyfd == -1) {
				/*
				 * Authentication is required but no terminal
				 * descriptor accompanied this request. Ask
				 * the kernel to repeat the upcall with one
				 * attached.
				 */
				res->pfr_authreq = B_TRUE;
				res->pfr_allowed = B_TRUE;
				goto ret;
			}

			/*
			 * The lookup state is released now so that nothing
			 * is held across the potentially long wait for the
			 * user to complete authentication.
			 */
			profname = strdup(exec->name);
			free_execattr(exec);
			exec = NULL;
			free(noisapath);
			noisapath = NULL;
			if (profname == NULL) {
				res->pfr_allowed = B_FALSE;
				goto ret;
			}

			ok = authenticate_user(pwd->pw_name, profname,
			    pap->pfa_pid, ttyfd);
			free(profname);
			if (!ok) {
				res->pfr_allowed = B_FALSE;
				goto ret;
			}

			/*
			 * Authentication succeeded. The reply tells the
			 * kernel to record this in the new process
			 * credentials, and the profile search below now
			 * includes the authenticated profiles.
			 */
			authed = true;
			res->pfr_setauth = B_TRUE;
			res->pfr_setcred = B_TRUE;

			noisapath = strdup(path);
			if (noisapath != NULL && !removeisapath(noisapath)) {
				free(noisapath);
				noisapath = NULL;
			}
		} else {
			free_execattr(exec);
			exec = NULL;
		}
	}

	exec = getexecuser(pwd->pw_name, KV_COMMAND, path,
	    GET_ONE | (authed ? 0 : GET_PROF));
	if ((exec == NULL || exec->attr == NULL) && noisapath != NULL) {
		free_execattr(exec);
		exec = getexecuser(pwd->pw_name, KV_COMMAND, noisapath,
		    GET_ONE | (authed ? 0 : GET_PROF));
	}

	if (exec == NULL) {
		res->pfr_allowed = B_FALSE;
		goto ret;
	}

	if (exec->attr == NULL)
		goto stdexec;

	/* Found in execattr, so clearly we can use it */
	res->pfr_allowed = B_TRUE;

	uid = euid = PFEXEC_NOTSET;
	gid = egid = PFEXEC_NOTSET;
	lset = iset = NULL;

	/*
	 * If there's an error in parsing uid, gid, privs, then return
	 * failure.
	 */
	if ((value = kva_match(exec->attr, EXECATTR_UID_KW)) != NULL)
		euid = uid = get_uid(value, &res->pfr_allowed, path);

	if ((value = kva_match(exec->attr, EXECATTR_GID_KW)) != NULL)
		egid = gid = get_gid(value, &res->pfr_allowed, path);

	if ((value = kva_match(exec->attr, EXECATTR_EUID_KW)) != NULL)
		euid = get_uid(value, &res->pfr_allowed, path);

	if ((value = kva_match(exec->attr, EXECATTR_EGID_KW)) != NULL)
		egid = get_gid(value, &res->pfr_allowed, path);

	if ((value = kva_match(exec->attr, EXECATTR_LPRIV_KW)) != NULL)
		lset = get_privset(value, &res->pfr_allowed, path);

	if ((value = kva_match(exec->attr, EXECATTR_IPRIV_KW)) != NULL)
		iset = get_privset(value, &res->pfr_allowed, path);

	/*
	 * Remove LD_* variables in the kernel when the runtime linker might
	 * use them later on because the uids are equal.
	 */
	res->pfr_scrubenv = (uid != PFEXEC_NOTSET && euid == uid) ||
	    (gid != PFEXEC_NOTSET && egid == gid) || iset != NULL;

	res->pfr_euid = euid;
	res->pfr_ruid = uid;
	res->pfr_egid = egid;
	res->pfr_rgid = gid;

	/* Now add the privilege sets */
	res->pfr_ioff = res->pfr_loff = 0;
	if (iset != NULL) {
		res->pfr_ioff = mysz;
		priv_copyset(iset, PFEXEC_REPLY_IPRIV(res));
		mysz += setsz;
		priv_freeset(iset);
	}
	if (lset != NULL) {
		res->pfr_loff = mysz;
		priv_copyset(lset, PFEXEC_REPLY_LPRIV(res));
		mysz += setsz;
		priv_freeset(lset);
	}

	res->pfr_setcred = res->pfr_setauth || uid != PFEXEC_NOTSET ||
	    euid != PFEXEC_NOTSET || egid != PFEXEC_NOTSET ||
	    gid != PFEXEC_NOTSET || iset != NULL || lset != NULL;

	/* If the real uid changes, we stop running under a profile shell */
	res->pfr_clearflag = uid != PFEXEC_NOTSET && uid != uuid;
ret:
	free(noisapath);
	free_execattr(exec);
	if (ttyfd != -1)
		(void) close(ttyfd);
	(void) door_return((char *)res, mysz, NULL, 0);
	return;

stdexec:
	free(noisapath);
	free_execattr(exec);
	if (ttyfd != -1)
		(void) close(ttyfd);

	res->pfr_scrubenv = B_FALSE;
	/* If authentication was performed, the flag still needs to be set */
	res->pfr_setcred = res->pfr_setauth;
	res->pfr_allowed = B_TRUE;

	(void) door_return((char *)res, mysz, NULL, 0);
}

static void
callback(void *cookie __unused, char *argp, size_t asz, door_desc_t *dp,
    uint_t ndesc)
{
	pfexec_arg_t *pap = (pfexec_arg_t *)argp;

	if (!caller_is_kernel() ||
	    asz < sizeof (pfexec_arg_t) || pap->pfa_vers != PFEXEC_ARG_VERS) {
		close_descs(dp, ndesc);
		reject_call(pap, asz);
		return;
	}

	switch (pap->pfa_call) {
	case PFEXEC_EXEC_ATTRS:
		callback_pfexec(pap, dp, ndesc);
		break;
	case PFEXEC_FORCED_PRIVS:
		close_descs(dp, ndesc);
		callback_forced_privs(pap);
		break;
	case PFEXEC_USER_PRIVS:
		close_descs(dp, ndesc);
		callback_user_privs(pap);
		break;
	case PFEXEC_AUTH_DROP:
		callback_auth_drop(pap, dp, ndesc);
		break;
	default:
		close_descs(dp, ndesc);
		syslog(LOG_ERR, "Bad Call: %d\n", pap->pfa_call);
		break;
	}

	/*
	 * If the door_return(ptr, size, NULL, 0) fails, make sure we
	 * don't lose server threads.
	 */
	(void) door_return(NULL, 0, NULL, 0);
}

/*
 * Door server threads. The default door server thread creation function in
 * libc places no limit on the number of threads it will create and gives
 * each one a default sized (1 MiB) stack. Instead, create a private door
 * with a fixed pool of server threads that have smaller stacks. The door
 * is created with DOOR_NO_DEPLETION_CB so the pool never grows beyond its
 * initial size; the kernel queues further door invocations until a server
 * thread becomes available.
 *
 * The stack size must accommodate libsecdb's profile enumeration, which
 * keeps MAXPROFS-sized pointer arrays and copies of profile lists on
 * the stack, with headroom for nested profiles.
 */

/* Initialised in main() before the door is created, then never modified. */
static pthread_attr_t door_attr;

/*
 * Since the door uses DOOR_NO_DEPLETION_CB, this is only called from
 * door_xcreate() to populate the initial thread pool.
 */
static int
create_door_thread(door_info_t *dip __unused, void *(*startf)(void *),
    void *startfarg, void *cookie __unused)
{
	if (pthread_create(NULL, &door_attr, startf, startfarg) != 0)
		return (-1);
	return (1);
}

int
main(void)
{
	const priv_impl_info_t *info;
	int ret;

	(void) signal(SIGINT, unregister_pfexec);
	(void) signal(SIGQUIT, unregister_pfexec);
	(void) signal(SIGTERM, unregister_pfexec);
	(void) signal(SIGHUP, unregister_pfexec);

	info = getprivimplinfo();
	if (info == NULL)
		exit(1);

	if (fork() > 0)
		_exit(0);

	openlog("pfexecd", LOG_PID, LOG_DAEMON);
	setsz = info->priv_setsize * sizeof (priv_chunk_t);
	repsz = 2 * setsz + sizeof (pfexec_reply_t);

	init_isa_regex();

	if ((ret = pthread_attr_init(&door_attr)) != 0 ||
	    (ret = pthread_attr_setdetachstate(&door_attr,
	    PTHREAD_CREATE_DETACHED)) != 0 ||
	    (ret = pthread_attr_setstacksize(&door_attr,
	    DOOR_THREAD_STACKSIZE)) != 0) {
		errc(EXIT_FAILURE, ret,
		    "failed to configure door thread attributes");
	}

	doorfd = door_xcreate(callback, NULL, DOOR_NO_DEPLETION_CB,
	    create_door_thread, NULL, NULL, DOOR_THREADS);

	/*
	 * The kernel attaches at most one descriptor to an upcall, for the
	 * calling process's controlling terminal. Refuse anything more.
	 */
	if (doorfd != -1)
		(void) door_setparam(doorfd, DOOR_PARAM_DESC_MAX, 1);

	if (doorfd == -1 || register_pfexec(doorfd) != 0) {
		perror("doorfd");
		exit(1);
	}

	while (1)
		(void) sigpause(SIGINT);

	return (0);
}
