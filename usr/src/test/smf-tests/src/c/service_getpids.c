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
 * Get the pids of the service processes
 */

#include <stdio.h>
#include <messages.h>
#include <state.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>

int
main(int argc, char **argv)
{
	int offset;
	char *printed = "";

	if (read_switches(&argc, argv)) {
		return (1);
	}

	if (saved->service_count == 0) {
		return (1);
	}

	for (offset = 0; offset < saved->service_count; offset++) {
		(void) printf("%s%ld", printed, saved->service_pid[offset]);
		if (*printed == '\0') printed = " ";
	}
	(void) printf("\n");

	return (0);
}
