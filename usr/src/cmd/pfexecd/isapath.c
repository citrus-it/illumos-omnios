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
 *
 * Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved.
 * Copyright 2015, Joyent, Inc.
 */

#include <alloca.h>
#include <ctype.h>
#include <errno.h>
#include <regex.h>
#include <stdlib.h>
#include <sys/systeminfo.h>
#include <string.h>
#include <unistd.h>

/*
 * Remove the isaexec path of an executable if we can't find the
 * executable at the first attempt.
 */

static regex_t regc;
static boolean_t cansplice = B_FALSE;

void
init_isa_regex(void)
{
	char *isalist;
	size_t isalen = 255;		/* wild guess */
	size_t len;
	long ret;
	char *regexpr;
	char *p;

	/*
	 * Extract the isalist(5) for userland from the kernel.
	 */
	isalist = malloc(isalen);
	do {
		ret = sysinfo(SI_ISALIST, isalist, isalen);
		if (ret == -1l) {
			free(isalist);
			return;
		}
		if (ret > isalen) {
			isalen = ret;
			isalist = realloc(isalist, isalen);
		} else
			break;
	} while (isalist != NULL);

	if (isalist == NULL)
		return;

	/* allocate room for the regex + (/())/[^/]*$ + needed \\. */
#define	LEFT	"(/("
#define	RIGHT	"))/[^/]*$"

	regexpr = alloca(ret * 2 + sizeof (LEFT RIGHT));
	(void) strcpy(regexpr, LEFT);
	len = strlen(regexpr);

	for (p = isalist; *p; p++) {
		switch (*p) {
		case '+':
		case '|':
		case '*':
		case '[':
		case ']':
		case '{':
		case '}':
		case '\\':
			regexpr[len++] = '\\';
			/* FALLTHROUGH */
		default:
			regexpr[len++] = *p;
			break;
		case ' ':
		case '\t':
			regexpr[len++] = '|';
			break;
		}
	}

	free(isalist);
	regexpr[len] = '\0';
	(void) strcat(regexpr, RIGHT);

	if (regcomp(&regc, regexpr, REG_EXTENDED) != 0)
		return;

	cansplice = B_TRUE;
}

#define	NMATCH	2

boolean_t
removeisapath(char *path)
{
	regmatch_t match[NMATCH];

	if (!cansplice || regexec(&regc, path, NMATCH, match, 0) != 0)
		return (B_FALSE);

	/*
	 * The first match includes the whole matched expression including the
	 * end of the string.  The second match includes the "/" + "isa" and
	 * that is the part we need to remove.
	 */

	if (match[1].rm_so == -1)
		return (B_FALSE);

	/* match[0].rm_eo == strlen(path) */
	(void) memmove(path + match[1].rm_so, path + match[1].rm_eo,
	    match[0].rm_eo - match[1].rm_eo + 1);

	return (B_TRUE);
}
