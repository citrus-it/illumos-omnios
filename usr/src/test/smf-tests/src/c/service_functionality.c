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

#pragma ident	"@(#)service_functionality.c	1.3	08/05/22 SMI"

/*
 * Provides the service functionality for the dummy service.
 */

#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <messages.h>
#include <time.h>
#include <ctype.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <service_functionality.h>
#include "siglist_impl.h"

#define	SVC_ONLINE	100
#define	SVC_DEGRADED	101
#define	SVC_OFFLINE	102

int triggersegv();
int triggerbuserr();
void dolockdown(void);

/*
 * record service's pid and things for stopping purposes
 */
static void
do_servicebeginstate(int id)
{
	robust_lock(&saved->lock);

	if (saved->service_pid[id] != 0) {
		int rv = kill(saved->service_pid[id], 0);
		if ((rv == 0) || (rv == -1 && errno != ESRCH)) {
			/* another copy of me running... abend */
			log_service("already running %s:%s in "
			    "pid %ld%s", unsaved->servicename,
			    unsaved->serviceinst, saved->service_pid[id],
			    (rv == -1) ?
			    " and I don't have privileges on it" : "");
			log_error("service terminating abnormally");
			(void) mutex_unlock(&saved->lock);
			exit(255);
		}
	}
	saved->service_pid[id] = getpid();
	(void) mutex_unlock(&saved->lock);
}

/*
 * clear the service pid information from the state
 */
static void
do_serviceendstate(int id)
{
	robust_lock(&saved->lock);
	saved->service_pid[id] = 0;
	(void) mutex_unlock(&saved->lock);
}

/*
 * tell the service to terminate.
 */
static int
do_servicedostop(void)
{
	int atchild;
	int deadchild = 0;
	int sleepcount = 5;
	int childcount = 0;

	robust_lock(&saved->lock);
	saved->stopmeflag = SVC_ALL_STOP;
	(void) mutex_unlock(&saved->lock);
	do {
		deadchild = 0;
		robust_lock(&saved->lock);
		childcount = saved->service_count;
		(void) mutex_unlock(&saved->lock);
		for (atchild = 0; atchild < childcount; atchild++) {
			pid_t pid;
			robust_lock(&saved->lock);
			pid = saved->service_pid[atchild];
			(void) mutex_unlock(&saved->lock);
			if (pid == 0 ||
			    ((-1 == kill(pid, 0)) && (errno == ESRCH))) {
				deadchild++;
				continue;
			}
		}
		if (deadchild >= childcount)
			return (0);
		(void) sleep(1);
	} while (sleepcount--);
	return (1);
}

/*
 * Just kill the service.
 * Intended to kill a service without any logging output.
 */
static void
do_servicekill()
{
	if (saved->service_pgid != 0 && saved->service_pgid > 0)
		(void) kill(-saved->service_pgid, SIGKILL);
}

/*
 * force the service to terminate.
 * This is for when the service method is stalled, it makes no progress
 * when in that state, so we need an external halting mechanism.
 */
static void
do_serviceforcestop(void)
{
	pid_t spid = 0;
	struct timespec tosl = { 0, 100000000L };
	int atchild;

	robust_lock(&saved->lock);
	saved->stopmeflag = SVC_ALL_STOP;
	(void) mutex_unlock(&saved->lock);
	(void) nanosleep(&tosl, NULL);
	for (atchild = 0; atchild < saved->service_count; atchild++) {
		int slc = 0;
		robust_lock(&saved->lock);
		spid = saved->service_pid[atchild];
		(void) mutex_unlock(&saved->lock);
		if (spid == 0)
			continue;
		while (slc++ < 15) {
			if (kill(spid, 0) == -1 && errno == ESRCH) {
				spid = 0;
				break;
			}
			(void) nanosleep(&tosl, NULL);
		}
		robust_lock(&saved->lock);
		if (spid != 0 && spid != -1) {
			(void) kill(spid, SIGKILL);
		}
		saved->service_pid[atchild] = 0;
		(void) mutex_unlock(&saved->lock);
	}
	robust_lock(&saved->lock);
	saved->stopmeflag = SVC_NO_STOP;
	(void) mutex_unlock(&saved->lock);
}

/* for triggering a segv/buserr */
static int my_id;

/*
 * The method that executes as the service
 */
int
perform_service(void)
{
	int id;

	if (getsid(getpid()) != getpid())
		if (setpgid(0, 0) != 0) {
			perror("setpgid");
			return (-1);
		}
	saved->service_pgid = getpid();
	saved->stopmeflag = SVC_NO_STOP;

	for (id = 0; id < unsaved->numchildren; id++) {
		pid_t child = fork();
		if (child == 0) {
			my_id = id;
			do_servicebeginstate(id);
			while (0 == process_activity("service"))
				(void) sleep(1);
			do_serviceendstate(id);
			exit(saved->returncode);
		} else if (child == -1) {
			if (id > 0) {
				saved->service_count = id-1;
				do_serviceforcestop();
			}
			return (-1);
		}
	}
	saved->service_count = unsaved->numchildren;

	exit(0);
	/* NOTREACHED */
}

/*
 * do the activity recorded for the service in question
 */
int
process_activity(const char *method)
{
	activity_t *tact;

	if (strcmp(method, "force_stop") == 0) {
		do_serviceforcestop();
		return (0);
	}

	if (strcmp(method, "service_justkill") == 0) {
		do_servicekill();
		return (0);
	}

	tact = unsaved->activity;
	if (tact != NULL) {
		int rv = 0;

		while (tact != NULL) {
			int trv;

			trv = tact->method(tact->data);
			if (trv != 0) {
				rv = trv;
			}
			tact = tact->next;
		}
		return (rv);
	}

	if (strcmp(method, "stop") == 0) {
		return (do_servicedostop());
	}

	if (strcmp(method, "service") == 0) {
		int wstat;

		(void) waitpid((pid_t)-1, &wstat, WNOHANG);

		/* check the termination choices */
		if (saved->stopmeflag == SVC_ALL_STOP)
			return (1);
		if (saved->stopmeflag == my_id) {
			saved->stopmeflag = SVC_NO_STOP;
			return (1);
		}
		if (saved->lockdown == my_id) {
			saved->lockdown = -1;
			dolockdown();
		}
		if (saved->dosegv == my_id) {
			saved->dosegv = -1;
			(void) triggersegv();
		}
		if (saved->dobuserr == my_id) {
			saved->dobuserr = -1;
			(void) triggerbuserr();
		}
		if (saved->stopval != 0)
			exit(saved->stopval);
		if (saved->childtofork == my_id) {
			pid_t cpid;

			cpid = fork();
			saved->childtofork = -1;
			if (cpid == 0) {
				if (saved->segvchild == my_id) {
					saved->segvchild = -1;
					(void) triggersegv();
				}
				exit(1);
			}
		}
		if (saved->forkexec == my_id) {
			saved->forkexec = -1;
			(void) system(saved->commandline);
		}
		return (0);
	}

	if (strcmp(method, "monitor") == 0) {
		/* Tell the truth... */
		int32_t sc = saved->service_count;
		int32_t countonline = 0;
		if (sc == 0) {
			return (SVC_OFFLINE);
		}
		while (sc--) {
			if (saved->service_pid[sc] > 0)
				if (kill(saved->service_pid[sc], 0) != -1)
					countonline++;
		}
		if (countonline == saved->service_count)
			return (SVC_ONLINE);
		else
			return (SVC_DEGRADED);
	}
	return (0);
}

/*
 * lock down the entire address space of a process
 */
void
dolockdown(void)
{
	int rv;

	rv = memcntl(NULL, 0, MC_LOCKAS, (caddr_t)MCL_CURRENT,
	    PROC_TEXT | PROC_DATA, 0);

	if (rv == -1) {
		perror("memcntl failed");
	} else {
		record_invocation(saved, "memcntl");
	}
}

/*
 * The fault functions...
 */

/*
 * @fault_function: stall <int>
 * Stall for an indefinite period. Never returns.
 */
int
stall(char *activity)
{
	int interval = 1;

	if (activity != NULL)
		interval = atoi(activity);
	if (interval < 1) interval = 1;
	for (;;)
		(void) sleep(interval);
	/* NOTREACHED */
	return (0);
}

/*
 * @fault_function: returncode <code>
 * Generate a return code.
 */
int
returncode(char *value)
{
	int rv = 0;
	if (value != NULL)
		rv = atoi(value);
	return (rv);
}

/* convert a piece of text reading S???? to a signal number */
static int
signalparse(char *sigtext)
{
	struct sigelt const *elt;

	for (elt = &(sigarray[0]); elt->signum != -1; elt++) {
		if (strcasecmp(elt->text, sigtext) == 0) {
			return (elt->signum);
		}
	}
	return (-1);
}

/* read some text and convert it into a number */
static int
readvalue(char *text)
{
	if (*text == 'S' || *text == 's')
		return (signalparse(text));
	else
		return (atoi(text));
}

/*
 * @fault_function: signalme <SIGNAL (number, code)>
 * generate a synchronous signal in the process.
 */
int
signalme(char *value)
{
	int signum = readvalue(value);
	(void) kill(getpid(), signum);
	return (0);
}

/*
 * @fault_function: signalservice <SIGNAL (number, code)>
 * generate a signal to all the service processes.
 */
int
signalservice(char *value)
{
	int signum = readvalue(value);
	(void) kill(saved->service_pgid, signum);
	return (0);
}

/*
 * @fault_function: triggerservicesegv <Child#>
 * Cause service to SEGV on it's own.
 */
int
triggerservicesegv(char *child)
{
	if (child != NULL)
		saved->dosegv = atoi(child);
	else
		saved->dosegv = 0;
	return (0);
}

/*
 * @fault_function: triggerservicebuserr <Child#>
 * Cause service to BUS err on it's own.
 */
int
triggerservicebuserr(char *child)
{
	if (child != NULL)
		saved->dobuserr = atoi(child);
	else
		saved->dobuserr = 0;
	return (0);
}

/*
 * @fault_function: triggerbuserr
 * trigger a bus error by attempting to access a non-4byte aligned piece of
 * memory using a 4byte alignment request.
 * \todo this will not work on intel -- fix it
 */
int
triggerbuserr()
{
	int foo;

	foo = *((int *)1);
	return (foo);
}

/*
 * @fault_function: triggersegv
 * Trigger a SEGV by attempting to access a null pointer
 */
int
triggersegv()
{
	char *foo = (char *)NULL;
	(void) fprintf(stderr, "%s", foo);
	return ((int)foo);
}

/*
 * @fault_function: triggernonzeroterm <value>
 * for all children trigger service termination with non-zero RV
 */
int
triggernonzeroterm(char *value)
{
	int8_t stopval = atoi(value);
	saved->stopval = stopval;
	return (0);
}

/*
 * @fault_function: triggerchildexit <Child#>
 * for a single child, cause it to terminate. The child will terminate with
 * a non-zero return code.
 */
int
triggerchildexit(char *value)
{
	if (value == NULL) {
		saved->stopmeflag = 0;
	} else {
		saved->stopmeflag = atoi(value);
	}
	return (0);
}

/*
 * @fault_function: triggerchildrv <Child#> <Code>
 * get a certain child to terminate with a certain exit code.
 */
int
triggerchildrv(char *value)
{
	if (value == NULL) {
		saved->stopmeflag = SVC_ALL_STOP;
	} else {
		char *chap = strchr(value, ' ');

		if (chap != NULL)
			saved->returncode = atoi(chap+1);
		saved->stopmeflag = atoi(value);
	}
	return (0);
}

/*
 * @fault_function: dally <millisecs>
 * Pause the process for the specified time period in milli-secs;
 * default is 1/2 a second (500); There is a *minimum* of 50 msecs.
 */
int
dally(char *value)
{
	struct timespec slspec = { 0, 0 };
	uint32_t millis = 500;

	if (value != NULL) {
		millis = atoi(value);
		if (millis < 50)
			millis = 50;
	}
	if (millis >= 1000) {
		slspec.tv_sec = millis / 1000;
		millis %= 1000;
	}
	slspec.tv_nsec = millis * 1000;
	(void) nanosleep(&slspec, NULL);
	return (0);
}

/*
 * @fault_function: servicecausefork <Child#>
 * cause a specific service process to fork off a child
 */
int
servicecausefork(char *value)
{
	if (value == NULL) {
		saved->childtofork = 0;
	} else {
		saved->childtofork = atoi(value);
	}
	return (0);
}

/*
 * fault_function: servicecauseforksegv <Child#>
 * cause a service to fork off a child who segv's
 */
int
servicecauseforksegv(char *value)
{
	if (value == NULL) {
		saved->segvchild = 0;
		saved->childtofork = 0;
	} else {
		int id = atoi(value);
		saved->segvchild = id;
		saved->childtofork = id;
	}
	return (0);
}

/*
 * @fault_function: lockdown <Child#>
 * request a service's memory to be locked down in memory
 */
int
lockdown(char *value)
{
	if (value == NULL)
		saved->lockdown = 0;
	else
		saved->lockdown = atoi(value);
	return (0);
}

/*
 * @fault_function: forkexec <Child#> <exec string>
 * cause a fork-execute in a child process
 */
int
forkexec(char *value)
{
	if (value == NULL)
		saved->forkexec = 0;
	else {
		size_t len;
		int id = atoi(value);
		while (isdigit(*value)) value++;
		while (isspace(*value)) value++;
		if ((len = strlcpy(saved->commandline, value,
		    MAXPATHLEN)) > MAXPATHLEN) {
			(void) printf("command line too long(%d)\n", len);
			return (0);
		}
		saved->forkexec = id;
	}
	return (0);
}
