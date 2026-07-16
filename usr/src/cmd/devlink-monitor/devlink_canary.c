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

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <signal.h>
#include <syslog.h>
#include <time.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>

#define	CANARY_BYTE	0x45		/* 'E': bit 5 is clear */
#define	CANARY_FILE	".devlink_canary"
#define	MAX_REPORT_LINES 64
#define	CHUNK		(1024 * 1024)

typedef struct region {
	struct region *r_next;
	char r_path[PATH_MAX];		/* file path; empty for anon */
	int r_fd;			/* file descriptor; -1 for anon */
	uint8_t *r_map;			/* long-lived mapping */
	size_t r_size;
} region_t;

static region_t *regions;
static size_t region_size = 16 * 1024 * 1024;
static uint_t interval = 5;
static const char *spool = "/var/tmp/devlink-monitor";
static bool abort_on_trip = false;
static volatile sig_atomic_t quitting = 0;
static uint8_t iobuf[CHUNK];

static void
usage(void)
{
	(void) fprintf(stderr, "Usage: devlink-canary [-a] [-d dir]... "
	    "[-i seconds] [-s size[k|m|g]] [-S spooldir]\n");
	exit(2);
}

static void
handler(int sig __unused)
{
	quitting = 1;
}

static bool
fill_region(region_t *r)
{
	size_t off, n;

	if (r->r_fd == -1) {
		(void) memset(r->r_map, CANARY_BYTE, r->r_size);
		return (true);
	}

	(void) memset(iobuf, CANARY_BYTE, sizeof (iobuf));
	for (off = 0; off < r->r_size; off += n) {
		n = r->r_size - off;
		if (n > sizeof (iobuf))
			n = sizeof (iobuf);
		if (pwrite(r->r_fd, iobuf, n, off) != (ssize_t)n) {
			syslog(LOG_ERR, "write of %s failed: %m", r->r_path);
			return (false);
		}
	}
	if (fsync(r->r_fd) != 0) {
		syslog(LOG_ERR, "fsync of %s failed: %m", r->r_path);
		return (false);
	}
	return (true);
}

static region_t *
region_file(const char *dir)
{
	region_t *r;

	if ((r = calloc(1, sizeof (*r))) == NULL)
		return (NULL);

	(void) snprintf(r->r_path, sizeof (r->r_path), "%s/%s",
	    dir, CANARY_FILE);
	(void) unlink(r->r_path);
	r->r_size = region_size;
	r->r_fd = open(r->r_path, O_RDWR | O_CREAT | O_TRUNC, 0600);
	if (r->r_fd == -1) {
		syslog(LOG_ERR, "cannot create %s: %m", r->r_path);
		free(r);
		return (NULL);
	}

	if (!fill_region(r)) {
		(void) close(r->r_fd);
		(void) unlink(r->r_path);
		free(r);
		return (NULL);
	}

	r->r_map = mmap(NULL, r->r_size, PROT_READ, MAP_SHARED, r->r_fd, 0);
	if (r->r_map == MAP_FAILED) {
		syslog(LOG_ERR, "cannot map %s: %m", r->r_path);
		(void) close(r->r_fd);
		(void) unlink(r->r_path);
		free(r);
		return (NULL);
	}

	return (r);
}

static region_t *
region_anon(void)
{
	region_t *r;

	if ((r = calloc(1, sizeof (*r))) == NULL)
		return (NULL);

	r->r_fd = -1;
	r->r_size = region_size;
	r->r_map = mmap(NULL, r->r_size, PROT_READ | PROT_WRITE,
	    MAP_ANON | MAP_PRIVATE, -1, 0);
	if (r->r_map == MAP_FAILED) {
		syslog(LOG_ERR, "cannot map anonymous region: %m");
		free(r);
		return (NULL);
	}
	(void) fill_region(r);

	return (r);
}

static FILE *
report_begin(void)
{
	char path[PATH_MAX], host[64];
	FILE *fp;

	(void) mkdir(spool, 0755);
	(void) snprintf(path, sizeof (path), "%s/canary.report.%ld.%ld",
	    spool, (long)time(NULL), (long)getpid());
	if ((fp = fopen(path, "w")) == NULL) {
		syslog(LOG_ERR, "cannot create report %s: %m", path);
		return (NULL);
	}

	if (gethostname(host, sizeof (host)) != 0)
		(void) strlcpy(host, "unknown", sizeof (host));
	(void) fprintf(fp, "devlink-canary report: host %s pid %ld "
	    "size %zu interval %u\n", host, (long)getpid(), region_size,
	    interval);

	return (fp);
}

static void
report_line(FILE *fp, uint_t count, const region_t *r, const char *view,
    size_t off, uint8_t got)
{
	char tstr[32];
	time_t now = time(NULL);

	if (fp == NULL || count > MAX_REPORT_LINES)
		return;
	if (count == MAX_REPORT_LINES) {
		(void) fprintf(fp, "further reports suppressed\n");
		return;
	}

	(void) strftime(tstr, sizeof (tstr), "%Y-%m-%dT%H:%M:%SZ",
	    gmtime(&now));
	(void) fprintf(fp, "%s region %s view %s offset %zu "
	    "got %#04x want %#04x xor %#04x "
	    "mod4 %zu mod8 %zu mod16 %zu mod64 %zu pageoff %zu\n",
	    tstr, r->r_fd == -1 ? "anon" : r->r_path, view, off,
	    got, CANARY_BYTE, got ^ CANARY_BYTE,
	    off % 4, off % 8, off % 16, off % 64, off % 4096);
}

static uint_t
scan(FILE **fpp, const region_t *r, const char *view, const uint8_t *buf,
    size_t len, size_t base, uint_t count)
{
	size_t i;

	for (i = 0; i < len; i++) {
		if (buf[i] == CANARY_BYTE)
			continue;
		if (*fpp == NULL)
			*fpp = report_begin();
		report_line(*fpp, count, r, view, base + i, buf[i]);
		count++;
	}
	return (count);
}

static uint_t
verify_region(region_t *r, FILE **fpp)
{
	size_t off, n;
	uint_t count = 0;

	count = scan(fpp, r, "mmap", r->r_map, r->r_size, 0, count);

	if (r->r_fd != -1) {
		for (off = 0; off < r->r_size; off += n) {
			ssize_t rd;

			n = r->r_size - off;
			if (n > sizeof (iobuf))
				n = sizeof (iobuf);
			rd = pread(r->r_fd, iobuf, n, off);
			if (rd <= 0)
				break;
			count = scan(fpp, r, "pread", iobuf, (size_t)rd,
			    off, count);
		}
	}

	if (count > 0) {
		syslog(LOG_ALERT, "canary tripped: %u corrupt bytes in %s",
		    count, r->r_fd == -1 ? "anon region" : r->r_path);
		(void) fill_region(r);
	}

	return (count);
}

static size_t
parse_size(const char *arg)
{
	unsigned long long val;
	char *end;

	val = strtoull(arg, &end, 0);
	switch (*end) {
	case 'g':
	case 'G':
		val *= 1024;
		/* FALLTHROUGH */
	case 'm':
	case 'M':
		val *= 1024;
		/* FALLTHROUGH */
	case 'k':
	case 'K':
		val *= 1024;
		end++;
		break;
	default:
		break;
	}
	if (val == 0 || *end != '\0' || val > SIZE_MAX / 2) {
		(void) fprintf(stderr, "invalid size: %s\n", arg);
		usage();
	}
	return ((size_t)val);
}

int
main(int argc, char **argv)
{
	const char *dirs[8];
	uint_t ndirs = 0, i;
	region_t *r, **tail = &regions;
	struct sigaction sa;
	int c;

	while ((c = getopt(argc, argv, "ad:i:s:S:")) != -1) {
		switch (c) {
		case 'a':
			abort_on_trip = true;
			break;
		case 'd':
			if (ndirs >= sizeof (dirs) / sizeof (dirs[0])) {
				(void) fprintf(stderr, "too many -d\n");
				usage();
			}
			dirs[ndirs++] = optarg;
			break;
		case 'i':
			interval = (uint_t)strtoul(optarg, NULL, 0);
			if (interval == 0)
				usage();
			break;
		case 's':
			region_size = parse_size(optarg);
			break;
		case 'S':
			spool = optarg;
			break;
		default:
			usage();
		}
	}

	if (ndirs == 0) {
		dirs[ndirs++] = "/etc/dev";
		dirs[ndirs++] = "/var/run";
	}

	openlog("devlink-canary", LOG_PID, LOG_DAEMON);

	bzero(&sa, sizeof (sa));
	sa.sa_handler = handler;
	(void) sigaction(SIGTERM, &sa, NULL);
	(void) sigaction(SIGINT, &sa, NULL);

	for (i = 0; i < ndirs; i++) {
		if ((r = region_file(dirs[i])) != NULL) {
			*tail = r;
			tail = &r->r_next;
		}
	}
	if ((r = region_anon()) != NULL) {
		*tail = r;
		tail = &r->r_next;
	}

	if (regions == NULL) {
		syslog(LOG_ERR, "no canary regions could be created");
		return (1);
	}

	syslog(LOG_INFO, "monitoring %zu byte regions every %u seconds",
	    region_size, interval);

	while (!quitting) {
		FILE *fp = NULL;
		uint_t count = 0;

		for (r = regions; r != NULL; r = r->r_next)
			count += verify_region(r, &fp);

		if (fp != NULL) {
			(void) fflush(fp);
			(void) fsync(fileno(fp));
			(void) fclose(fp);
		}
		if (count > 0 && abort_on_trip)
			abort();

		(void) sleep(interval);
	}

	for (r = regions; r != NULL; r = r->r_next) {
		if (r->r_fd != -1) {
			(void) close(r->r_fd);
			(void) unlink(r->r_path);
		}
	}

	return (0);
}
