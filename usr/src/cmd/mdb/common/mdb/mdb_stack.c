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

/*
 * Common code to help printing stack frames in a consistent way, and with
 * options to include frame size and type data where it can be retrieved from
 * CTF data.
 */

#include <sys/types.h>

#include <mdb/mdb_string.h>
#include <mdb/mdb_modapi.h>
#include <mdb/mdb_debug.h>
#include <mdb/mdb_ctf.h>
#include <mdb/mdb_stack.h>
#include <mdb/mdb.h>

typedef struct {
	mdb_tgt_t		*msfd_tgt;
	uint_t			msfd_arglim;
	mdb_stack_frame_flags_t	msfd_flags;
	uintptr_t		msfd_lastbp;
} mdb_stack_frame_data_t;

mdb_stack_frame_hdl_t *
mdb_stack_frame_init(mdb_tgt_t *tgt, uint_t arglim,
    mdb_stack_frame_flags_t flags)
{
	mdb_stack_frame_data_t *data;

	data = mdb_alloc(sizeof (*data), UM_SLEEP);
	if (data == NULL)
		return (NULL);
	data->msfd_tgt = tgt;
	data->msfd_arglim = arglim;
	data->msfd_flags = flags;
	data->msfd_lastbp = 0;

	return (data);
}

void
mdb_stack_frame_fini(mdb_stack_frame_hdl_t *datap)
{
	mdb_free(datap, sizeof (mdb_stack_frame_data_t));
}

uint_t
mdb_stack_frame_arglim(mdb_stack_frame_hdl_t *datap)
{
	mdb_stack_frame_data_t *data = datap;

	return (data->msfd_arglim);
}

void
mdb_stack_frame_flags_set(mdb_stack_frame_hdl_t *datap,
    mdb_stack_frame_flags_t flags)
{
	mdb_stack_frame_data_t *data = datap;

	data->msfd_flags |= flags;
}

void
mdb_stack_frame(mdb_stack_frame_hdl_t *datap, uintptr_t pc, uintptr_t bp,
    uint_t argc, const long *argv)
{
	mdb_stack_frame_data_t *data = datap;
	boolean_t types = B_FALSE;
	uint_t nargc = MIN(argc, data->msfd_arglim);
	mdb_ctf_id_t argtypes[nargc];
	char type[32];
	uint_t i;

	if (data->msfd_flags & MSF_SIZES) {
		if (data->msfd_lastbp != 0)
			mdb_printf("[%4lr] ", bp - data->msfd_lastbp);
		else
			mdb_printf("%7s", "");
		data->msfd_lastbp = bp;
	}

	if (data->msfd_flags & MSF_VERBOSE)
		mdb_printf("%0?lr ", bp);
	if (data->msfd_flags & MSF_TYPES) {
		mdb_ctf_funcinfo_t mfp;
		mdb_syminfo_t sip;
		GElf_Sym s;

		if (mdb_tgt_lookup_by_addr(data->msfd_tgt, pc,
		    MDB_TGT_SYM_FUZZY, NULL, 0, &s, &sip) == 0 &&
		    mdb_ctf_func_info(&s, &sip, &mfp) == 0 &&
		    mdb_ctf_type_name(mfp.mtf_return, type, sizeof (type)) !=
		    NULL) {
			mdb_printf("%s ", type);
			if (mdb_ctf_func_args(&mfp, nargc, argtypes) == 0)
				types = B_TRUE;
		}
	}
	mdb_printf("%a(", pc);

	for (i = 0; i < nargc; i++) {
		if (i > 0)
			mdb_printf(", ");
		if (types && mdb_ctf_type_name(argtypes[i],
		    type, sizeof (type)) != NULL) {
			switch (mdb_ctf_type_kind(argtypes[i])) {
			case CTF_K_POINTER:
				if (argv[i] == 0)
					mdb_printf("(%s)NULL", type);
				else
					mdb_printf("(%s)%lr", type, argv[i]);
				break;
			case CTF_K_ENUM: {
				const char *cp;

				cp = mdb_ctf_enum_name(argtypes[i], argv[i]);
				if (cp != NULL)
					mdb_printf("(%s)%s", type, cp);
				else
					mdb_printf("(%s)%lr", type, argv[i]);
				break;
			}
			default:
				mdb_printf("(%s)%lr", type, argv[i]);
			}
		} else {
			mdb_printf("%lr", argv[i]);
		}
	}

	mdb_printf(")\n");
}
