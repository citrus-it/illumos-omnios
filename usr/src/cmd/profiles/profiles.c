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
#include <string.h>
#include <libintl.h>
#include <locale.h>
#include <deflt.h>
#include <user_attr.h>
#include <prof_attr.h>
#include <exec_attr.h>
#include <auth_attr.h>


#define	EXIT_OK		0
#define	EXIT_FATAL	1
#define	EXIT_NON_FATAL	2

#define	TMP_BUF_LEN	2048		/* size of temp string buffer */

#define	PRINT_DEFAULT	0x0000
#define	PRINT_NAME	0x0010
#define	PRINT_LONG	0x0020
#define	PRINT_VERBOSE	0x0040

#ifndef TEXT_DOMAIN			/* Should be defined by cc -D */
#define	TEXT_DOMAIN	"SYS_TEST"
#endif

#define	AUTH_REQUIRED	"Authentication required"

static void usage(void);
static int show_profs(char *, char *, int, int);
static void print_profs_long(execattr_t *, int, bool);
static void print_profs_short(execattr_t *, int, bool);
static void print_profile_privs(kva_t *);

static char *progname = "profiles";

typedef struct {
	int cnt;
	int print_flag;
	bool auth;
} callback_t;

int
main(int argc, char *argv[])
{
	extern int	optind;
	char		*cmd = NULL;
	int		c;
	int		status = EXIT_OK;
	int		print_flag = PRINT_DEFAULT;
	int		search_flag = 0;

	(void) setlocale(LC_ALL, "");
	(void) textdomain(TEXT_DOMAIN);

	while ((c = getopt(argc, argv, "c:lvxX")) != EOF) {
		switch (c) {
		case 'c':
			cmd = optarg;
			break;
		case 'l':
			print_flag |= PRINT_LONG;
			break;
		case 'v':
			print_flag |= PRINT_VERBOSE;
			break;
		case 'x':
			search_flag = GET_AUTHPROF;
			break;
		case 'X':
			search_flag = GET_PROF;
			break;
		default:
			usage();
			return (EXIT_FATAL);
		}
	}
	argc -= optind;
	argv += optind;

	if (*argv == NULL) {
		status = show_profs(NULL, cmd, print_flag, search_flag);
	} else {
		print_flag |= PRINT_NAME;
		do {
			(void) printf("%s:", *argv);
			(void) printf("\n");
			status = show_profs((char *)*argv, cmd, print_flag,
			    search_flag);
			if (status == EXIT_FATAL) {
				break;
			}
			if (argv[1] != NULL) {
				/* seperate users with empty line */
				(void) printf("\n");
			}
		} while (*++argv);
	}
	status = (status == EXIT_OK) ? status : EXIT_FATAL;

	return (status);
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

static int
show_profs(char *username, char *cmd, int print_flag, int search_flag)
{
	int		status = EXIT_OK;
	struct passwd	*pw;
	execattr_t	*exec;

	if (username == NULL) {
		if ((pw = getpwuid(getuid())) == NULL) {
			status = EXIT_NON_FATAL;
			(void) fprintf(stderr, "%s: ", progname);
			(void) fprintf(stderr, gettext("No passwd entry\n"));
			return (status);
		}
		username = pw->pw_name;
	} else if (getpwnam(username) == NULL) {
		status = EXIT_NON_FATAL;
		(void) fprintf(stderr, "%s: %s: ", progname, username);
		(void) fprintf(stderr, gettext("No such user\n"));
		return (status);
	}

	if ((print_flag & PRINT_LONG) || cmd != NULL) {
		status = EXIT_NON_FATAL;

		if (search_flag == 0 || IS_GET_PROF(search_flag)) {
			exec = getexecuser(username, KV_COMMAND, cmd,
			    GET_AUTHPROF | GET_ALL | __SEARCH_ALL_POLS);
			if (exec != NULL) {
				status = EXIT_OK;
				if (print_flag & PRINT_LONG) {
					print_profs_long(exec, print_flag,
					    true);
				} else {
					print_profs_short(exec, print_flag,
					    true);
				}
				free_execattr(exec);
			}
		}

		if (search_flag == 0 || IS_GET_AUTHPROF(search_flag)) {
			exec = getexecuser(username, KV_COMMAND, cmd,
			    GET_PROF | GET_ALL | __SEARCH_ALL_POLS);
			if (exec != NULL) {
				status = EXIT_OK;
				if (print_flag & PRINT_LONG) {
					print_profs_long(exec, print_flag,
					    false);
				} else {
					print_profs_short(exec, print_flag,
					    false);
				}
				free_execattr(exec);
			}
		}
	} else {
		callback_t call;

		call.print_flag = print_flag;
		call.cnt = 0;

		if (search_flag == 0 || IS_GET_PROF(search_flag)) {
			call.auth = true;
			(void) _enum_profs(username, show_profs_callback,
			    &call, NULL, _ENUM_PROFS_AUTHPROFILES);
		}
		if (search_flag == 0 || IS_GET_AUTHPROF(search_flag)) {
			call.auth = false;
			(void) _enum_profs(username, show_profs_callback,
			    &call, NULL, _ENUM_PROFS_PROFILES);
		}

		if (call.cnt == 0)
			status = EXIT_NON_FATAL;
	}

	if (status == EXIT_NON_FATAL) {
		(void) fprintf(stderr, "%s: %s: ", progname, username);
		(void) fprintf(stderr, gettext("No profiles\n"));
	}

	return (status);
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
print_profs_long(execattr_t *exec, int print_flag, bool auth)
{
	char	*curprofile;
	int	len;
	kv_t	*kv_pair;
	char	*key;
	char	*val;
	int	i;

	for (curprofile = ""; exec != NULL; exec = exec->next) {
		/* print profile name if it is a new one */
		if (strcmp(curprofile, exec->name) != 0) {
			profattr_t *pa;
			curprofile = exec->name;

			if (auth) {
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
		 * print the attribues.
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
print_profs_short(execattr_t *exec, int print_flag, bool auth)
{
	char	*curprofile;

	for (curprofile = ""; exec != NULL; exec = exec->next) {
		/* print profile name if it is a new one */
		if (strcmp(curprofile, exec->name) != 0) {
			curprofile = exec->name;
			if ((print_flag & PRINT_VERBOSE) && auth) {
				(void) printf("      %s (%s)\n", curprofile,
				    gettext(AUTH_REQUIRED));
			} else {
				(void) printf("      %s\n", curprofile);
			}
		}
	}
}

static void
usage(void)
{
	(void) fprintf(stderr, gettext(
	    "  usage: profiles [-vxX] [-l] [-c command] [user...]\n"));
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
