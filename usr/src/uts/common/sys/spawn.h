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
 * Copyright 2014 Garrett D'Amore <garrett@damore.org>
 *
 * Copyright 2009 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * Copyright (c) 2011 by Delphix. All rights reserved.
 * Copyright 2026 Oxide Computer Company
 */

#ifndef _SYS_SPAWN_H
#define	_SYS_SPAWN_H

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * flags for posix_spawnattr_setflags()
 */
#define	POSIX_SPAWN_RESETIDS		0x0001
#define	POSIX_SPAWN_SETPGROUP		0x0002
#define	POSIX_SPAWN_SETSIGDEF		0x0004
#define	POSIX_SPAWN_SETSIGMASK		0x0008
#define	POSIX_SPAWN_SETSCHEDPARAM	0x0010
#define	POSIX_SPAWN_SETSCHEDULER	0x0020
#define	POSIX_SPAWN_SETSID		0x0040
/*
 * non-portable extensions
 */
#if !defined(_STRICT_POSIX) || defined(_KERNEL)
#define	POSIX_SPAWN_SETSIGIGN_NP	0x0800
#define	POSIX_SPAWN_NOSIGCHLD_NP	0x1000
#define	POSIX_SPAWN_WAITPID_NP		0x2000
#define	POSIX_SPAWN_NOEXECERR_NP	0x4000
#endif	/* !_STRICT_POSIX || defined(_KERNEL) */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_SPAWN_H */
