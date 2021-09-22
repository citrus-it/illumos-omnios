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
 * Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T
 * Copyright (c) 1987, 1988 Microsoft Corporation
 * Copyright (c) 1988, 2010, Oracle and/or its affiliates. All rights reserved.
 * Copyright 2012 Milan Jurik. All rights reserved.
 * Copyright 2014 Nexenta Systems, Inc.
 * Copyright 2022 OmniOS Community Edition (OmniOSce) Association.
 */

#include <err.h>
#include <errno.h>
#include <exec_attr.h>
#include <priv.h>
#include <pwd.h>
#include <secdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <sys/types.h>

#include <bsm/adt.h>
#include <bsm/adt_event.h>

#include <security/pam_appl.h>

#include "isapath.h"

static void
freeresponses(int nmsg, struct pam_response **responses)
{
	struct pam_response *r;
	uint_t i;

	for (i = 0, r = *responses; i < nmsg; i++, r++) {
		if (r->resp != NULL)
			freezero(r->resp, strlen(r->resp));
	}
	free(*responses);
	*responses = NULL;
}

static int
pfexec_conv(int nmsg, struct pam_message **messages,
    struct pam_response **responses, void *data __unused)
{
	struct pam_message *m;
	struct pam_response *r;

	if (nmsg <= 0)
		return (PAM_CONV_ERR);

	*responses = calloc(nmsg, sizeof (struct pam_response));
	if (*responses == NULL)
		return (PAM_BUF_ERR);

	m = *messages;
	r = *responses;
	for (uint_t i = 0; i < nmsg; i++) {
		switch (m->msg_style) {

		case PAM_PROMPT_ECHO_OFF: {
			char *p;

			errno = 0;
			p = getpassphrase(m->msg);
			if (errno == EINTR)
				return (PAM_CONV_ERR);
			if (p != NULL) {
				r->resp = strdup(p);
				if (r->resp == NULL) {
					freeresponses(nmsg, responses);
					return (PAM_BUF_ERR);
				}
			}
			break;
		}

		case PAM_PROMPT_ECHO_ON: {
			char respbuf[PAM_MAX_RESP_SIZE];
			char *p;

			if (m->msg != NULL)
				(void) fputs(m->msg, stdout);

			(void) fgets(respbuf, sizeof (respbuf), stdin);
			p = respbuf;
			(void) strsep(&p, "\n");

			r->resp = strdup(respbuf);
			if (r->resp == NULL) {
				freeresponses(nmsg, responses);
				return (PAM_BUF_ERR);
			}
			break;
		}

		case PAM_ERROR_MSG:
			if (m->msg != NULL) {
				(void) fputs(m->msg, stderr);
				(void) fputs("\n", stderr);
			}
			break;

		case PAM_TEXT_INFO:
			if (m->msg != NULL) {
				(void) fputs(m->msg, stdout);
				(void) fputs("\n", stdout);
			}
			break;

		default:
			break;
		}
		m++, r++;
	}

	return (PAM_SUCCESS);
}

static void
audit_result(pam_handle_t *pamh, int pamerr)
{
	adt_session_data_t *ah;
	adt_event_data_t *event;

	if (adt_start_session(&ah, NULL, ADT_USE_PROC_DATA) != 0) {
		syslog(LOG_AUTH | LOG_ALERT, "adt_start_session(pfauth): %m");
		return;
	}

	if ((event = adt_alloc_event(ah, ADT_pfauth)) == NULL) {
		syslog(LOG_AUTH | LOG_ALERT, "adt_alloc_event(pfauth): %m");
		return;
	}

	if (adt_put_event(event,
	    pamerr == PAM_SUCCESS ? ADT_SUCCESS : ADT_FAILURE,
	    ADT_FAIL_PAM + pamerr) != 0) {
		syslog(LOG_AUTH | LOG_ALERT, "adt_put_event(pfauth, %s): %m",
		    pam_strerror(pamh, pamerr));
	}

	adt_free_event(event);
	(void) adt_end_session(ah);
}

int
main(int argc, char **argv)
{
	struct pam_conv pam_conv;
	pam_handle_t *pamh;
	struct passwd *pw;
	execattr_t *exec;
	const char *cmd;
	char *tty;
	uid_t uid;
	int ret;

	/* Prevent this process from creating core dumps. */
	(void) setpflags(__PROC_PROTECT, 1);

	if (argc < 2) {
		(void) fprintf(stderr, "Syntax: pfauth cmd [arg ..]\n");
		exit(EXIT_FAILURE);
	}

	cmd = argv[1];
	uid = getuid();

	if ((pw = getpwuid(uid)) == NULL)
		err(EXIT_FAILURE, "getpwuid() failed");

	exec = getexecuser(pw->pw_name, KV_COMMAND, cmd,
	    GET_ONE | GET_AUTHPROF);
	if (exec == NULL || exec->name == NULL || exec->attr == NULL) {
		char *isacmd;

		isacmd = strdup(cmd);
		if (isacmd == NULL)
			err(EXIT_FAILURE, NULL);

		init_isa_regex();
		if (removeisapath(isacmd)) {
			free_execattr(exec);
			exec = getexecuser(pw->pw_name, KV_COMMAND, isacmd,
			    GET_ONE | GET_AUTHPROF);
		}
		free(isacmd);
	}

	if (exec == NULL) {
		err(EXIT_FAILURE, "Cannot find matching auth profile for '%s'",
		    cmd);
	}

	pam_conv.conv = pfexec_conv;
	pam_conv.appdata_ptr = NULL;

	ret = pam_start("pfexec", pw->pw_name, &pam_conv, &pamh);
	if (ret != PAM_SUCCESS) {
		errx(EXIT_FAILURE, "pam_start() failed: %s",
		    pam_strerror(pamh, ret));
	}

	if ((tty = ttyname(STDIN_FILENO)) == NULL)
		tty = "/dev/???";

	ret = pam_set_item(pamh, PAM_TTY, tty);
	if (ret != PAM_SUCCESS) {
		errx(EXIT_FAILURE, "failed to set TTY: %s",
		    pam_strerror(pamh, ret));
	}

	printf("Authentication required for '%s' profile\n", exec->name);
	(void) signal(SIGQUIT, SIG_IGN);
	(void) signal(SIGINT, SIG_IGN);
	ret = pam_authenticate(pamh, PAM_DISALLOW_NULL_AUTHTOK);
	(void) signal(SIGQUIT, SIG_DFL);
	(void) signal(SIGINT, SIG_DFL);

	openlog("pfauth", LOG_CONS, LOG_AUTH);
	audit_result(pamh, ret);
	closelog();

	(void) pam_end(pamh, PAM_SUCCESS);

	if (ret != PAM_SUCCESS) {
		fprintf(stderr, "Authentication failed\n");
		return (1);
	}

	if (setpflags(PRIV_PFEXEC, 1) != 0)
		err(EXIT_FAILURE, "Failed to reset pfexec privilege");
	if (setpflags(PRIV_PFEXEC_AUTH, 1) != 0)
		err(EXIT_FAILURE, "Failed to record authentication");
	if (setreuid(uid, uid) != 0)
		err(EXIT_FAILURE, "setreuid() failed");

	argv++;
	(void) execvp(argv[0], argv);

	err(EXIT_FAILURE, "Failed to execute %s", argv[0]);
	return (1);
}
