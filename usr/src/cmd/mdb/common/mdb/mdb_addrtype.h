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
 * Copyright 2025 Oxide Computer Company
 */

#ifndef	_MDB_ADDRTYPE_H
#define	_MDB_ADDRTYPE_H

#include <mdb/mdb_modapi.h>
#include <mdb/mdb_ctf.h>

#ifdef	__cplusplus
extern "C" {
#endif

typedef enum mdb_addrtype_flag {
	ADDRTYPE_AUTO		= 1 << 0,
	ADDRTYPE_MANUAL		= 1 << 1
} mdb_addrtype_flag_t;

extern int mdb_addrtype_add(const uintptr_t, const char *, mdb_addrtype_flag_t);
extern int mdb_addrtype_addid(const uintptr_t, mdb_ctf_id_t,
    mdb_addrtype_flag_t);
extern int mdb_addrtype_lookup(const uintptr_t, mdb_ctf_id_t *);

extern int cmd_addrtype(uintptr_t, uint_t, int, const mdb_arg_t *);
extern void cmd_addrtype_help(void);

#ifdef	__cplusplus
}
#endif

#endif	/* _MDB_ADDRTYPE_H */
