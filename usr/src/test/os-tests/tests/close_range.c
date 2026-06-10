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
 * Tests for close_range(2): range semantics, CLOEXEC/CLOFORK marking and
 * argument validation. We also exercise closefrom(3C), which is implemented
 * on top of close_range().
 */

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static uint_t failures = 0;

#define	TFAIL(name, fmt, ...)	do {					\
	(void) fprintf(stderr, "TEST FAILED: %s: " fmt "\n",		\
	    (name), ##__VA_ARGS__);					\
	failures++;							\
} while (0)

#define	TPASS(name)	(void) printf("TEST PASSED: %s\n", (name))

/*
 * The test descriptors are pinned at known numbers, comfortably above
 * anything the process opens for itself.
 */
#define	FD_BASE		50
#define	FD_LIMIT	70

static void
open_fds(void)
{
	int fd;

	if ((fd = open("/dev/null", O_RDONLY)) == -1)
		err(EXIT_FAILURE, "could not open /dev/null");

	for (int i = FD_BASE; i < FD_LIMIT; i++) {
		if (dup2(fd, i) != i)
			err(EXIT_FAILURE, "could not dup to fd %d", i);
	}

	(void) close(fd);
}

/*
 * Verify that fd is open and carries precisely the descriptor flags in
 * 'want', or, if want is -1, that fd is closed.
 */
static bool
check_fd(const char *name, int fd, int want)
{
	int fl = fcntl(fd, F_GETFD);

	if (want == -1) {
		if (fl != -1 || errno != EBADF) {
			TFAIL(name, "fd %d unexpectedly open (flags %#x)",
			    fd, fl);
			return (false);
		}
		return (true);
	}

	if (fl == -1) {
		TFAIL(name, "fd %d unexpectedly closed", fd);
		return (false);
	}
	if ((fl & (FD_CLOEXEC | FD_CLOFORK)) != want) {
		TFAIL(name, "fd %d flags %#x, expected %#x", fd, fl, want);
		return (false);
	}
	return (true);
}

static bool
check_range(const char *name, int low, int high, int want)
{
	bool pass = true;

	for (int fd = low; fd <= high; fd++) {
		if (!check_fd(name, fd, want))
			pass = false;
	}
	return (pass);
}

static void
expect_einval(const char *name, uint_t low, uint_t high, int flags)
{
	int ret = close_range(low, high, flags);

	if (ret != -1) {
		TFAIL(name, "close_range returned %d, expected failure", ret);
		return;
	}
	if (errno != EINVAL) {
		TFAIL(name, "got errno %d (%s), wanted EINVAL", errno,
		    strerror(errno));
		return;
	}
	TPASS(name);
}

int
main(void)
{
	open_fds();

	/* Invalid arguments. An empty high range so a bug cannot bite. */
	expect_einval("low-gt-high", 5, 4, 0);
	expect_einval("flag-reserved", 1000000, 1000001, 1 << 1);
	expect_einval("flag-low", 1000000, 1000001, 1 << 0);
	expect_einval("flag-junk", 1000000, 1000001, ~0);

	/* A range beyond every open descriptor succeeds and does nothing */
	if (close_range(1000000, UINT_MAX, 0) != 0) {
		TFAIL("range-empty", "failed: %s", strerror(errno));
	} else {
		TPASS("range-empty");
	}

	/* Close the middle of the block and check the neighbours */
	if (close_range(55, 59, 0) != 0)
		err(EXIT_FAILURE, "close_range(55, 59, 0)");
	if (check_range("close-mid", 50, 54, 0) &&
	    check_range("close-mid", 55, 59, -1) &&
	    check_range("close-mid", 60, 69, 0)) {
		TPASS("close-mid");
	}

	/* Mark ranges with each flag, then both together */
	if (close_range(60, 64, CLOSE_RANGE_CLOEXEC) != 0)
		err(EXIT_FAILURE, "close_range(CLOSE_RANGE_CLOEXEC)");
	if (check_range("mark-cloexec", 60, 64, FD_CLOEXEC) &&
	    check_range("mark-cloexec", 65, 69, 0)) {
		TPASS("mark-cloexec");
	}

	if (close_range(65, 67, CLOSE_RANGE_CLOFORK) != 0)
		err(EXIT_FAILURE, "close_range(CLOSE_RANGE_CLOFORK)");
	if (check_range("mark-clofork", 65, 67, FD_CLOFORK) &&
	    check_range("mark-clofork", 68, 69, 0)) {
		TPASS("mark-clofork");
	}

	if (close_range(68, 69,
	    CLOSE_RANGE_CLOEXEC | CLOSE_RANGE_CLOFORK) != 0)
		err(EXIT_FAILURE, "close_range(both flags)");
	if (check_range("mark-both", 68, 69, FD_CLOEXEC | FD_CLOFORK))
		TPASS("mark-both");

	/* Marking a range that includes closed descriptors skips them */
	if (close_range(55, 64, CLOSE_RANGE_CLOEXEC) != 0)
		err(EXIT_FAILURE, "close_range over closed fds");
	if (check_range("mark-gaps", 55, 59, -1) &&
	    check_range("mark-gaps", 60, 64, FD_CLOEXEC)) {
		TPASS("mark-gaps");
	}

	/* The closefrom() form: everything at or above low goes */
	if (close_range(52, UINT_MAX, 0) != 0)
		err(EXIT_FAILURE, "close_range(52, UINT_MAX, 0)");
	if (check_range("close-from", 50, 51, 0) &&
	    check_range("close-from", 52, FD_LIMIT - 1, -1)) {
		TPASS("close-from");
	}

	/* And closefrom(3C) itself, now built on close_range() */
	open_fds();
	closefrom(FD_BASE + 1);
	if (check_fd("closefrom", FD_BASE, 0) &&
	    check_range("closefrom", FD_BASE + 1, FD_LIMIT - 1, -1)) {
		TPASS("closefrom");
	}

	if (failures == 0) {
		(void) printf("All tests passed\n");
		return (EXIT_SUCCESS);
	}

	(void) fprintf(stderr, "%u test(s) failed\n", failures);
	return (EXIT_FAILURE);
}
