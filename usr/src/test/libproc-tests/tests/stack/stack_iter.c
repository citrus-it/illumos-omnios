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

/*
 * This program verifies that Pstack_iter() reports the correct arguments for
 * the frames of a 32-bit process, in particular for functions which realign
 * their stack and so do not have their arguments adjacent to their frame. It
 * operates alongside the target built from stack_target.c and stack_frames.S.
 * The target is run until it reaches the function 'stk_hook()', at which
 * point every function of interest has a frame on its stack.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <err.h>
#include <libproc.h>
#include <sys/wait.h>
#include <sys/sysmacros.h>

#include "stack_target.h"

static uint_t stk_timeout = 5 * 1000; /* 5s in ms */

typedef struct {
	const char *sf_name;
	uint_t sf_nargs;
	long sf_args[STK_NARGS];
	uint_t sf_seen;
	bool sf_pass;
} stk_frame_t;

#define	STK_FRAME(name, id) \
	{ name, STK_NARGS, \
	{ STK_ARG(id, 0), STK_ARG(id, 1), STK_ARG(id, 2) }, 0, false }

/*
 * The functions that we expect to find on the stack of the target along with
 * the arguments that each was called with. main() is included as the compiler
 * is likely to have generated a prologue for it which realigns the stack.
 */
static stk_frame_t stk_frames[] = {
	{ "main", 1, { STK_ARGC }, 0, false },
	STK_FRAME("stk_plain", STK_ID_PLAIN),
	STK_FRAME("stk_realign", STK_ID_REALIGN),
	STK_FRAME("stk_realign_regs", STK_ID_REALIGN_REGS),
	STK_FRAME("stk_realign_sched", STK_ID_REALIGN_SCHED),
	STK_FRAME("stk_realign64", STK_ID_REALIGN64)
};

static int
stk_frame_cb(void *arg, prgregset_t regs, uint_t argc, const long *argv)
{
	struct ps_prochandle *P = arg;
	char name[64];
	GElf_Sym sym;

	if (Plookup_by_addr(P, regs[R_PC], name, sizeof (name), &sym) != 0)
		return (0);

	for (uint_t i = 0; i < ARRAY_SIZE(stk_frames); i++) {
		stk_frame_t *sf = &stk_frames[i];

		if (strcmp(name, sf->sf_name) != 0)
			continue;

		sf->sf_seen++;
		sf->sf_pass = true;

		if (argc < sf->sf_nargs) {
			warnx("TEST FAILED: %s has %u arguments, expected at "
			    "least %u", name, argc, sf->sf_nargs);
			sf->sf_pass = false;
			break;
		}

		for (uint_t j = 0; j < sf->sf_nargs; j++) {
			if (argv[j] == sf->sf_args[j])
				continue;
			warnx("TEST FAILED: %s argument %u is 0x%lx, expected "
			    "0x%lx", name, j, argv[j], sf->sf_args[j]);
			sf->sf_pass = false;
		}
		break;
	}

	return (0);
}

static bool
stk_check(struct ps_prochandle *P)
{
	bool ret = true;

	if (Pstack_iter(P, Pstatus(P)->pr_lwp.pr_reg, stk_frame_cb, P) != 0) {
		warnx("TEST FAILED: Pstack_iter() failed");
		return (false);
	}

	for (uint_t i = 0; i < ARRAY_SIZE(stk_frames); i++) {
		const stk_frame_t *sf = &stk_frames[i];

		if (sf->sf_seen != 1) {
			warnx("TEST FAILED: found %u frames for %s, expected 1",
			    sf->sf_seen, sf->sf_name);
			ret = false;
		} else if (!sf->sf_pass) {
			ret = false;
		} else {
			(void) printf("TEST PASSED: found the expected "
			    "arguments for %s\n", sf->sf_name);
		}
	}

	return (ret);
}

int
main(int argc, char **argv)
{
	int ret = EXIT_SUCCESS, perr, wstat;
	struct ps_prochandle *P;
	char *targv[STK_ARGC + 1];
	GElf_Sym sym;
	ulong_t bkpt;
	pid_t pid;

	if (argc != 2)
		errx(EXIT_FAILURE, "missing required program to examine");

	targv[0] = argv[1];
	for (uint_t i = 1; i < STK_ARGC; i++)
		targv[i] = "arg";
	targv[STK_ARGC] = NULL;

	P = Pcreate(argv[1], targv, &perr, NULL, 0);
	if (P == NULL) {
		errx(EXIT_FAILURE, "failed to create %s: %s (0x%x)", argv[1],
		    Pcreate_error(perr), perr);
	}

	(void) Punsetflags(P, PR_RLC);
	if (Psetflags(P, PR_KLC | PR_BPTADJ) != 0) {
		int e = errno;
		Prelease(P, PRELEASE_KILL);
		errc(EXIT_FAILURE, e, "failed to set PR_KLC | PR_BPTADJ flags");
	}

	if (Pxlookup_by_name(P, LM_ID_BASE, PR_OBJ_EXEC, "stk_hook", &sym,
	    NULL) != 0) {
		err(EXIT_FAILURE, "failed to find stk_hook symbol");
	}

	pid = Ppsinfo(P)->pr_pid;

	if (Pfault(P, FLTBPT, 1) != 0)
		errx(EXIT_FAILURE, "failed to set the FLTBPT disposition");

	if (Psetbkpt(P, sym.st_value, &bkpt) != 0) {
		err(EXIT_FAILURE, "failed to set breakpoint on stk_hook "
		    "(0x%" PRIx64 ")", sym.st_value);
	}

	if (Psetrun(P, 0, 0) != 0)
		err(EXIT_FAILURE, "failed to resume running our target");

	if (Pwait(P, stk_timeout) != 0) {
		err(EXIT_FAILURE, "%s did not hit our expected breakpoint",
		    argv[1]);
	}

	if (!stk_check(P))
		ret = EXIT_FAILURE;

	if (Pdelbkpt(P, sym.st_value, bkpt) != 0)
		err(EXIT_FAILURE, "failed to delete breakpoint");

	if (Psetrun(P, 0, PRCFAULT) != 0)
		err(EXIT_FAILURE, "failed to resume running our target");

	if (waitpid(pid, &wstat, 0) != pid) {
		err(EXIT_FAILURE, "failed to get our %s's (%" _PRIdID "), "
		    "wait info", argv[1], pid);
	}

	if (WIFEXITED(wstat) == 0)
		errx(EXIT_FAILURE, "%s didn't actually exit!", argv[1]);

	if (WEXITSTATUS(wstat) != 0) {
		errx(EXIT_FAILURE, "%s failed with 0x%x", argv[1],
		    WEXITSTATUS(wstat));
	}

	(void) printf("TEST PASSED: target self-verification passed\n");

	if (ret == EXIT_SUCCESS)
		(void) printf("All tests passed successfully\n");

	return (ret);
}
