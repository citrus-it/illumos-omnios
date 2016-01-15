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

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>

#include <libgen.h>

/*
 * NAME
 * 	name_gen - Generate a random name of a specified length made up
 * 	of basic characters.
 *
 * SYNOPSIS
 * 	name_gen <x>
 *
 * DESCRIPTION
 * 	This program will take a size and then genarate a "random" name
 * 	based on a seed value of time.
 *
 * OPTION
 * 	<x> size of the name to be generated.
 */

int
main(int argc, char *argv[]) {
	char	*name, c;
	int	len, i;
	int	seed;

	/*
	 * Seed with the current hires time.
	 */
	seed = gethrtime();
	srand48(seed);

	if (argc != 2) {
		(void) printf("Must specify a length\n");
		exit(1);
	}

	len = atoi(argv[1]);

	if ((name = malloc(len)) == NULL) {
		return (1);
	}
	for (i = 0; i < len; i++) {
		c = lrand48() & 0xff;
		while (c < 'a' || c > 'z')
			c = lrand48() & 0xff;

		name[i] = c;
	}

	(void) printf("%s\n", name);

	free(name);

	return (0);
}
