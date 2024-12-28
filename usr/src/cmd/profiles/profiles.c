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
 * Copyright (c) 1999, 2010, Oracle and/or its affiliates. All rights reserved.
 * Copyright 2022 OmniOS Community Edition (OmniOSce) Association.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <libintl.h>
#include <locale.h>
#include <deflt.h>
#include <user_attr.h>
#include <prof_attr.h>
#include <exec_attr.h>
#include <auth_attr.h>
#include <sys/ccompile.h>

#define	TMP_BUF_LEN	2048		/* size of temp string buffer */

#define	PRINT_DEFAULT	0x0000
#define	PRINT_NAME	0x0010
#define	PRINT_LONG	0x0020
#define	PRINT_VERBOSE	0x0040

#ifndef TEXT_DOMAIN			/* Should be defined by cc -D */
#define	TEXT_DOMAIN	"SYS_TEST"
#endif

#define	AUTH_REQUIRED	"Authentication required"

typedef struct {
	uint_t cnt;
	int print_flag;
	bool auth;
} callback_t;

static void usage(int) __NORETURN;
static void show_profs(const char *, const char *, int, int);
static void print_profs_long(execattr_t *, callback_t *);
static void print_profs_short(execattr_t *, callback_t *);
static void print_profile_privs(kva_t *);

static char *progname = "profiles";

int
main(int argc, char *argv[])
{
	extern int	optind;
	char		*cmd = NULL;
	int		c;
	int		print_flag = PRINT_DEFAULT;
	int		search_flag = GET_ALL;

	(void) setlocale(LC_ALL, "");
	(void) textdomain(TEXT_DOMAIN);

	while ((c = getopt(argc, argv, "c:hlvxX")) != EOF) {
		switch (c) {
		case 'c':
			cmd = optarg;
			break;
		case 'h':
			usage(EXIT_SUCCESS);
		case 'l':
			print_flag |= PRINT_LONG;
			break;
		case 'v':
			print_flag |= PRINT_VERBOSE;
			break;
		case 'x':
			search_flag |= GET_AUTHPROF;
			break;
		case 'X':
			search_flag |= GET_PROF;
			break;
		default:
			usage(EXIT_FAILURE);
		}
	}
	argc -= optind;
	argv += optind;

	if (IS_GET_PROF(search_flag) && IS_GET_AUTHPROF(search_flag)) {
		(void) fprintf(stderr, gettext(
		    "-x and -X may not be used together.\n"));
		usage(EXIT_FAILURE);
	}

	if (*argv == NULL) {
		show_profs(NULL, cmd, print_flag, search_flag);
	} else {
		print_flag |= PRINT_NAME;
		do {
			(void) printf("%s:", *argv);
			(void) printf("\n");
			show_profs(*argv, cmd, print_flag, search_flag);
			if (argv[1] != NULL) {
				/* seperate users with empty line */
				(void) printf("\n");
			}
		} while (*(++argv) != NULL);
	}

	return (EXIT_SUCCESS);
}

static int
show_profs_callback(const char *prof, kva_t *pa, void *callbackp,
    void *arg __unused)
{
	char *indent = "";
	callback_t *call = callbackp;

	call->cnt++;

	if ((call->print_flag & PRINT_NAME))
		indent = "          ";

	if (call->auth && (call->print_flag & PRINT_VERBOSE)) {
		(void) printf("%s%s (%s)", indent, prof,
		    gettext(AUTH_REQUIRED));
	} else {
		(void) printf("%s%s", indent, prof);
	}
	print_profile_privs(pa);
	(void) printf("\n");

	return (0);
}

static void
show_profs(const char *username, const char *cmd, int print_flag,
    int search_flag)
{
	struct passwd *pw;
	execattr_t *exec;
	callback_t call = {
		.print_flag = print_flag,
		.cnt = 0
	};

	if (username == NULL) {
		if ((pw = getpwuid(getuid())) == NULL) {
			(void) fprintf(stderr, "%s: ", progname);
			(void) fprintf(stderr, gettext("No passwd entry\n"));
			return;
		}
		username = pw->pw_name;
	} else if (getpwnam(username) == NULL) {
		(void) fprintf(stderr, "%s: %s: ", progname, username);
		(void) fprintf(stderr, gettext("No such user\n"));
		return;
	}

	if ((print_flag & PRINT_LONG) || cmd != NULL) {
		if (!IS_GET_PROF(search_flag)) {
			call.auth = true;
			exec = getexecuser(username, KV_COMMAND, cmd,
			    GET_AUTHPROF | GET_ALL | __SEARCH_ALL_POLS);

			if (exec != NULL) {
				if (print_flag & PRINT_LONG)
					print_profs_long(exec, &call);
				else
					print_profs_short(exec, &call);

				free_execattr(exec);
			}
		}
		if (!IS_GET_AUTHPROF(search_flag)) {
			call.auth = false;
			exec = getexecuser(username, KV_COMMAND, cmd,
			    GET_PROF | GET_ALL | __SEARCH_ALL_POLS);

			if (exec != NULL) {
				if (print_flag & PRINT_LONG)
					print_profs_long(exec, &call);
				else
					print_profs_short(exec, &call);

				free_execattr(exec);
			}
		}
	} else {
		if (!IS_GET_PROF(search_flag)) {
			call.auth = true;
			(void) _enum_profs(username, show_profs_callback,
			    &call, NULL, _ENUM_PROFS_AUTHPROFILES);
		}
		if (!IS_GET_AUTHPROF(search_flag)) {
			call.auth = false;
			(void) _enum_profs(username, show_profs_callback,
			    &call, NULL, _ENUM_PROFS_PROFILES);
		}
	}

	if (call.cnt == 0) {
		(void) fprintf(stderr, "%s: %s: ", progname, username);
		(void) fprintf(stderr, gettext("No profiles\n"));
	}
}

/*
 * print extended profile information.
 *
 * output is "pretty printed" like
 *   [6spaces]Profile Name1[ possible profile privileges]
 *   [10spaces  ]execname1 [skip to ATTR_COL]exec1 attributes1
 *   [      spaces to ATTR_COL              ]exec1 attributes2
 *   [10spaces  ]execname2 [skip to ATTR_COL]exec2 attributes1
 *   [      spaces to ATTR_COL              ]exec2 attributes2
 *   [6spaces]Profile Name2[ possible profile privileges]
 *   etc
 */
/*
 * ATTR_COL is based on
 *   10 leading spaces +
 *   25 positions for the executable +
 *    1 space seperating the execname from the attributes
 * so attribute printing starts at column 37 (36 whitespaces)
 *
 *  25 spaces for the execname seems reasonable since currently
 *  less than 3% of the shipped exec_attr would overflow this
 */
#define	ATTR_COL	37

static void
print_profs_long(execattr_t *exec, callback_t *call)
{
	char	*curprofile;
	int	len;
	kv_t	*kv_pair;
	char	*key;
	char	*val;
	int	i;

	for (curprofile = ""; exec != NULL; exec = exec->next) {
		call->cnt++;
		/* print profile name if it is a new one */
		if (strcmp(curprofile, exec->name) != 0) {
			profattr_t *pa;
			curprofile = exec->name;

			if (call->auth) {
				(void) printf("      %s (%s)", curprofile,
				    gettext(AUTH_REQUIRED));
			} else {
				(void) printf("      %s", curprofile);
			}

			pa = getprofnam(curprofile);
			if (pa != NULL) {
				print_profile_privs(pa->attr);
				free_profattr(pa);
			}
			(void) printf("\n");
		}
		len = printf("          %s ", exec->id);

		if ((exec->attr == NULL || exec->attr->data == NULL)) {
			(void) printf("\n");
			continue;
		}

		/*
		 * if printing the name of the executable got us past the
		 * ATTR_COLth column, skip to ATTR_COL on a new line to
		 * print the attributes.
		 * else, just skip to ATTR_COL column.
		 */
		if (len >= ATTR_COL)
			(void) printf("\n%*s", ATTR_COL, " ");
		else
			(void) printf("%*s", ATTR_COL-len, " ");
		len = ATTR_COL;

		/* print all attributes of this profile */
		kv_pair = exec->attr->data;
		for (i = 0; i < exec->attr->length; i++) {
			key = kv_pair[i].key;
			val = kv_pair[i].value;
			if (key == NULL || val == NULL)
				break;
			/* align subsequent attributes on the same column */
			if (i > 0)
				(void) printf("%*s", len, " ");
			(void) printf("%s=%s\n", key, val);
		}
	}
}

static void
print_profs_short(execattr_t *exec, callback_t *call)
{
	char	*curprofile;

	for (curprofile = ""; exec != NULL; exec = exec->next) {
		call->cnt++;
		/* print profile name if it is a new one */
		if (strcmp(curprofile, exec->name) != 0) {
			curprofile = exec->name;
			if ((call->print_flag & PRINT_VERBOSE) && call->auth) {
				(void) printf("      %s (%s)\n", curprofile,
				    gettext(AUTH_REQUIRED));
			} else {
				(void) printf("      %s\n", curprofile);
			}
		}
	}
}

static void __NORETURN
usage(int status)
{
	(void) fprintf(stderr, gettext(
	    "  usage: profiles [-hlv] [-x | -X] [-c command] [user...]\n"));
	exit(status);
}

static void
print_profile_privs(kva_t *attr)
{
	char *privs;

	if (attr) {
		privs = kva_match(attr, PROFATTR_PRIVS_KW);
		if (privs)
			(void) printf(" privs=%s", privs);
	}
}
