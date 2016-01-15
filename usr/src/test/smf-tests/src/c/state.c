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
 * Copyright 2008 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * handle the persistent state of the service
 */

#include <stdio.h>
#include <stdlib.h>
#include <state.h>
#include <messages.h>
#include <synch.h>
#include <thread.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <dlfcn.h>
#include <libscf.h>

servicestate_p_t *saved;
servicestate_v_t *unsaved;

static void
switchusage(void)
{
	(void) printf("\t** STF/SMF argument preparser **\n");
	(void) printf("usage: %s -h -l <logger> [-s <service>] "
	    "[-i <instance>] [-m <method>] [ -n <children> ] "
	    "\n\t [ -r <reaction> ] [ -t <functions> ] -f <statefile> "
	    "-- <program arguments>\n",
	    find_argv0());
	(void) printf("\t<logger> is the log to use\n"
	"\t<service> is the SMF symbolic service name\n"
	"\t<method> is the method to use. Valid methods are\n"
	"\t\tstart, stop, refresh, monitor [ service ]\n"
	"\t<statefile> is the file to keep the service state\n"
	"\t<instance> is the instance id of the service\n"
	"\t<children> is the number of children to start\n"
	"\t<reaction> is the reaction of the service application\n"
	"\t\tit can be returncode <NUMBER>, signalme <SIGNAL>,\n"
	"\t\tsignalservice <SIGNAL>, triggerservicesegv <child#>,\n"
	"\t\ttriggersegv <child#>, dally <millis>\n"
	"\t\ttriggerchildexit <child#>, triggerchildrv <child#> <rv>\n"
	"\t\tservicecausefork <child#>, servicecauseforksegv <child#>\n"
	"\t\tlockdown <child#>, forkexec <child#> <command line>\n"
	"\t<functions> is a set of functions to call to report state\n"
	"\t\tthey include: userinfo, projectinfo, privinfo, poolinfo, myfmri\n"
	"\t\tlogger can be:\n\t\t\tfile:<file>\n\t\t\tpipe:<pipe file>\n"
	"\t\t\tdoor:<door file>\n\t\t\tsocket:TCP|UDP|MCP@machine:port\n\n");
}

/*
 * Try to get the scf state file for the service. This is a utility
 * so we don't need to keep specifying the -f <cfgfile> on the command
 * line.
 */
static void
get_scf_statefile(void)
{
	scf_simple_prop_t *prop;
	char *instance;

	instance = (char *)malloc(scf_limit(SCF_LIMIT_MAX_FMRI_LENGTH));

	if (instance == NULL)
		return;

	(void) snprintf(instance, scf_limit(SCF_LIMIT_MAX_FMRI_LENGTH) - 1,
	    "svc:/%s:%s", unsaved->servicename, unsaved->serviceinst);
	prop = scf_simple_prop_get(NULL, instance, "cfg", "state");
	if (prop != NULL) {
		if (scf_simple_prop_numvalues(prop) > 0) {
			const char *val = scf_simple_prop_next_astring(prop);
			if (val != NULL) {
				(void) strlcpy(unsaved->savefile, val, MAXPATHLEN);
			}
		}
		scf_simple_prop_free(prop);
	}
	free(instance);
}

/*
 * persist the service state
 * This _DOES NOT_ use the SMF repository because it may be
 * temporarily 'offline' while some of the services are running, and
 * we don't want to hamper progress of the service.
 */
void
persist_servicestate(int openflag)
{
	char savename[MAXPATHLEN];
	char *stric = &unsaved->savefile[0];
	char *lstric = stric;
	int chap = 0;
	int fh = 0;
	servicestate_p_t *pstate;

	*savename = '\0';

	if (*unsaved->savefile == '\0')
		get_scf_statefile();

	if (*unsaved->savefile == '\0') {
		(void) strlcpy(&unsaved->savefile[0], "/tmp/gltest.%s.%i",
		    MAXPATHLEN);
	}
	while ((stric = strchr(lstric, '%')) != NULL) {
		char *sp = NULL;
		*stric = '\0';
		chap += snprintf(&savename[chap], MAXPATHLEN-chap,
		    "%s", lstric);
		stric++;

		switch (*stric) {
		case '\0':
			goto breakout;
		case 'i':
			sp = unsaved->serviceinst;
			break;
		case 's':
			sp = unsaved->servicename;
			break;
		case '%':
			sp = "%";
			break;
		}
		stric++;
		chap += snprintf(&savename[chap], MAXPATHLEN-chap,
		    "%s", sp);
		lstric = stric;
	}
breakout:
	if (*savename != '\0')
		(void) strlcpy(unsaved->savefile, savename, MAXPATHLEN);

	if (openflag & O_RDWR)
		openflag |= O_CREAT;

	fh = open(unsaved->savefile, openflag,
	    S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

	if (fh == -1) {
		log_error("could not persist state for service into file %s. "
		    "Reason: %s. This is a fatal error.", unsaved->savefile,
		    strerror(errno));
		exit(5);
	}
	if (openflag & O_RDWR) {
		(void) ftruncate(fh, sizeof (*saved));
		(void) strcpy(saved->servicename, unsaved->servicename);
		(void) strcpy(saved->serviceinst, unsaved->serviceinst);

		/* LINTED */
		pstate = (servicestate_p_t *)mmap(NULL, sizeof (*saved),
		    PROT_READ | PROT_WRITE, MAP_SHARED, fh, 0);
		if (stric == MAP_FAILED) {
			log_error("could not save the persistent state of "
			    "the service this is a fatal error");
			exit(5);
		}
		if (pstate->version != PERSIST_VERSION)
			bcopy(saved, pstate, sizeof (*pstate));

		free(saved);
		saved = pstate;
	} else {
		/* LINTED E_BAD_PTR_CAST_ALIGN */
		saved = (servicestate_p_t *)mmap(NULL, sizeof (*saved),
		    PROT_READ | PROT_WRITE, MAP_PRIVATE, fh, 0);
		if (saved == MAP_FAILED)
			saved = NULL;
	}
	(void) close(fh);
}

/*
 * Lock a robust mutex. Needs to deal with the EOWNERDEAD syntax.
 */
void
robust_lock(mutex_t *mutex)
{
	int rv = mutex_lock(mutex);

	if (rv == EOWNERDEAD)
		(void) mutex_init(mutex, USYNC_PROCESS_ROBUST, 0);
}

/*
 * Add a function to the set of reporting functions.
 */
void
add_function(char *name, reportfn_t **head, reportfn_t **tail)
{
	reportfn_t *rv = calloc(1, sizeof (reportfn_t));

	rv->name = strdup(name);
	rv->method = (void (*)(void))dlsym(RTLD_SELF, rv->name);
	if (rv->method == NULL) {
		log_error("Could not find method '%s'\n", rv->name);
		free(rv->name);
		free(rv);
		return;
	}

	if (*head == NULL) {
		*head = rv;
	} else {
		(*tail)->next = rv;
	}
	*tail = rv;
}

/*
 * Parse the reporting functions string for functions to call for reporting
 */
reportfn_t *
parse_functions(char *funcs)
{
	char *pos, *start;
	reportfn_t *tail = NULL;
	reportfn_t *head = NULL;

	pos = funcs;
	start = pos;

	while (*pos != '\0') {
		if (*pos == ',' || *pos == ' ') {
			*pos = '\0';
			add_function(start, &head, &tail);
			start = pos+1;
		}
		pos++;
	}
	if (start < pos)
		add_function(start, &head, &tail);

	return (head);
}

/*
 * parse the activity to the method and extra string parameter
 */
activity_t *
parse_activity(char *text)
{
	activity_t *rv = NULL;
	int (*func)(char *);
	char *newt = NULL;
	char *spc;
	struct stat stbuf;

	if (stat(text, &stbuf) != -1) {
		int fd = open(text, O_RDONLY);
		if (fd != -1) {
			newt = calloc(MAXPATHLEN, 1);
			(void) read(fd, newt, MAXPATHLEN-1);
			(void) close(fd);
		}
	} else
		newt = strdup(text);

	spc = strchr(newt, ' ');

	if (spc != NULL) {
		*spc = '\0';
		spc++;
	}
	func = (int (*)(char *))dlsym(RTLD_SELF, (const char *)newt);
	if (func != NULL) {
		rv = (activity_t *)calloc(1, sizeof (activity_t));
		rv->method = func;
		if (spc != NULL)
			rv->data = strdup(spc);
	} else {
		(void) fprintf(stderr, "could not find method %s\n", newt);
	}
	free(newt);

	return (rv);
}

/*
 * Free activity structure
 */
void
free_activity(activity_t *activity)
{
	activity_t *pract = activity;

	while (activity != NULL) {
		if (activity->data) {
			free(activity->data);
		}
		pract = activity;
		activity = activity->next;
		free(pract);
	}
}

/*
 * Free the unsaved state
 */
void
freeunsaved(void)
{
	if (unsaved != NULL) {
		free_activity(unsaved->activity);
		if (unsaved->functions != NULL) {
			while (unsaved->functions) {
				reportfn_t *fn = unsaved->functions;
				unsaved->functions = fn->next;
				free(fn->name);
			}
		}
		free(unsaved);
	}
}
/*
 * This is a layered switch reading routine, if it sees a -h it prints it's
 * usage message then passes back to the caller.
 * This may swallow legal options to the command being called; if you want
 * to prevent that then use the -- syntax on the command line.
 */
int
read_switches(int *argc, char **argv)
{
	static int done = 0;
	mutex_t mut = DEFAULTMUTEX;
	int opt;
	int nchildren;
	activity_t *tmp_act;

	(void) mutex_lock(&mut);
	if (done != 0) {
		(void) mutex_unlock(&mut);
		return (0);
	}
	unsaved = (servicestate_v_t *)calloc(sizeof (servicestate_v_t), 1);
	unsaved->numchildren = 1;
	(void) atexit(freeunsaved);
	saved = (servicestate_p_t *)calloc(sizeof (servicestate_p_t), 1);
	saved->version = PERSIST_VERSION;
	saved->dosegv = -1;
	saved->dobuserr = -1;
	saved->stopmeflag = SVC_NO_STOP;
	saved->childtofork = -1;
	saved->segvchild = -1;
	saved->lockdown = -1;
	saved->forkexec = -1;

	while (-1 != (opt = getopt(*argc, argv, "h?s:n:m:f:l:i:r:t:"))) {
		switch (opt) {
		case 'h':
			/* FALLTHRU */
		case '?':
			switchusage();
			goto raus;
		case 's':
			(void) strlcpy(unsaved->servicename, optarg,
			    MAXPATHLEN);
			break;
		case 'm':
			(void) strlcpy(unsaved->method, optarg, MAXPATHLEN);
			break;
		case 'n':
			nchildren = atoi(optarg);
			if (nchildren > MAXCHILD)
				nchildren = MAXCHILD;
			unsaved->numchildren = nchildren;
			break;
		case 'f':
			(void) strlcpy(unsaved->savefile, optarg, MAXPATHLEN);
			break;
		case 'l':
			log_setup(optarg);
			break;
		case 'i':
			(void) strlcpy(unsaved->serviceinst, optarg,
			    MAXPATHLEN);
			break;
		case 'r':
			tmp_act = unsaved->activity;
			unsaved->activity = parse_activity(optarg);
			if (unsaved->activity != NULL)
				unsaved->activity->next = tmp_act;
			break;
		case 't':
			if (unsaved->functions == NULL)
				unsaved->functions = parse_functions(optarg);
		default:
			break;
		}
	}
raus:
	/* preserve h or ? */
	if (opt == 'h' || opt == '?')
		optind--;
	/* shift arguments */
	if (optind != 1) {
		*argc -= (optind-1);
		bcopy(argv+optind, argv+1, *argc * sizeof (char *));
	}
	optind = 1;
	if (*unsaved->servicename != '\0' && *unsaved->serviceinst != '\0')
		persist_servicestate(O_RDWR);
	else
		if (*unsaved->savefile != '\0')
			persist_servicestate(O_RDONLY);

	done = 1;
	(void) mutex_unlock(&mut);

	return (0);
}

/*
 * get a string corresponding to the reporting functions to be called.
 * This string needs to be freed by the caller.
 */
char *
get_reportfunctions(servicestate_v_t *state)
{
	char *rv = calloc(getpagesize(), sizeof (char));
	int printed = 0;
	int max = getpagesize();
	reportfn_t *function = state->functions;

	while (function != NULL) {
		int len = strlen(function->name);
		if (len + printed + 2 > max) {
			max <<= 1;
			rv = realloc(rv, max);
		}
		if (printed != 0) {
			int top = snprintf(rv + printed, max - printed, ",");
			printed += top;
		}
		/* LINTED E_SEC_PRINTF_VAR_FMT */
		printed += snprintf(rv + printed, max - printed,
		    function->name);
		function = function->next;
	}
	return (rv);
}

/*
 * Invoke all the reporting functions in order.
 */
void
invoke_reportfns(servicestate_v_t *state)
{
	reportfn_t *function = state->functions;

	while (function != NULL) {
		function->method();
		function = function->next;
	}
}

/*
 * Record a method that has been invoked. This is to assist in
 * synchronization in the tests.
 */
void
record_invocation(servicestate_p_t *state, char *method)
{
	if (*state->methodscalled[state->headcalled] != '\0') {
		state->headcalled++;
		if (state->headcalled >= METHNAMECOUNT)
			state->headcalled = 0;
	}
	(void) strlcpy(state->methodscalled[state->headcalled], method,
	    METHNAMELENGTH);
}

/*
 * get the last method that was invoked.
 * Needs to be freed by caller.
 */
char *
last_invocation(servicestate_p_t *state)
{
	char *rv = calloc(1, METHNAMELENGTH);

	(void) strlcpy(rv, state->methodscalled[state->headcalled],
	    METHNAMELENGTH);
	return (rv);
}

/*
 * Get the offset of the last call of the method passed
 */
int
invocation_offset(servicestate_p_t *state, char *method)
{
	int atpoint = state->headcalled;
	int offset = 0;

	do {
		char *location = state->methodscalled[atpoint];
		if (*location == '\0')
			break;
		if (strncmp(state->methodscalled[atpoint], method,
		    METHNAMELENGTH) == 0)
			return (offset);

		offset++;
		if (atpoint == 0)
			atpoint = METHNAMECOUNT - 1;
		else
			atpoint--;
	} while (atpoint != state->headcalled);

	return (-1);
}

/*
 * get count of a method that have been invoked within the buffer
 */
int
count_invocation(servicestate_p_t *state, char *method)
{
	int count = 0;
	int atpoint = state->headcalled;

	do {
		char *location = state->methodscalled[atpoint];
		if (*location == '\0')
			break;
		if (strncmp(state->methodscalled[atpoint],
		    method, METHNAMELENGTH) == 0)
			count++;
		if (atpoint == 0)
			atpoint = METHNAMECOUNT - 1;
		else
			atpoint--;
	} while (atpoint != state->headcalled);
	return (count);
}

/*
 * dump the saved service state to stdout
 */
void
dump_savedstate(servicestate_p_t *state)
{
	int at;

	if (state == NULL) {
		(void) printf("state is NULL\n");
		return;
	}
	(void) printf("Version: %x\n", state->version);
	(void) printf("Service name:instance: %s:%s\n", state->servicename,
	    state->serviceinst);
	(void) printf("Service count: %d\n", state->service_count);
	(void) printf("Process Group: %ld\n", (long)state->service_pgid);
	(void) printf("Process Ids:");
	for (at = 0; at < state->service_count; at++) {
		(void) printf(" %ld", (long)state->service_pid[at]);
	}
	(void) printf("\nStop me flag: %d\n", state->stopmeflag);
	(void) printf("Child# to do segv: %d\n", state->dosegv);
	(void) printf("Child# to do buserror: %d\n", state->dobuserr);
	(void) printf("Terminate with rv of: %d\n", state->stopval);
	(void) printf("service process to fork a child: %d\n", state->childtofork);
	(void) printf("Call Log:");
	at = state->headcalled;
	do {
		char *location = state->methodscalled[at];
		if (*location == '\0')
			break;
		(void) printf(" %s", location);
		if (at == 0)
			at = METHNAMECOUNT - 1;
		else
			at--;
	} while (at != state->headcalled);
	(void) printf("\n");
}
