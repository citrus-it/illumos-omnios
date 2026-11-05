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
 * Test the lock that libc uses to serialise operations on POSIX named
 * semaphores, shared memory objects and message queues. A lock file left by a
 * process that has gone must not block anybody, a killed holder must not stick
 * the object, and contention must not result in long delays or spurious
 * failures. A message queue left incomplete by a process that died must not
 * stop its name being used again.
 */

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <mqueue.h>
#include <semaphore.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

/* The old implementation gave up after 64 seconds. */
#define	MAX_SECS	10
#define	KILL_ITERS	200
#define	CONTEND_PROCS	16
#define	CONTEND_ITERS	200
#define	EXCL_PROCS	8
#define	EXCL_ITERS	50

typedef struct {
	const char	*po_desc;
	const char	*po_lockpfx;
	bool		(*po_open)(const char *, int);
	void		(*po_unlink)(const char *);
} pobj_t;

static bool
sem_try(const char *name, int oflag)
{
	sem_t *s = sem_open(name, oflag, 0600, 1);

	if (s == SEM_FAILED)
		return (false);
	(void) sem_close(s);
	return (true);
}

static void
sem_rm(const char *name)
{
	(void) sem_unlink(name);
}

static bool
shm_try(const char *name, int oflag)
{
	int fd = shm_open(name, oflag | O_RDWR, 0600);

	if (fd < 0)
		return (false);
	(void) close(fd);
	return (true);
}

static void
shm_rm(const char *name)
{
	(void) shm_unlink(name);
}

static bool
mq_try(const char *name, int oflag)
{
	mqd_t mq = mq_open(name, oflag | O_RDWR, 0600, NULL);

	if (mq == (mqd_t)-1)
		return (false);
	(void) mq_close(mq);
	return (true);
}

static void
mq_rm(const char *name)
{
	(void) mq_unlink(name);
}

static const pobj_t pobjs[] = {
	{ "semaphore", ".SEML", sem_try, sem_rm },
	{ "shared memory", ".SHML", shm_try, shm_rm },
	{ "message queue", ".MQL", mq_try, mq_rm },
};

static int nfail;

static void
fail(const pobj_t *po, const char *test, const char *msg)
{
	warnx("TEST FAILED: %s: %s: %s", po->po_desc, test, msg);
	nfail++;
}

static hrtime_t
elapsed_secs(hrtime_t start)
{
	return ((gethrtime() - start) / NANOSEC);
}

static void __PRINTFLIKE(3)
mkpath(char *buf, size_t len, const char *fmt, ...)
{
	va_list ap;
	int n;

	va_start(ap, fmt);
	n = vsnprintf(buf, len, fmt, ap);
	va_end(ap);

	if (n < 0 || (size_t)n >= len)
		errx(EXIT_FAILURE, "path too long or could not be formatted");
}

static void
lock_path(const pobj_t *po, const char *name, char *buf, size_t len)
{
	mkpath(buf, len, "/tmp/%s%s", po->po_lockpfx, name + 1);
}

/*
 * A process killed while creating a lock file can leave behind the temporary
 * file that libc creates it under. Remove any left by a killed child.
 */
static void
rm_lock_tmp(pid_t pid)
{
	char path[PATH_MAX];

	mkpath(path, sizeof (path), "/tmp/.pos4tmp.%d.1", (int)pid);
	(void) unlink(path);
}

static void
test_stale(const pobj_t *po, const char *name)
{
	char path[PATH_MAX];
	hrtime_t start;
	int fd;

	lock_path(po, name, path, sizeof (path));
	if ((fd = open(path, O_CREAT | O_EXCL | O_RDWR, 0644)) < 0)
		err(EXIT_FAILURE, "failed to create %s", path);
	(void) close(fd);

	start = gethrtime();
	if (!po->po_open(name, O_CREAT))
		fail(po, "stale lock file", strerror(errno));
	if (elapsed_secs(start) >= MAX_SECS)
		fail(po, "stale lock file", "open took too long");

	start = gethrtime();
	po->po_unlink(name);
	if (elapsed_secs(start) >= MAX_SECS)
		fail(po, "stale lock file", "unlink took too long");
	(void) unlink(path);
}

static void
test_kill(const pobj_t *po, const char *name)
{
	if (!po->po_open(name, O_CREAT)) {
		fail(po, "killed holder", strerror(errno));
		return;
	}

	for (uint_t i = 0; i < KILL_ITERS; i++) {
		struct timespec ts = { 0, (i % 20 + 1) * 100000 };
		hrtime_t start;
		pid_t pid;

		if ((pid = fork()) < 0)
			err(EXIT_FAILURE, "fork failed");
		if (pid == 0) {
			for (;;)
				(void) po->po_open(name, 0);
		}

		(void) nanosleep(&ts, NULL);
		if (kill(pid, SIGKILL) != 0)
			err(EXIT_FAILURE, "failed to kill %d", (int)pid);
		if (waitpid(pid, NULL, 0) != pid)
			err(EXIT_FAILURE, "failed to wait for %d", (int)pid);
		rm_lock_tmp(pid);

		start = gethrtime();
		if (!po->po_open(name, 0))
			fail(po, "killed holder", strerror(errno));
		if (elapsed_secs(start) >= MAX_SECS) {
			fail(po, "killed holder", "open took too long");
			break;
		}
	}

	po->po_unlink(name);
}

/*
 * A creator killed while creating the lock file must not leave it with a mode
 * that stops other users from locking it. Kill a process with a restrictive
 * umask while it creates and removes the object, then check the mode of any
 * lock file left behind and that another user is not held up.
 */
static void
test_lockmode(const pobj_t *po, const char *name)
{
	char path[PATH_MAX], msg[64];
	struct stat st;

	if (geteuid() != 0) {
		(void) printf("%s: lock file mode: skipped, needs root\n",
		    po->po_desc);
		return;
	}

	lock_path(po, name, path, sizeof (path));

	for (uint_t i = 0; i < KILL_ITERS; i++) {
		struct timespec ts = { 0, (i % 20 + 1) * 100000 };
		int status;
		pid_t pid;

		if ((pid = fork()) < 0)
			err(EXIT_FAILURE, "fork failed");
		if (pid == 0) {
			(void) umask(077);
			for (;;) {
				(void) po->po_open(name, O_CREAT);
				po->po_unlink(name);
			}
		}

		(void) nanosleep(&ts, NULL);
		if (kill(pid, SIGKILL) != 0)
			err(EXIT_FAILURE, "failed to kill %d", (int)pid);
		if (waitpid(pid, NULL, 0) != pid)
			err(EXIT_FAILURE, "failed to wait for %d", (int)pid);
		rm_lock_tmp(pid);

		if (lstat(path, &st) == 0 && (st.st_mode & 0777) != 0666) {
			(void) snprintf(msg, sizeof (msg),
			    "lock file left with mode %o",
			    (uint_t)(st.st_mode & 0777));
			fail(po, "lock file mode", msg);
			break;
		}

		if ((pid = fork()) < 0)
			err(EXIT_FAILURE, "fork failed");
		if (pid == 0) {
			hrtime_t start;

			if (setgid(GID_NOBODY) != 0 || setuid(UID_NOBODY) != 0)
				_exit(2);
			start = gethrtime();
			/* The object may belong to root, so this can fail. */
			(void) po->po_open(name, O_CREAT);
			_exit(elapsed_secs(start) >= MAX_SECS ? 1 : 0);
		}
		if (waitpid(pid, &status, 0) != pid)
			err(EXIT_FAILURE, "failed to wait for %d", (int)pid);
		if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
			fail(po, "lock file mode", WIFEXITED(status) &&
			    WEXITSTATUS(status) == 2 ?
			    "failed to become another user" :
			    "another user was held up");
			break;
		}
	}

	po->po_unlink(name);
	(void) unlink(path);
}

static void
test_contend(const pobj_t *po, const char *name)
{
	pid_t pids[CONTEND_PROCS];

	if (!po->po_open(name, O_CREAT)) {
		fail(po, "contention", strerror(errno));
		return;
	}

	for (uint_t i = 0; i < CONTEND_PROCS; i++) {
		if ((pids[i] = fork()) < 0)
			err(EXIT_FAILURE, "fork failed");
		if (pids[i] != 0)
			continue;

		for (uint_t j = 0; j < CONTEND_ITERS; j++) {
			hrtime_t start = gethrtime();

			if (!po->po_open(name, 0) ||
			    elapsed_secs(start) >= MAX_SECS) {
				_exit(EXIT_FAILURE);
			}
		}
		_exit(EXIT_SUCCESS);
	}

	for (uint_t i = 0; i < CONTEND_PROCS; i++) {
		int status;

		if (waitpid(pids[i], &status, 0) != pids[i])
			err(EXIT_FAILURE, "failed to wait for child");
		if (!WIFEXITED(status) || WEXITSTATUS(status) != EXIT_SUCCESS)
			fail(po, "contention", "child failed");
	}

	po->po_unlink(name);
}

/* Only one of a number of racing exclusive creators can win. */
static void
test_excl(const pobj_t *po, const char *name)
{
	for (uint_t i = 0; i < EXCL_ITERS; i++) {
		pid_t pids[EXCL_PROCS];
		int pipefd[2];
		uint_t nwon = 0;

		if (pipe(pipefd) != 0)
			err(EXIT_FAILURE, "failed to create pipe");

		for (uint_t j = 0; j < EXCL_PROCS; j++) {
			char c;

			if ((pids[j] = fork()) < 0)
				err(EXIT_FAILURE, "fork failed");
			if (pids[j] != 0)
				continue;

			(void) close(pipefd[1]);
			(void) read(pipefd[0], &c, 1);
			_exit(po->po_open(name, O_CREAT | O_EXCL) ? 0 : 1);
		}

		(void) close(pipefd[0]);
		(void) close(pipefd[1]);

		for (uint_t j = 0; j < EXCL_PROCS; j++) {
			int status;

			if (waitpid(pids[j], &status, 0) != pids[j])
				err(EXIT_FAILURE, "failed to wait for child");
			if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
				nwon++;
		}

		po->po_unlink(name);

		if (nwon != 1) {
			fail(po, "exclusive create", "expected one winner");
			break;
		}
	}
}

/* The lock path is in a world-writable directory. Do not follow symlinks. */
static void
test_symlink(const pobj_t *po, const char *name)
{
	char path[PATH_MAX], target[PATH_MAX + sizeof (".target")];
	struct stat st;

	lock_path(po, name, path, sizeof (path));
	mkpath(target, sizeof (target), "%s.target", path);

	if (symlink(target, path) != 0)
		err(EXIT_FAILURE, "failed to create symlink %s", path);

	if (po->po_open(name, O_CREAT)) {
		fail(po, "symlink", "open succeeded");
		po->po_unlink(name);
	}
	if (lstat(target, &st) == 0) {
		fail(po, "symlink", "followed link");
		(void) unlink(target);
	}
	(void) unlink(path);
}

static void
mq_path(const char *pfx, const char *name, char *buf, size_t len)
{
	mkpath(buf, len, "/tmp/%s%s", pfx, name + 1);
}

static void
mq_mkfile(const char *pfx, const char *name, off_t size)
{
	char path[PATH_MAX];
	int fd;

	mq_path(pfx, name, path, sizeof (path));
	if ((fd = open(path, O_CREAT | O_EXCL | O_RDWR, 0600)) < 0)
		err(EXIT_FAILURE, "failed to create %s", path);
	if (ftruncate(fd, size) != 0)
		err(EXIT_FAILURE, "failed to size %s", path);
	(void) close(fd);
}

static bool
mq_hasfile(const char *pfx, const char *name)
{
	char path[PATH_MAX];
	struct stat st;

	mq_path(pfx, name, path, sizeof (path));
	return (lstat(path, &st) == 0);
}

static void
mq_rmfile(const char *pfx, const char *name)
{
	char path[PATH_MAX];

	mq_path(pfx, name, path, sizeof (path));
	(void) unlink(path);
}

/*
 * A message queue is a permission file and a data file. The files left by a
 * process that died part way through creating or removing a queue must not
 * stop the name being used again.
 */
static void
test_mq_incomplete(const pobj_t *po, const char *name)
{
	static const struct {
		const char	*mi_desc;
		bool		mi_perm;
		off_t		mi_data;	/* -1 for no data file */
	} cases[] = {
		{ "permission file only", true, -1 },
		{ "empty data file", true, 0 },
		{ "uninitialised data file", true, 4096 },
		{ "data file only", false, 4096 },
	};
	static const struct {
		const char	*mo_desc;
		int		mo_oflag;	/* -1 for mq_unlink() */
	} ops[] = {
		{ "open", 0 },
		{ "create", O_CREAT },
		{ "exclusive create", O_CREAT | O_EXCL },
		{ "unlink", -1 },
	};
	char msg[128];

	for (size_t i = 0; i < ARRAY_SIZE(cases); i++) {
		for (size_t j = 0; j < ARRAY_SIZE(ops); j++) {
			int oflag = ops[j].mo_oflag;
			bool ok;
			int e;

			if (cases[i].mi_perm)
				mq_mkfile(".MQP", name, 0);
			if (cases[i].mi_data >= 0)
				mq_mkfile(".MQD", name, cases[i].mi_data);

			errno = 0;
			if (oflag == -1)
				ok = mq_unlink(name) == 0;
			else
				ok = po->po_open(name, oflag);
			e = errno;

			(void) snprintf(msg, sizeof (msg), "%s: %s: %s",
			    cases[i].mi_desc, ops[j].mo_desc,
			    ok ? "succeeded" : strerror(e));

			if (oflag == -1 || (oflag & O_CREAT) == 0) {
				/* The queue does not exist. */
				if (ok || e != ENOENT)
					fail(po, "incomplete queue", msg);
			} else if (!ok) {
				fail(po, "incomplete queue", msg);
			} else {
				po->po_unlink(name);
			}

			/* A create or unlink must have removed the leftovers */
			if (oflag != 0 && (mq_hasfile(".MQP", name) ||
			    mq_hasfile(".MQD", name))) {
				(void) snprintf(msg, sizeof (msg),
				    "%s: %s: files left behind",
				    cases[i].mi_desc, ops[j].mo_desc);
				fail(po, "incomplete queue", msg);
			}

			mq_rmfile(".MQP", name);
			mq_rmfile(".MQD", name);
		}
	}
}

/* POSIX requires FD_CLOEXEC to be set on the shm_open() descriptor. */
static void
test_cloexec(const pobj_t *po, const char *name)
{
	int fd, flags;

	if ((fd = shm_open(name, O_CREAT | O_RDWR, 0600)) < 0) {
		fail(po, "close on exec", strerror(errno));
		return;
	}

	if ((flags = fcntl(fd, F_GETFD)) < 0)
		err(EXIT_FAILURE, "failed to get descriptor flags");
	if ((flags & FD_CLOEXEC) == 0)
		fail(po, "close on exec", "FD_CLOEXEC is not set");

	(void) close(fd);
	(void) shm_unlink(name);
}

int
main(void)
{
	char name[64];

	mkpath(name, sizeof (name), "/pos4obj.%d", (int)getpid());

	for (size_t i = 0; i < ARRAY_SIZE(pobjs); i++) {
		const pobj_t *po = &pobjs[i];

		test_stale(po, name);
		test_kill(po, name);
		test_lockmode(po, name);
		test_contend(po, name);
		test_excl(po, name);
		test_symlink(po, name);
	}

	test_cloexec(&pobjs[1], name);
	test_mq_incomplete(&pobjs[2], name);

	if (nfail == 0)
		(void) printf("All tests passed\n");
	return (nfail == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
}
