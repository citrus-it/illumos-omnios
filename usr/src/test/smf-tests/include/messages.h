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

#ifndef _MESSAGES_H
#define	_MESSAGES_H

#include <stdarg.h>

/*
 * Block comment which describes the contents of this file.
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef enum logtype {
	lt_progress = 1, /* Don't start at zero for 'paranoia' reasons */
	lt_info,
	lt_warn,
	lt_error
} logtype_t;

char *find_argv0(void);

/* async signal safety of routines? */
void log_progress(const char *message, ...);
void log_info(const char *message, ...);
void log_warn(const char *message, ...);
void log_error(const char *message, ...);
void log_service(const char *message, ...);
void log_monitor(const char *message, ...);

void log_setup(char *logname);
void log_close(void);


#ifdef __cplusplus
}
#endif

#endif /* _MESSAGES_H */
