/*
 * This file and its contents are supplied under the terms of the
 * Common Development and Distribution License ("CDDL"), version 1.0.
 * You may only use this file in accordance with the terms of version
 * 1.0 of the CDDL.
 *
 * A full copy of the text of the CDDL should have accompanied this
 * source. A copy of the CDDL is also available via the Internet at
 * http://www.illumos.org/license/CDDL.
 */

/*
 * Copyright 2026 OmniOS Community Edition (OmniOSce) Association.
 */

/*
 * Tests for the TIOCGRAFT terminal ioctl, which binds a terminal that is
 * already the controlling terminal of another session onto the calling
 * process's session, without disturbing the owning session.
 *
 * The test creates a pseudo-terminal and an "owner" child process which
 * acquires it as its controlling terminal in a new session. A second
 * "grafter" child then exercises TIOCGRAFT against the same terminal,
 * including the expected failure modes, and checks that /dev/tty, reads,
 * writes and termios changes all work in the grafting session despite it
 * not being the terminal's foreground process group. Finally, the owner
 * is asked to confirm that its own session was unaffected.
 *
 * This test must run as root; it manipulates privilege sets.
 */

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <priv.h>
#include <procfs.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stropts.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

static uint_t failures;

static void
fail(const char *test, const char *fmt, ...)
{
	va_list ap;

	failures++;
	(void) fprintf(stderr, "FAIL %s: ", test);
	va_start(ap, fmt);
	(void) vfprintf(stderr, fmt, ap);
	va_end(ap);
	(void) fprintf(stderr, "\n");
}

static void
pass(const char *test)
{
	(void) printf("PASS %s\n", test);
}

static void
check_errno(const char *test, int ret, int expected_errno)
{
	if (ret != -1) {
		fail(test, "unexpectedly succeeded (wanted %s)",
		    strerror(expected_errno));
	} else if (errno != expected_errno) {
		fail(test, "wrong errno %d (%s), wanted %d (%s)",
		    errno, strerror(errno), expected_errno,
		    strerror(expected_errno));
	} else {
		pass(test);
	}
}

/*
 * The owner child. Acquire the terminal as the controlling terminal of a
 * new session, report readiness, then wait to be told to verify that the
 * session is still intact.
 */
static int
owner_child(const char *spath, int rfd, int wfd)
{
	struct termios ti;
	int sfd, tfd;
	pid_t fgpgrp;
	char c;

	if (setsid() == -1)
		err(EXIT_FAILURE, "owner setsid");

	/* This open makes the terminal our controlling terminal. */
	if ((sfd = open(spath, O_RDWR)) == -1)
		err(EXIT_FAILURE, "owner open %s", spath);

	/*
	 * The terminal modules are not automatically pushed onto a
	 * freshly opened pseudo-terminal, and without them the terminal
	 * rejects termios ioctls, so do what every terminal owner does:
	 * push the emulation and line discipline modules and then
	 * establish sane modes. The special characters are all left
	 * disabled; these tests do not use them.
	 */
	if (ioctl(sfd, I_PUSH, "ptem") != 0)
		err(EXIT_FAILURE, "owner I_PUSH ptem");
	if (ioctl(sfd, I_PUSH, "ldterm") != 0)
		err(EXIT_FAILURE, "owner I_PUSH ldterm");

	(void) memset(&ti, 0, sizeof (ti));
	ti.c_iflag = BRKINT | ICRNL | IXON;
	ti.c_oflag = OPOST | ONLCR;
	ti.c_cflag = CS8 | CREAD;
	ti.c_lflag = ISIG | ICANON | ECHO;
	(void) cfsetispeed(&ti, B38400);
	(void) cfsetospeed(&ti, B38400);
	if (tcsetattr(sfd, TCSANOW, &ti) != 0)
		err(EXIT_FAILURE, "owner tcsetattr");

	if ((tfd = open("/dev/tty", O_RDWR)) == -1)
		err(EXIT_FAILURE, "owner initial /dev/tty");
	(void) close(tfd);

	if (write(wfd, "R", 1) != 1)
		err(EXIT_FAILURE, "owner readiness write");

	if (read(rfd, &c, 1) != 1)
		err(EXIT_FAILURE, "owner command read");

	/* The graft has come and gone; check our session is untouched. */
	if ((tfd = open("/dev/tty", O_RDWR)) == -1) {
		warnx("owner /dev/tty no longer opens");
		return (EXIT_FAILURE);
	}
	(void) close(tfd);

	if (ioctl(sfd, TIOCGPGRP, &fgpgrp) != 0) {
		warn("owner TIOCGPGRP");
		return (EXIT_FAILURE);
	}
	if (fgpgrp != getpid()) {
		warnx("owner foreground pgrp changed: %d", (int)fgpgrp);
		return (EXIT_FAILURE);
	}

	return (0);
}

/*
 * The grafting child. Runs the TIOCGRAFT test cases against a terminal
 * which is the controlling terminal of the owner child's session. The
 * exit status is the number of test failures.
 */
static int
graft_child(const char *spath, int mfd)
{
	struct termios tio;
	struct stat st;
	psinfo_t ps;
	priv_set_t *pset;
	char buf[64];
	int sfd, tfd, psfd;
	ssize_t n;

	if ((sfd = open(spath, O_RDWR | O_NOCTTY)) == -1)
		err(EXIT_FAILURE, "graft open %s", spath);

	/* Not a session leader yet, so this must fail. */
	check_errno("graft-not-leader", ioctl(sfd, TIOCGRAFT, 0), ENOTTY);

	if (setsid() == -1)
		err(EXIT_FAILURE, "graft setsid");

	/* With no controlling terminal, /dev/tty must not open. */
	if ((tfd = open("/dev/tty", O_RDWR)) != -1) {
		fail("graft-no-ctty", "/dev/tty opened without a ctty");
		(void) close(tfd);
	} else {
		pass("graft-no-ctty");
	}

	/*
	 * A pipe is not a terminal. A fresh pipe is in fast-path mode, in
	 * which all terminal ioctls are rejected with EINVAL before they
	 * would reach the stream head.
	 */
	int pfd[2];
	if (pipe(pfd) != 0)
		err(EXIT_FAILURE, "pipe");
	check_errno("graft-not-tty", ioctl(pfd[0], TIOCGRAFT, 0), EINVAL);
	(void) close(pfd[0]);
	(void) close(pfd[1]);

	/* A descriptor which is not open for both read and write. */
	if ((tfd = open(spath, O_RDONLY | O_NOCTTY)) == -1)
		err(EXIT_FAILURE, "graft open readonly %s", spath);
	check_errno("graft-rdonly", ioctl(tfd, TIOCGRAFT, 0), EBADF);
	(void) close(tfd);

	/* Without PRIV_PROC_SESSION the graft is not permitted. */
	if ((pset = priv_allocset()) == NULL)
		err(EXIT_FAILURE, "priv_allocset");
	priv_emptyset(pset);
	if (priv_addset(pset, PRIV_PROC_SESSION) != 0 ||
	    setppriv(PRIV_OFF, PRIV_EFFECTIVE, pset) != 0) {
		err(EXIT_FAILURE, "could not drop PRIV_PROC_SESSION");
	}
	check_errno("graft-unpriv", ioctl(sfd, TIOCGRAFT, 0), EPERM);
	if (setppriv(PRIV_ON, PRIV_EFFECTIVE, pset) != 0)
		err(EXIT_FAILURE, "could not restore PRIV_PROC_SESSION");
	priv_freeset(pset);

	/* The real thing. */
	if (ioctl(sfd, TIOCGRAFT, 0) != 0) {
		fail("graft", "TIOCGRAFT failed: %s", strerror(errno));
		return ((int)failures);
	}
	pass("graft");

	/* A session may only have one controlling terminal. */
	check_errno("graft-again", ioctl(sfd, TIOCGRAFT, 0), ENOTTY);

	/*
	 * /dev/tty must now resolve to the grafted terminal. A descriptor
	 * from opening /dev/tty reports the indirection driver's device,
	 * not the terminal's, so the session's controlling terminal is
	 * verified through psinfo instead.
	 */
	if ((tfd = open("/dev/tty", O_RDWR)) == -1) {
		fail("graft-devtty", "/dev/tty did not open: %s",
		    strerror(errno));
		return ((int)failures);
	}
	pass("graft-devtty");

	if ((psfd = open("/proc/self/psinfo", O_RDONLY)) == -1)
		err(EXIT_FAILURE, "open psinfo");
	if (read(psfd, &ps, sizeof (ps)) != sizeof (ps))
		err(EXIT_FAILURE, "read psinfo");
	(void) close(psfd);
	if (fstat(sfd, &st) != 0)
		err(EXIT_FAILURE, "fstat");
	if (ps.pr_ttydev != st.st_rdev) {
		fail("graft-psinfo", "pr_ttydev is not the grafted terminal");
	} else {
		pass("graft-psinfo");
	}

	/*
	 * Terminal settings changes are exempt from job control on the
	 * grafted terminal, despite this process not being in the
	 * foreground process group. Failure would manifest as EIO, since
	 * this process group is orphaned, or as a job control stop.
	 */
	if (tcgetattr(tfd, &tio) != 0) {
		fail("graft-tcgetattr", "%s", strerror(errno));
		return ((int)failures);
	}
	tio.c_lflag &= ~ECHO;
	if (tcsetattr(tfd, TCSANOW, &tio) != 0) {
		fail("graft-tcsetattr", "%s", strerror(errno));
	} else {
		pass("graft-tcsetattr");
	}

	/* Reads are similarly exempt; SIGTTIN would result in EIO here. */
	if (write(mfd, "ping\n", 5) != 5)
		err(EXIT_FAILURE, "write to manager side");
	if ((n = read(tfd, buf, sizeof (buf))) != 5 ||
	    memcmp(buf, "ping\n", 5) != 0) {
		fail("graft-read", "read returned %zd: %s", n,
		    n == -1 ? strerror(errno) : "wrong data");
	} else {
		pass("graft-read");
	}

	/* And writes. */
	if (write(tfd, "pong\n", 5) != 5) {
		fail("graft-write", "%s", strerror(errno));
	} else {
		size_t got = 0;

		/* ONLCR is set by default, so expect a carriage return. */
		while (got < 6) {
			n = read(mfd, buf + got, sizeof (buf) - got);
			if (n <= 0)
				break;
			got += (size_t)n;
		}
		if (got != 6 || memcmp(buf, "pong\r\n", 6) != 0) {
			fail("graft-write", "readback returned %zu bytes",
			    got);
		} else {
			pass("graft-write");
		}
	}

	(void) close(tfd);
	(void) close(sfd);

	return ((int)failures);
}

int
main(void)
{
	int mfd, status, ret = 0;
	int rpipe[2], wpipe[2];
	char *spath;
	pid_t owner, grafter;
	char c;

	if ((mfd = posix_openpt(O_RDWR | O_NOCTTY)) == -1)
		err(EXIT_FAILURE, "posix_openpt");
	if (grantpt(mfd) != 0 || unlockpt(mfd) != 0)
		err(EXIT_FAILURE, "grantpt/unlockpt");
	if ((spath = ptsname(mfd)) == NULL)
		err(EXIT_FAILURE, "ptsname");

	if (pipe(rpipe) != 0 || pipe(wpipe) != 0)
		err(EXIT_FAILURE, "pipe");

	if ((owner = fork()) == -1)
		err(EXIT_FAILURE, "fork owner");
	if (owner == 0) {
		(void) close(mfd);
		(void) close(rpipe[0]);
		(void) close(wpipe[1]);
		_exit(owner_child(spath, wpipe[0], rpipe[1]));
	}
	(void) close(rpipe[1]);
	(void) close(wpipe[0]);

	/* Wait for the owner to have acquired the terminal. */
	if (read(rpipe[0], &c, 1) != 1 || c != 'R')
		errx(EXIT_FAILURE, "owner did not become ready");

	if ((grafter = fork()) == -1)
		err(EXIT_FAILURE, "fork grafter");
	if (grafter == 0)
		_exit(graft_child(spath, mfd));

	if (waitpid(grafter, &status, 0) == -1)
		err(EXIT_FAILURE, "waitpid grafter");
	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
		warnx("graft tests failed (status %x)", status);
		ret = EXIT_FAILURE;
	}

	/* Now that the graft is gone, ask the owner to check its session. */
	if (write(wpipe[1], "C", 1) != 1)
		err(EXIT_FAILURE, "owner command write");
	if (waitpid(owner, &status, 0) == -1)
		err(EXIT_FAILURE, "waitpid owner");
	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
		warnx("owner session was disturbed (status %x)", status);
		ret = EXIT_FAILURE;
	}

	if (ret == 0)
		(void) printf("all tests passed\n");

	return (ret);
}
