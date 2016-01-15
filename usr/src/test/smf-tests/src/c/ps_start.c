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
 * Copyright 2008 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * Takes a list of pids and outputs them in start time order.
 */

#include <sys/types.h>
#if defined(_FILE_OFFSET_BITS)
#undef _FILE_OFFSET_BITS
#endif
#define	_STRUCTURED_PROC 1
#include <sys/procfs.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>

struct ps_time {
	pid_t		pid;
	struct timespec	start;
};

int
pid_compare(const void * a, const void *b)
{
	struct ps_time *time1 = (struct ps_time *)a;
	struct ps_time *time2 = (struct ps_time *)b;

	if (time1->start.tv_sec > time2->start.tv_sec)
		return (1);
	if (time1->start.tv_sec < time2->start.tv_sec)
		return (-1);
	if (time1->start.tv_nsec > time2->start.tv_nsec)
		return (1);
	if (time1->start.tv_nsec < time2->start.tv_nsec)
		return (-1);
	return (0);
}

int
main(int argc, char **argv)
{
	int fd;
	char statfile[MAXPATHLEN];
	psinfo_t psinfo;
	struct ps_time	*infolist = malloc(2 * sizeof (struct ps_time));
	int ati = 0;
	int sti = 2;

	if (argc < 2) {
		(void) fprintf(stderr, "usage: %s <pid0> ... <pidn>\n", argv[0]);
		return (1);
	}

	while (--argc) {
		/* LINTED E_SEC_SPRINTF_UNBOUNDED_COPY */
		(void) sprintf(statfile, "/proc/%s/psinfo", argv[argc]);

		if (-1 == (fd = open(statfile, O_RDONLY))) {
			perror("open");
			continue;
		}

		if (sizeof (psinfo) != read(fd, &psinfo, sizeof (psinfo))) {
			perror("read");
			continue;
		}

		(void) close(fd);

		if (ati+1 >= sti) {
			sti *= 2;
			infolist = realloc(infolist,
			    sti * sizeof (struct ps_time));
		}
		infolist[ati].pid = atoi(argc[argv]);
		infolist[ati].start.tv_sec = psinfo.pr_start.tv_sec;
		infolist[ati].start.tv_nsec = psinfo.pr_start.tv_nsec;
		ati += 1;
	}
	qsort(infolist, ati, sizeof (struct ps_time), pid_compare);

	for (sti = 0; sti < ati; sti++) {
		if (sti > 0)
			(void) printf(" ");
		(void) printf("%ld", infolist[sti].pid);
	}
	(void) printf("\n");

	return (0);
}
