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
 * Copyright 2021 Oxide Computer Company
 */

#ifndef _KMDB_MODEXT_H
#define	_KMDB_MODEXT_H

/*
 * Extensions to the module API for built in modules.
 */

#ifdef __cplusplus
extern "C" {
#endif

extern int mdb_x86_rdmsr(uint32_t, uint64_t *);
extern int mdb_x86_wrmsr(uint32_t, uint64_t);

#ifdef __cplusplus
}
#endif

#endif /* _KMDB_MODEXT_H */
