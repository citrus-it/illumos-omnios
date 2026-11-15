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

#ifndef _STACK_TARGET_H
#define	_STACK_TARGET_H

/*
 * Definitions shared between stack_iter.c and the target program that it
 * examines, which is built from stack_target.c and stack_frames.S.
 */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * The number of arguments, including its name, that the target is run with.
 */
#define	STK_ARGC	4

/*
 * Each function in the target takes three arguments. Every argument has a
 * different value which is derived from an identifier for the function and
 * the position of the argument.
 */
#define	STK_NARGS	3
#define	STK_ARG(id, pos)	(((id) << 8) | ((pos) + 1))

#define	STK_ID_PLAIN		1
#define	STK_ID_REALIGN		2
#define	STK_ID_REALIGN_REGS	3
#define	STK_ID_REALIGN_SCHED	4
#define	STK_ID_REALIGN64	5
#define	STK_ID_LAST		6

/*
 * The value which stk_hook() returns, and which each of its callers passes
 * back to its own caller.
 */
#define	STK_RETVAL		0x57ac

#ifndef _ASM
extern int stk_plain(int, int, int);
#endif

#ifdef __cplusplus
}
#endif

#endif /* _STACK_TARGET_H */
