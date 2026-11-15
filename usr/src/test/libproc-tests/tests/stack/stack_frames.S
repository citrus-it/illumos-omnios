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

	.file	"stack_frames.s"

/*
 * This provides a chain of 32-bit functions for stack_iter.c to find on the
 * stack of the target program. They are written in assembly so that the
 * layout of each frame is known and does not depend on the compiler. Most of
 * them begin with the prologue that GCC generates for a function which has to
 * realign its stack, and they differ in the registers that they save and in
 * the amount of padding that the realignment introduces.
 */

#include <sys/asm_linkage.h>
#include "stack_target.h"

/*
 * Call a function with the three arguments that it expects. 'pad' is the
 * amount of space to leave above those arguments, and it determines the
 * alignment of the stack on entry to the function. The stack must be aligned
 * to at least 16 bytes when this is used. The stack is restored with a single
 * addl following the call, which is what libproc looks for in order to work
 * out how many arguments were passed.
 */
#define	STK_CALL(func, id, pad)			\
	subl	$pad, %esp;			\
	pushl	$STK_ARG(id, 2);		\
	pushl	$STK_ARG(id, 1);		\
	pushl	$STK_ARG(id, 0);		\
	call	func;				\
	addl	$(pad + 12), %esp

/*
 * int stk_plain(int, int, int)
 *
 * This has a conventional frame. It aligns the stack so that the functions
 * below are entered with a known alignment regardless of how main() left it.
 * stk_realign() is entered with the stack 12 bytes beyond a 16 byte boundary.
 */
	ENTRY_NP(stk_plain)
	pushl	%ebp
	movl	%esp, %ebp
	andl	$-16, %esp
	STK_CALL(stk_realign, STK_ID_REALIGN, 4)
	leave
	ret
	SET_SIZE(stk_plain)

/*
 * int stk_realign(int, int, int)
 *
 * This realigns its stack, which needs 12 bytes of padding, and saves only
 * %ecx. stk_realign_regs() is entered with the stack already aligned.
 */
	ENTRY_NP(stk_realign)
	leal	0x4(%esp), %ecx
	andl	$-16, %esp
	pushl	-0x4(%ecx)
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%ecx
	subl	$4, %esp
	STK_CALL(stk_realign_regs, STK_ID_REALIGN_REGS, 0)
	movl	-0x4(%ebp), %ecx
	leave
	leal	-0x4(%ecx), %esp
	ret
	SET_SIZE(stk_realign)

/*
 * int stk_realign_regs(int, int, int)
 *
 * This realigns its stack, which needs no padding, and saves all of the
 * callee-saved registers ahead of %ecx. stk_realign_sched() is entered with
 * the stack 8 bytes beyond a 16 byte boundary.
 */
	ENTRY_NP(stk_realign_regs)
	leal	0x4(%esp), %ecx
	andl	$-16, %esp
	pushl	-0x4(%ecx)
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%edi
	pushl	%esi
	pushl	%ebx
	pushl	%ecx
	subl	$8, %esp
	STK_CALL(stk_realign_sched, STK_ID_REALIGN_SCHED, 8)
	leal	-0x10(%ebp), %esp
	popl	%ecx
	popl	%ebx
	popl	%esi
	popl	%edi
	popl	%ebp
	leal	-0x4(%ecx), %esp
	ret
	SET_SIZE(stk_realign_regs)

/*
 * int stk_realign_sched(int, int, int)
 *
 * This realigns its stack, which needs 8 bytes of padding. Another
 * instruction appears among those which save the registers, as happens when
 * the compiler schedules an instruction from the body of a function early.
 * stk_realign64() is entered with the stack 60 bytes beyond a 64 byte
 * boundary.
 */
	ENTRY_NP(stk_realign_sched)
	leal	0x4(%esp), %ecx
	andl	$-16, %esp
	pushl	-0x4(%ecx)
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%edi
	xorl	%edi, %edi
	pushl	%ebx
	pushl	%ecx
	andl	$-64, %esp
	STK_CALL(stk_realign64, STK_ID_REALIGN64, 52)
	leal	-0xc(%ebp), %esp
	popl	%ecx
	popl	%ebx
	popl	%edi
	popl	%ebp
	leal	-0x4(%ecx), %esp
	ret
	SET_SIZE(stk_realign_sched)

/*
 * int stk_realign64(int, int, int)
 *
 * This realigns its stack to 64 bytes, which needs 60 bytes of padding.
 */
	ENTRY_NP(stk_realign64)
	leal	0x4(%esp), %ecx
	andl	$-64, %esp
	pushl	-0x4(%ecx)
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%ecx
	subl	$4, %esp
	STK_CALL(stk_last, STK_ID_LAST, 0)
	movl	-0x4(%ebp), %ecx
	leave
	leal	-0x4(%ecx), %esp
	ret
	SET_SIZE(stk_realign64)

/*
 * int stk_last(int, int, int)
 *
 * This has a conventional frame and exists only to call stk_hook(). When the
 * target stops on entry to stk_hook(), the frame at the top of the stack is
 * this one, and so a stack trace attributes it to stk_hook() and does not
 * include this function.
 */
	ENTRY_NP(stk_last)
	pushl	%ebp
	movl	%esp, %ebp
	call	stk_hook
	popl	%ebp
	ret
	SET_SIZE(stk_last)

/*
 * int stk_hook(void)
 *
 * This is where stack_iter.c places its breakpoint.
 */
	ENTRY_NP(stk_hook)
	movl	$STK_RETVAL, %eax
	ret
	SET_SIZE(stk_hook)
