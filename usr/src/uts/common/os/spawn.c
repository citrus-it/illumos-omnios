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
 * This file contains the spawn(2) implementation. A private system call used
 * to implement posix_spawn(3C).
 */

#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/param.h>
#include <sys/proc.h>
#include <sys/types.h>
#include <sys/sysmacros.h>

int
spawn(void)
{
	return (EINVAL);
}

