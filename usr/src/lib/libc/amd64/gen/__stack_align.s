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
 * Copyright 2020 OmniOS Community Edition (OmniOSce) Association.
 */

	.file	"__stack_align.s"

#include "SYS.h"

	ENTRY(__stack_align)
	pop	%rcx			/* save return address */
	andq	$-STACK_ALIGN, %rsp	/* adjust stack alignment */
	jmp	*%rcx			/* return */
	SET_SIZE(__stack_align)
