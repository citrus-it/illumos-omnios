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
	boolean_t		(*msfd_callcheck)(uintptr_t);
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
	data->msfd_callcheck = NULL;

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
mdb_stack_frame_callcheck_set(mdb_stack_frame_hdl_t *datap,
    boolean_t (*cb)(uintptr_t))
{
	mdb_stack_frame_data_t *data = datap;

	data->msfd_callcheck = cb;
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
	uint_t nargc = MIN(argc, data->msfd_arglim);
	mdb_ctf_id_t argtypes[nargc];
	mdb_ctf_funcinfo_t mfp;
	boolean_t ffound, ctf;
	mdb_syminfo_t sip;
	uintptr_t npc;
	char buf[32];
	GElf_Sym s;
	uint_t i;

	/*
	 * Attempt to find a symbol for this address. If we aren't successful,
	 * it is possible that we have called a function that the compiler
	 * knows will never return from right at the end of our function. If
	 * that's the case, 'pc' will be pointing just past the end of the
	 * function. If we don't manage to resolve the symbol at 'pc', we'll
	 * try a few addresses before too.
	 *
	 * There is an additional wrinkle which is that we might be pointing
	 * at the first address of the next function. DWARFv5 has a new
	 * DW_AT_noreturn attribute that we can eventually use to set a flag in
	 * the CTF data to detect functions that don't return. For now (and to
	 * support the case where we don't have CTF) we use some additional
	 * heuristics -- if:
	 *  - the address' symbol is different to the symbol of the immediately
	 *    prior address - this means that we are on a symbol boundary and
	 *    may be in the situation described above, and
	 *  - the previous instruction is identified as a 'call' by an optional
	 *    target-specific callback
	 * then we assume that we're in one of these cases and adjust.
	 */
	ffound = ctf = B_FALSE;
	for (npc = pc; npc > 0 && pc - npc < 4; npc--) {
		if (mdb_tgt_lookup_by_addr(data->msfd_tgt, npc,
		    MDB_TGT_SYM_FUZZY, NULL, 0, &s, &sip) == 0) {
			mdb_syminfo_t lsip;
			GElf_Sym ls;

			if (npc != pc || npc == 0) {
				ffound = B_TRUE;
				break;
			}

			/*
			 * Look up the symbol at the address prior and compare.
			 */
			if (mdb_tgt_lookup_by_addr(data->msfd_tgt,
			    npc - 1, MDB_TGT_SYM_FUZZY, NULL, 0,
			    &ls, &lsip) != 0) {
				ffound = B_TRUE;
				break;
			}
			if (sip.sym_id == lsip.sym_id) {
				/*
				 * Not a symbol boundary, go with what we found.
				 */
				ffound = B_TRUE;
				break;
			}
			/*
			 * This implies we're on a symbol boundary. If a
			 * callback was provided, use it as an extra check that
			 * the previous instruction is a likely call to a
			 * noreturn function.
			 */
			if (data->msfd_callcheck == NULL ||
			    !data->msfd_callcheck(pc)) {
				ffound = B_TRUE;
				break;
			}
		}
	}

	if (ffound) {
		if (mdb_ctf_func_info(&s, &sip, &mfp) == 0)
			ctf = B_TRUE;
	} else {
		npc = pc;
	}

	if (data->msfd_flags & MSF_SIZES) {
		if (data->msfd_lastbp != 0)
			mdb_printf("[%4lr] ", bp - data->msfd_lastbp);
		else
			mdb_printf("%7s", "");
		data->msfd_lastbp = bp;
	}

	if (data->msfd_flags & MSF_VERBOSE)
		mdb_printf("%0?lr ", bp);

	if (ctf && (data->msfd_flags & MSF_TYPES)) {
		if (mdb_ctf_type_name(mfp.mtf_return,
		    buf, sizeof (buf)) != NULL) {
			mdb_printf("%s ", buf);
		}
	}

	if (npc != pc)
		mdb_printf("~");
	mdb_printf("%a(", npc);

	if (ctf && (data->msfd_flags & MSF_TYPES)) {
		if (mdb_ctf_func_args(&mfp, nargc, argtypes) != 0)
			ctf = B_FALSE;
	}

	for (i = 0; i < nargc; i++) {
		if (i > 0)
			mdb_printf(", ");
		if (ctf && (data->msfd_flags & MSF_TYPES) &&
		    mdb_ctf_type_name(argtypes[i],
		    buf, sizeof (buf)) != NULL) {
			switch (mdb_ctf_type_kind(argtypes[i])) {
			case CTF_K_POINTER:
				if (argv[i] == 0)
					mdb_printf("(%s)NULL", buf);
				else
					mdb_printf("(%s)%lr", buf, argv[i]);
				break;
			case CTF_K_ENUM: {
				const char *cp;

				cp = mdb_ctf_enum_name(argtypes[i], argv[i]);
				if (cp != NULL)
					mdb_printf("(%s)%s", buf, cp);
				else
					mdb_printf("(%s)%lr", buf, argv[i]);
				break;
			}
			default:
				mdb_printf("(%s)%lr", buf, argv[i]);
			}
		} else {
			mdb_printf("%lr", argv[i]);
		}
	}

	mdb_printf(")\n");
}
