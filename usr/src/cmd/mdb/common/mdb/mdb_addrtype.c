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

#include <mdb/mdb_modapi.h>
#include <mdb/mdb_err.h>
#include <mdb/mdb_ctf.h>
#include <mdb/mdb_ctf_impl.h>
#include <mdb/mdb_list.h>
#include <mdb/mdb_print.h>
#include <mdb/mdb_nv.h>
#include <mdb/mdb_string.h>
#include <mdb/mdb_addrtype.h>
#include <mdb/mdb.h>
#include <mdb/mdb_debug.h>

#include <sys/stdbool.h>
#include <sys/sysmacros.h>
#include <errno.h>

typedef struct {
	mdb_list_t mcsa_list;
	uintptr_t mcsa_addr;
	mdb_ctf_impl_t mcsa_id;
} mdb_ctf_addrtype_t;

/*
 * Some types are only known by their opaque handles. To assist with inspecting
 * these common types we keep a translation table to map from opaque handles
 * to their underlying structures.
 */
struct {
	char		*att_type_name;		/* filled in statically */
	char		*att_actual_name;	/* filled in statically */
	mdb_ctf_id_t	att_type;		/* determined dynamically */
	mdb_ctf_id_t	att_actual_type;	/* determined dynamically */
} at_typetab[] = {
	{ "dev_info_t",		"struct dev_info" },
	{ "ddi_dma_handle_t",	"ddi_dma_impl_t *" },
};

static void
mdb_addrtype_typetab_init(void)
{
	uint_t i;

	for (i = 0; i < ARRAY_SIZE(at_typetab); i++) {
		if (mdb_ctf_lookup_by_name(at_typetab[i].att_type_name,
		    &at_typetab[i].att_type) == -1) {
			mdb_warn("can't find type '%s'\n",
			    at_typetab[i].att_type_name);
			mdb_ctf_type_invalidate(&at_typetab[i].att_type);
			continue;
		}

		if (mdb_ctf_lookup_by_name(at_typetab[i].att_actual_name,
		    &at_typetab[i].att_actual_type) == -1) {
			mdb_warn("can't find type '%s'\n",
			    at_typetab[i].att_actual_name);
			mdb_ctf_type_invalidate(&at_typetab[i].att_actual_type);
		}
	}
}

static int
mdb_addrtype_find(const uintptr_t addr, mdb_ctf_addrtype_t **ap)
{
	mdb_ctf_addrtype_t *a;

	for (a = mdb_list_next(&mdb.m_addrtype); a != NULL;
	    a = mdb_list_next(a)) {
		if (a->mcsa_addr == addr) {
			*ap = a;
			return (0);
		}
	}

	*ap = NULL;
	return (set_errno(ENOENT));
}

int
mdb_addrtype_lookup(const uintptr_t addr, mdb_ctf_id_t *p)
{
	mdb_ctf_impl_t *mcip = (mdb_ctf_impl_t *)p;
	mdb_ctf_addrtype_t *a;

	if (mdb_addrtype_find(addr, &a) == 0) {
		*mcip = a->mcsa_id;
		return (0);
	}

	mdb_ctf_type_invalidate(p);
	errno = ENOENT;
	return (-1);
}

int
mdb_addrtype_addid(const uintptr_t addr, mdb_ctf_id_t id,
    mdb_addrtype_flag_t flags)
{
	mdb_ctf_impl_t *mcip = (mdb_ctf_impl_t *)&id;
	mdb_ctf_addrtype_t *a;
	static bool initdone = false;
	uint_t i;

	if (!initdone) {
		mdb_addrtype_typetab_init();
		initdone = true;
	}

	if (addr == 0)
		return (0);

	for (i = 0; i < ARRAY_SIZE(at_typetab); i++) {
		if (mdb_ctf_type_cmp(id, at_typetab[i].att_type) == 0) {
			id = at_typetab[i].att_actual_type;
			break;
		}
	}

	if (mdb_addrtype_find(addr, &a) == 0) {
		a->mcsa_id = *mcip;
	} else {
		a = mdb_alloc(sizeof (*a), UM_SLEEP);
		a->mcsa_addr = addr;
		a->mcsa_id = *mcip;
		mdb_list_append(&mdb.m_addrtype, a);
	}

	return (0);
}

int
mdb_addrtype_add(const uintptr_t addr, const char *type,
    mdb_addrtype_flag_t flags)
{
	mdb_ctf_id_t id;

	if (mdb_ctf_lookup_by_name(type, &id) == -1) {
		mdb_warn("couldn't find type %s", type);
		return (-1);
	}

	return (mdb_addrtype_addid(addr, id, flags));
}

static int
cmd_addrtype_delete(const uintptr_t addr)
{
	mdb_ctf_addrtype_t *a;

	if (mdb_addrtype_find(addr, &a) == 0)
		mdb_list_delete(&mdb.m_addrtype, a);

	return (DCMD_OK);
}

static int
cmd_addrtype_flush(void)
{
	mdb_ctf_addrtype_t *a;

	while ((a = mdb_list_next(&mdb.m_addrtype)) != NULL)
		mdb_list_delete(&mdb.m_addrtype, a);

	return (DCMD_OK);
}

static int
cmd_addrtype_list(uintptr_t addr, uint_t flags)
{
	mdb_ctf_addrtype_t *a;

	if (!(flags & DCMD_PIPE_OUT) && DCMD_HDRSPEC(flags))
		mdb_printf("%<u>%?s %s%</u>\n", "ADDRESS", "TYPE");

	for (a = mdb_list_next(&mdb.m_addrtype); a != NULL;
	    a = mdb_list_next(a)) {
		char buf[MDB_SYM_NAMLEN];
		const char *name;

		if ((flags & DCMD_ADDRSPEC) && a->mcsa_addr != addr)
			continue;

		name = ctf_type_name(a->mcsa_id.mci_fp, a->mcsa_id.mci_id,
		    buf, sizeof (buf));
		if (name == NULL) {
			(void) strlcpy(buf, "?", sizeof (buf));
			name = buf;
		}
		if (flags & DCMD_PIPE_OUT)
			mdb_printf("%lr\n", a->mcsa_addr);
		else
			mdb_printf("%0?p %s\n", a->mcsa_addr, name);
	}

	return (DCMD_OK);
}

int
cmd_addrtype(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	mdb_ctf_id_t id;
	bool delete = false;
	int i;

	i = mdb_getopts(argc, argv,
	    'd', MDB_OPT_SETBITS, true, &delete,
	    NULL);

	argc -= i;
	argv += i;

	if (delete) {
		if (argc > 0)
			return (DCMD_USAGE);
		if (flags & DCMD_ADDRSPEC)
			return (cmd_addrtype_delete(addr));
		return (cmd_addrtype_flush());
	}

	if (argc != 0) {
		char type[MDB_SYM_NAMLEN];
		int ret;

		if (!(flags & DCMD_ADDRSPEC) || argv->a_type != MDB_TYPE_STRING)
			return (DCMD_USAGE);

		ret = args_to_typename(&argc, &argv, type, sizeof (type));
		if (ret != 0)
			return (ret);

		if (mdb_addrtype_add(addr, type, ADDRTYPE_MANUAL) != 0)
			return (DCMD_ERR);
		return (DCMD_OK);
	}

	return (cmd_addrtype_list(addr, flags));
}

void
cmd_addrtype_help(void)
{
	mdb_printf("Manage type-tagged addresses.\n");
	mdb_printf("\n");
	(void) mdb_dec_indent(2);
	mdb_printf("%<b>OPTIONS%</b>\n");
	(void) mdb_inc_indent(2);
	mdb_printf("\t-d\tremove a specific entry, or all entries.\n");
	mdb_printf("\n");
	(void) mdb_dec_indent(2);
	mdb_printf("%<b>EXAMPLES%</b>\n");
	(void) mdb_inc_indent(2);
	mdb_printf("TBC\n");
}
