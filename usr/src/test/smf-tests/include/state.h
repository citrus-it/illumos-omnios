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

#ifndef _STATE_H
#define	_STATE_H

/*
 * Describes the state of the daemon
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>
#include <sys/param.h>
#include <synch.h>

#define	METHNAMELENGTH	20
#define	METHNAMECOUNT	100
#define	MAXCHILD	10
#define	SVC_ALL_STOP	(~0U)
#define	SVC_NO_STOP	(~1U)

/*
 * This item is an activity.
 * It performs the method passed with the data (a string) as the parameter.
 * This allows you to invoke functions in the service to perform some of
 * the test operations.
 */
typedef struct activity {
	int		(*method)(char *);
	char		*data;
	struct activity	*next;
} activity_t;

/* chain of report functions */
typedef struct reportfn {
	char		*name;			/* saved for output */
	void		(*method)(void);	/* method to call */
	struct reportfn	*next;			/* next method to call */
} reportfn_t;

/* Volatile service state ... not persisted into the mmaped filed */
typedef struct servicestate_v {
	char		savefile[MAXPATHLEN];	/* file to save state */
	char		method[MAXPATHLEN]; /* methods to perform */
	char		servicename[MAXPATHLEN];	/* service name */
	char		serviceinst[MAXPATHLEN];	/* service instance */
	int		numchildren;	/* number of child processes */
	activity_t	*activity; /* Activity to do */
	char		activity_string[MAXPATHLEN];
					/* a new reaction for the service */
	reportfn_t	*functions;  /* a set of functions to report state */
} servicestate_v_t;

typedef struct servicestate_p {
	uint32_t	version;	/* version number */
	mutex_t		lock;
	uint32_t	service_count;	/* Count of service sub-processes */
	pid_t		service_pgid;	/* Service process group */
	pid_t		service_pid[MAXCHILD];	/* pids of the service */
	uint32_t	returncode;	/* value of the returncode from stop */
	uint32_t	stopmeflag;	/* flag to decide if it terminates */
	uint32_t	dosegv;		/* Flag to trigger SEGV */
	uint32_t	dobuserr;	/* Flag to trigger BUSERR */
	uint32_t	stopval;	/* stop with certain value */
	uint32_t	headcalled;	/* count of methods called */
	uint32_t	childtofork;	/* child# to cause fork */
	uint32_t	segvchild;	/* child# to SEGV after fork */
	uint32_t	lockdown;	/* child# to wire down memory */
	uint32_t	forkexec;	/* child# to issue fork-exec */
	char		commandline[MAXPATHLEN];
					/* command line for fork-exec */
	char		methodscalled[METHNAMECOUNT][METHNAMELENGTH];
					/* The methods that have been invoked */
	char		servicename[MAXPATHLEN];	/* service name */
	char		serviceinst[MAXPATHLEN];	/* service instance */
} servicestate_p_t;

#define	PERSIST_VERSION	((1<<4) | 6)

/* saved and unsaved service state ... saved state is guaranteed */
extern servicestate_p_t	*saved;
extern servicestate_v_t	*unsaved;

/* functions */
int read_switches(int *argc, char **argv);
void robust_lock(mutex_t *mutex);

activity_t *find_activity(activity_t *list, const char *methname);
activity_t *create_activitylist(void);
void destroy_activitylist(activity_t *list);
activity_t *add_activity(activity_t *list, const char *methname,
    int(*activity)(struct activity *), char *value);
char *get_reportfunctions(servicestate_v_t *state);
void invoke_reportfns(servicestate_v_t *state);
void record_invocation(servicestate_p_t *state, char *method);
char *last_invocation(servicestate_p_t *state);
int count_invocation(servicestate_p_t *state, char *method);
int invocation_offset(servicestate_p_t *state, char *method);
void dump_savedstate(servicestate_p_t *state);
void free_activity(activity_t *activity);
activity_t *parse_activity(char *text);

#ifdef __cplusplus
}
#endif

#endif /* _STATE_H */
