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
 * This program serves as a target for stack_iter.c to examine. It calls into
 * the chain of functions in stack_frames.S, the last of which is stk_hook()
 * where stack_iter.c places a breakpoint. Once it has been resumed, it
 * checks that the stack has unwound correctly.
 */

#include <stdlib.h>
#include <err.h>

#include "stack_target.h"

int
main(int argc, char **argv)
{
	int ret;

	if (argc != STK_ARGC)
		errx(EXIT_FAILURE, "expected to be run by stack_iter");

	ret = stk_plain(STK_ARG(STK_ID_PLAIN, 0), STK_ARG(STK_ID_PLAIN, 1),
	    STK_ARG(STK_ID_PLAIN, 2));
	if (ret != STK_RETVAL) {
		errx(EXIT_FAILURE, "TEST FAILED: %s: stk_plain() returned "
		    "0x%x, expected 0x%x", argv[0], ret, STK_RETVAL);
	}

	return (EXIT_SUCCESS);
}
