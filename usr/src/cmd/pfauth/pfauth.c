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
 * Copyright 2026 OmniOS Community Edition (OmniOSce) Association.
 */

/*
 * pfauth - authentication helper for pfexecd
 *
 * pfexecd invokes this program when a command matches one of a user's
 * authenticated rights profiles (see the auth_profiles keyword in
 * user_attr(5)) in order to authenticate that user before the command is
 * allowed to execute.
 *
 * It is passed the name of the user to be authenticated, the name of the
 * matched profile and the process ID of the process being authenticated.
 * A descriptor for that process's controlling terminal is provided on the
 * standard file descriptors. The terminal is grafted onto this process's
 * session (TIOCGRAFT) so that /dev/tty resolves to it, both for
 * getpassphrase() in the PAM conversation below and for any PAM module
 * which opens the terminal directly. The exit status is 0 if the user
 * authenticated successfully.
 *
 * When invoked with -k, no authentication takes place; instead any
 * cached authentication state which modules such as pam_timestamp(7)
 * hold for the user and terminal is discarded, via
 * pam_setcred(PAM_DELETE_CRED). This is how pfexec -k is implemented.
 */

#include <err.h>
#include <errno.h>
#include <limits.h>
#include <priv.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <termios.h>
#include <ucred.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mkdev.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <bsm/adt.h>
#include <bsm/adt_event.h>

#include <security/pam_appl.h>

/*
 * The maximum time to wait for authentication to complete, and the
 * interval at which the process being authenticated is checked for
 * liveness, both in seconds.
 */
#define	PFAUTH_TIMEOUT	300
#define	PFAUTH_TICK	1

typedef struct pfauth_cbdata {
	const char *pc_profile;
	bool pc_announced;
} pfauth_cbdata_t;

static struct termios saved_ti;
static bool have_ti;
static volatile int timeleft = PFAUTH_TIMEOUT;
static pid_t clientpid;
static ucred_t *clientuc;

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
pfauth_conv(int nmsg, const struct pam_message **messages,
    struct pam_response **responses, void *data)
{
	pfauth_cbdata_t *pc = data;
	const struct pam_message *m;
	struct pam_response *r;

	if (nmsg <= 0)
		return (PAM_CONV_ERR);

	/*
	 * Explain why a conversation is happening, once, and only if one
	 * happens at all; a module which satisfies authentication from
	 * cached state, such as pam_timestamp(7), never calls here and
	 * the exec proceeds silently.
	 */
	if (pc != NULL && pc->pc_profile != NULL && !pc->pc_announced) {
		pc->pc_announced = true;
		(void) printf("Authentication required for '%s' profile\n",
		    pc->pc_profile);
	}

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

			if (fgets(respbuf, sizeof (respbuf), stdin) == NULL) {
				freeresponses(nmsg, responses);
				return (PAM_CONV_ERR);
			}
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
audit_result(const char *profile, const pam_handle_t *pamh, int pamerr)
{
	adt_session_data_t *ah;
	adt_event_data_t *event;

	if (adt_start_session(&ah, NULL, 0) != 0) {
		syslog(LOG_AUTH | LOG_ALERT, "adt_start_session(pfauth): %m");
		return;
	}

	/*
	 * The audit context for the event is that of the process being
	 * authenticated, not that of this helper, which is a child of
	 * pfexecd. The credentials were captured before the conversation
	 * began, guarding against the process ID being reused while the
	 * user was at the prompt.
	 */
	if (clientuc == NULL) {
		syslog(LOG_AUTH | LOG_ALERT,
		    "no client credentials for audit (pid %d)",
		    (int)clientpid);
		(void) adt_end_session(ah);
		return;
	}

	if (adt_set_from_ucred(ah, clientuc, ADT_NEW) != 0) {
		syslog(LOG_AUTH | LOG_ALERT,
		    "adt_set_from_ucred(pfauth): %m");
		(void) adt_end_session(ah);
		return;
	}

	if ((event = adt_alloc_event(ah, ADT_pfauth)) == NULL) {
		syslog(LOG_AUTH | LOG_ALERT, "adt_alloc_event(pfauth): %m");
		(void) adt_end_session(ah);
		return;
	}

	event->adt_pfauth.message = (char *)profile;

	if (adt_put_event(event,
	    pamerr == PAM_SUCCESS ? ADT_SUCCESS : ADT_FAILURE,
	    pamerr == PAM_SUCCESS ? ADT_SUCCESS : ADT_FAIL_PAM + pamerr) !=
	    0) {
		syslog(LOG_AUTH | LOG_ALERT, "adt_put_event(pfauth, %s): %m",
		    pam_strerror((pam_handle_t *)pamh, pamerr));
	}

	adt_free_event(event);
	(void) adt_end_session(ah);
}

/*
 * Resolve the name of the terminal on the given descriptor. The terminal
 * descriptor which pfexecd passes on refers to the underlying device
 * rather than to a node in /dev, which defeats ttyname(3C)'s exact
 * matching and sends it on an expensive walk of the whole of /dev. The
 * terminal is nearly always a pseudo-terminal, so try the corresponding
 * /dev/pts name first and verify that it refers to the same device,
 * falling back to ttyname(3C) for anything else.
 */
static const char *
tty_name(int fd)
{
	static char buf[32];
	struct stat st, pst;

	if (fstat(fd, &st) == 0) {
		(void) snprintf(buf, sizeof (buf), "/dev/pts/%d",
		    (int)minor(st.st_rdev));
		if (stat(buf, &pst) == 0 && S_ISCHR(pst.st_mode) &&
		    pst.st_rdev == st.st_rdev) {
			return (buf);
		}
	}

	return (ttyname(fd));
}

/*
 * Invoked for SIGTERM and SIGHUP, and from sigtick() below. The terminal
 * may be mid-conversation with echo disabled, so restore its initial
 * settings before exiting.
 */
static void
sigexit(int sig __unused)
{
	if (have_ti)
		(void) tcsetattr(STDIN_FILENO, TCSANOW, &saved_ti);
	_exit(1);
}

/*
 * Invoked for SIGALRM. Give up if the overall authentication timeout has
 * expired, or if the process being authenticated has gone away; the latter
 * happens when the pending exec(2) is interrupted, for example by the user
 * pressing ^C at the prompt. The handler is installed with SA_RESTART so
 * that each tick does not disturb the PAM conversation.
 */
static void
sigtick(int sig)
{
	int serrno = errno;

	timeleft -= PFAUTH_TICK;
	if (timeleft <= 0 || (kill(clientpid, 0) != 0 && errno == ESRCH))
		sigexit(sig);

	(void) alarm(PFAUTH_TICK);
	errno = serrno;
}

int
main(int argc, char **argv)
{
	struct pam_conv pam_conv;
	pam_handle_t *pamh;
	struct sigaction sa;
	const char *user, *profile, *tty, *errstr;
	const char *failmsg = NULL;
	pfauth_cbdata_t cbdata = { NULL, false };
	bool drop = false;
	int ret;

	/* Prevent this process from creating core dumps. */
	(void) setpflags(__PROC_PROTECT, 1);

	if (argc == 4 && strcmp(argv[1], "-k") == 0) {
		drop = true;
		user = argv[2];
		profile = NULL;
	} else if (argc == 4) {
		user = argv[1];
		profile = argv[2];
	} else {
		errx(EXIT_FAILURE, "pfauth is a private pfexecd helper");
	}

	if (!isatty(STDIN_FILENO))
		errx(EXIT_FAILURE, "pfauth is a private pfexecd helper");

	clientpid = (pid_t)strtonum(argv[3], 1, INT_MAX, &errstr);
	if (errstr != NULL)
		errx(EXIT_FAILURE, "process ID '%s' is %s", argv[3], errstr);

	/*
	 * Capture the client's credentials before the conversation begins,
	 * for the audit record; see audit_result().
	 */
	clientuc = ucred_get(clientpid);

	if (!drop) {
		/*
		 * Detach from pfexecd's session and graft the terminal onto
		 * the new session.
		 */
		if (setsid() == -1)
			err(EXIT_FAILURE, "could not create a new session");
		if (ioctl(STDIN_FILENO, TIOCGRAFT, 0) != 0)
			err(EXIT_FAILURE, "could not graft the terminal");

		if (tcgetattr(STDIN_FILENO, &saved_ti) == 0)
			have_ti = true;

		sa.sa_flags = 0;
		sa.sa_handler = sigexit;
		(void) sigemptyset(&sa.sa_mask);
		(void) sigaction(SIGTERM, &sa, NULL);
		(void) sigaction(SIGHUP, &sa, NULL);

		sa.sa_flags = SA_RESTART;
		sa.sa_handler = sigtick;
		(void) sigaction(SIGALRM, &sa, NULL);

		(void) alarm(PFAUTH_TICK);
	}

	pam_conv.conv = pfauth_conv;
	pam_conv.appdata_ptr = &cbdata;

	ret = pam_start("pfexec", user, &pam_conv, &pamh);
	if (ret != PAM_SUCCESS) {
		errx(EXIT_FAILURE, "pam_start() failed: %s",
		    pam_strerror(pamh, ret));
	}

	if ((tty = tty_name(STDIN_FILENO)) == NULL)
		tty = "/dev/???";

	ret = pam_set_item(pamh, PAM_TTY, tty);
	if (ret != PAM_SUCCESS) {
		errx(EXIT_FAILURE, "failed to set TTY: %s",
		    pam_strerror(pamh, ret));
	}

	/* For pfexec, the authenticating user is the user being checked. */
	ret = pam_set_item(pamh, PAM_AUSER, user);
	if (ret != PAM_SUCCESS) {
		errx(EXIT_FAILURE, "failed to set AUSER: %s",
		    pam_strerror(pamh, ret));
	}

	if (drop) {
		/*
		 * Discard any cached authentication state for this user
		 * and terminal. This is best effort; whether anything is
		 * cached depends on the configured PAM stack.
		 */
		(void) pam_setcred(pamh, PAM_DELETE_CRED);
		(void) pam_end(pamh, PAM_SUCCESS);
		return (0);
	}

	cbdata.pc_profile = profile;
	ret = pam_authenticate(pamh, PAM_DISALLOW_NULL_AUTHTOK);

	if (ret != PAM_SUCCESS) {
		failmsg = "Authentication failed";
	} else {
		/*
		 * Also apply account validity policy, such as password and
		 * account expiry, before allowing the authenticated
		 * profiles to be used.
		 */
		ret = pam_acct_mgmt(pamh, PAM_DISALLOW_NULL_AUTHTOK);
		if (ret != PAM_SUCCESS) {
			failmsg = "Account validation failed";
		} else {
			/*
			 * Give modules such as pam_timestamp(7) the
			 * opportunity to record the successful
			 * authentication. This does not affect the result.
			 */
			(void) pam_setcred(pamh, PAM_ESTABLISH_CRED);
		}
	}

	/*
	 * The conversation is over; disarm the liveness tick so that it
	 * cannot cut down the audit record for a completed attempt.
	 */
	(void) alarm(0);

	openlog("pfauth", LOG_CONS, LOG_AUTH);
	audit_result(profile, pamh, ret);
	closelog();

	(void) pam_end(pamh, PAM_SUCCESS);

	if (ret != PAM_SUCCESS) {
		/*
		 * If the process whose exec prompted this authentication
		 * has already gone away then the conversation was abandoned
		 * at the prompt, typically by an interrupt, and the failure
		 * is of no interest to whatever now owns the terminal, so
		 * do not write over it. The audit record is still cut.
		 */
		if (kill(clientpid, 0) == 0 || errno != ESRCH)
			(void) fprintf(stderr, "%s\n", failmsg);
		return (EXIT_FAILURE);
	}

	return (0);
}
