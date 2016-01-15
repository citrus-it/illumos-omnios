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
 * parse the arguments for the daemon and pass them on to the
 * functional elements.
 */

#include <sys/types.h>
#include <sys/task.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <state.h>
#include <service_functionality.h>
#include <messages.h>
#include <errno.h>
#include <string.h>
#include <dlfcn.h>
#include <priv.h>
#include <pwd.h>
#include <grp.h>
#include <libscf.h>
#include <pool.h>
#include <project.h>

int start_method(void);
int stop_method(void);
int dummy_method(char *methname);

void log_method(char *methname);
void userinfo(void);
void privinfo(void);
void poolinfo(void);
void myfmri(void);

int
main(int argc, char **argv)
{
	int (*meth)(void);
	char methodtocall[MAXPATHLEN];
	char *reportfunctions;

	if (read_switches(&argc, argv)) {
		log_error("reading command line switches failed");
		exit(1);
	}
	if (unsaved->method == NULL || *unsaved->method == '\0') {
		log_error("Didn't pass a method to invoke\n");
		exit(1);
	}
	log_progress("service name: %s", unsaved->servicename);
	log_progress("method name: %s", unsaved->method);
	reportfunctions = get_reportfunctions(unsaved);
	log_progress("functions name: %s", reportfunctions);
	free(reportfunctions);
	invoke_reportfns(unsaved);

	(void) snprintf(methodtocall, MAXPATHLEN, "%s_method", unsaved->method);

	meth = (int(*)(void))dlsym(RTLD_SELF, methodtocall);

	if (meth == NULL) {
		return (dummy_method(unsaved->method));
	}
	return (meth());
}

/*
 * We need to fork and start the service program here.
 */
int
start_method(void)
{
	int rv;

	log_method("start");
	rv = process_activity("start");
	if (rv != 0)
		return (rv);
	(void) perform_service();
	return (process_activity("start"));
}

/*
 * Log a method being invoked
 */
void
log_method(char *methname)
{
	record_invocation(saved, methname);
	log_service("<invoke service=\"%s\" instance=\"%s\" method=\"%s\" />",
	    unsaved->servicename, unsaved->serviceinst, methname);
}

/*
 * dummy method, invokes in case of a non functional method
 */
int
dummy_method(char *methname)
{
	log_method(methname);
	return (process_activity(methname));
}

typedef enum {
	chk_userid = 1,
	chk_username,
	chk_groupid,
	chk_groupname,
	chk_suppgroupids,
	chk_suppgroupnames,
	chk_all
} chunks_t;

void
loguserinfochunk(chunks_t chunks)
{
	char *passbuffer;
	char *groupbuffer;
	long passbufsize = sysconf(_SC_GETPW_R_SIZE_MAX);
	long groupbufsize = sysconf(_SC_GETGR_R_SIZE_MAX);
	long max_groups = sysconf(_SC_NGROUPS_MAX);
	struct passwd passwdinfo;
	struct group groupinfo;
	char *username = "??";
	char *groupname = username;
	char value[MAXPATHLEN];
	char *string;
	char *supp_groups;
	char *supp_gids;
	gid_t *grps;
	int gcount;

	passbuffer = calloc(1, passbufsize);
	groupbuffer = calloc(1, groupbufsize);
	grps = calloc(sizeof (gid_t), max_groups);
	supp_groups = calloc(sysconf(_SC_LOGIN_NAME_MAX) + 1, max_groups);
	supp_gids = calloc(sysconf(_SC_LOGIN_NAME_MAX) + 1, max_groups);

	if (getpwuid_r(getuid(), &passwdinfo, passbuffer,
	    (int)passbufsize) != NULL) {
		username = passwdinfo.pw_name;
	}

	if (getgrgid_r(getgid(), &groupinfo, groupbuffer,
	    (int)groupbufsize) != NULL) {
		groupname = strdup(groupinfo.gr_name);
	} else {
		groupname = strdup("??");
	}

	if ((gcount = getgroups(sysconf(_SC_NGROUPS_MAX), grps)) != -1) {
		int i;
		size_t lin = 0;
		size_t lin2 = 0;
		char *xch = "";
		char *xch2 = "";
		for (i = 0; i < gcount; i++) {
			/* LINTED E_SEC_SPRINTF_UNBOUNDED_COPY */
			lin += sprintf(supp_gids + lin, "%s%d", xch, grps[i]);
			if (*xch == '\0') xch = ",";

			if (getgrgid_r(grps[i], &groupinfo, groupbuffer,
			    (int)groupbufsize) != NULL) {
				/* LINTED E_SEC_SPRINTF_UNBOUNDED_COPY */
				lin2 += sprintf(supp_groups + lin2, "%s%s",
				    xch2, groupinfo.gr_name);
				if (*xch2 == '\0') xch2 = ",";
			}
		}
	}

	if (chunks == chk_all) {
		log_service("<userinfo service=\"%s\" instance=\"%s\" "
		    "method=\"%s\" userid=\"%ld\" username=\"%s\" "
		    "groupid=\"%ld\" groupname=\"%s\" "
		    "suppgroupnames=\"%s\" suppgroupids=\"%s\"/>",
		    unsaved->servicename, unsaved->serviceinst,
		    unsaved->method, (long)getuid(), username,
		    (long)getgid(), groupname, supp_groups, supp_gids);
	} else {
		switch (chunks) {
		case chk_userid:
			(void) snprintf(value, MAXPATHLEN -1, "%ld",
			    (long)getuid());
			string = "userid";
			break;
		case chk_username:
			(void) snprintf(value, MAXPATHLEN - 1, "%s", username);
			string = "username";
			break;
		case chk_groupid:
			(void) snprintf(value, MAXPATHLEN -1, "%ld",
			    (long)getgid());
			string = "groupid";
			break;
		case chk_groupname:
			(void) snprintf(value, MAXPATHLEN - 1, "%s", groupname);
			string = "groupname";
			break;
		case chk_suppgroupids:
			(void) snprintf(value, MAXPATHLEN - 1, "%s", supp_gids);
			string = "suppgroupids";
			break;
		case chk_suppgroupnames:
			(void) snprintf(value, MAXPATHLEN - 1, "%s", supp_groups);
			string = "suppgroupnames";
			break;
		default:
			(void) snprintf(value, MAXPATHLEN - 1, "??");
			string = "??";
			break;
		}
		log_service("<%s service=\"%s\" instance=\"%s\" "
		    "method=\"%s\" %s=\"%s\" />",
		    string, unsaved->servicename, unsaved->serviceinst,
		    unsaved->method, string, value);
	}

	free(passbuffer);
	free(groupbuffer);
	free(grps);
	free(supp_groups);
	free(supp_gids);
	free(groupname);
}

/*
 * @log_function: userinfo
 * Log all user and group information in a useful format
 */
void
userinfo(void)
{
	loguserinfochunk(chk_all);
}

/*
 * @log_function: groupid
 * Log the group id
 */
void
groupid(void)
{
	loguserinfochunk(chk_groupid);
}

/*
 * @log_function: groupname
 * Log the primary group name
 */
void
groupname(void)
{
	loguserinfochunk(chk_groupname);
}

/*
 * @log_function: userid
 * Log the user id
 */
void
userid(void)
{
	loguserinfochunk(chk_userid);
}

/*
 * @log_function: username
 * Log the user name
 */
void
username(void)
{
	loguserinfochunk(chk_username);
}

/*
 * @log_function: suppgroupnames
 * Log the names of the supplemental groups
 */
void
suppgroupnames(void)
{
	loguserinfochunk(chk_suppgroupnames);
}

/*
 * @log_function: suppgroupids
 * Log the ids of the supplemental groups
 */
void
suppgroupids(void)
{
	loguserinfochunk(chk_suppgroupids);
}

const struct priv_tostring {
	priv_ptype_t priv;
	char *string;
} privstrings[] = {
	{ PRIV_EFFECTIVE, "privileges" },
	{ PRIV_LIMIT, "limit_privileges" },
	{ PRIV_PERMITTED, "permitted_privileges" },
	{ PRIV_INHERITABLE, "inheritable_privileges" }
};

#define	PRIVS_COUNT	(sizeof (privstrings) / sizeof (privstrings[0]))

/*
 * log one of the privilege sets
 */
static void
a_privinfo(priv_ptype_t type)
{
	priv_set_t *priv_set;
	char *privsetstr;
	char *tag = NULL;
	int i;

	priv_set = priv_allocset();

	for (i = 0; i < PRIVS_COUNT; i++)
		if (strcmp(type, privstrings[i].priv) == 0) {
			tag = privstrings[i].string;
			break;
		}

	if (tag == NULL) {
		tag = "?huh?";
	}
	if (getppriv(type, priv_set) == -1) {
		log_error("PRIVILEGES: FAILED getppriv: %s", strerror(errno));
		priv_freeset(priv_set);
		return;
	}
	privsetstr = priv_set_to_str(priv_set, ',', PRIV_STR_LIT);
	if (privsetstr == NULL) {
		log_error("PRIVILEGES: FAILED priv_set_to_str: %s",
		    strerror(errno));
		priv_freeset(priv_set);
		return;
	}
	log_service("<%s service=\"%s\" instance=\"%s\" method=\"%s\" "
	    "%s=\"%s\" />", tag, unsaved->servicename, unsaved->serviceinst,
	    unsaved->method, tag, privsetstr);
	priv_freeset(priv_set);
	free(privsetstr);
}

/*
 * @log_function: privinfo
 * log effective privileges attributes for process
 */
void
privinfo(void)
{
	a_privinfo(PRIV_EFFECTIVE);
}

/*
 * @log_function: limit_privinfo
 * log the limit set of the privileges information
 */
void
limit_privinfo(void)
{
	a_privinfo(PRIV_LIMIT);
}

/*
 * @log_function: permitted_privinfo
 * log the permitted privilege information
 */
void
permitted_privinfo(void)
{
	a_privinfo(PRIV_PERMITTED);
}

/*
 * @log_function: inheritable_privinfo
 * log the inheritable privileges information
 */
void
inheritable_privinfo(void)
{
	a_privinfo(PRIV_INHERITABLE);
}


/*
 * @log_function: projectid
 * log project id
 */
void
projectid(void)
{
	projid_t projid = getprojid();
	log_service("<projectid service=\"%s\" instance=\"%s\" method=\"%s\" "
	    "projectid=\"%ld\" />", unsaved->servicename,
	    unsaved->serviceinst, unsaved->method, (long)projid);
}

/*
 * @log_function: projectname
 * log project name
 */
void
projectname(void)
{
	projid_t projid = getprojid();
	long page = sysconf(_SC_PAGESIZE);
	void *projbuffer;
	struct project proj;
	char *projname = NULL;

	projbuffer = malloc(page);
	if (projbuffer != NULL) {
		if (NULL != getprojbyid(projid, &proj, projbuffer, page)) {
			projname = proj.pj_name;
		} else {
			projname = "??";
		}
	}
	log_service("<projectname service=\"%s\" instance=\"%s\" method=\"%s\" "
	    "projectname=\"%s\" />", unsaved->servicename,
	    unsaved->serviceinst, unsaved->method, projname);
	if (projbuffer != NULL) free(projbuffer);
}

/*
 * @log_function: poolinfo
 * log resource pool info
 */
void
poolinfo(void)
{
	char *pool = pool_get_binding(getpid());

	log_service("<poolinfo service=\"%s\" instance=\"%s\" "
	    "method=\"%s\" poolinfo=\"%s\" />",
	    unsaved->servicename, unsaved->serviceinst, unsaved->method,
	    pool ? pool : "--INVALID--");
	if (pool)
		free(pool);
}

/*
 * @log_function: myfmri
 * Log the fmri of a process
 */
void
myfmri(void)
{
	char *buffer = NULL;
	scf_handle_t *handle = NULL;

	handle = scf_handle_create(SCF_VERSION);
	if (handle != NULL) {
		if (scf_handle_bind(handle) == SCF_SUCCESS) {
			buffer = (char *)calloc(1,
			    scf_limit(SCF_LIMIT_MAX_FMRI_LENGTH)+1);
			(void) scf_myname(handle, buffer,
			    scf_limit(SCF_LIMIT_MAX_FMRI_LENGTH));
			(void) scf_handle_unbind(handle);
		}
		scf_handle_destroy(handle);
	}
	log_service("<myfmri service=\"%s\" instance=\"%s\" method=\"%s\" "
	    "myfmri=\"%s\" />", unsaved->servicename, unsaved->serviceinst,
	    unsaved->method, buffer != NULL ? buffer : "??");
	if (buffer != NULL) free(buffer);
}

/*
 * @log_function: mycwd
 * Log the working directory of the process
 */
void
mycwd(void)
{
	char *buffer;
	size_t size;

	size = (size_t)pathconf("/", _PC_PATH_MAX);

	buffer = getcwd(NULL, size);

	if (buffer == NULL) {
		log_service("getcwd failed with: %s", strerror(errno));
	}
	log_service("<mycwd service=\"%s\" instance=\"%s\" method=\"%s\" "
	    "mycwd=\"%s\" />", unsaved->servicename, unsaved->serviceinst,
	    unsaved->method, buffer != NULL ? buffer : "??");

	if (buffer != NULL)
		free(buffer);
}
