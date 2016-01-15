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
 * Log errors and messages to the log file
 */

#if defined(_FILE_OFFSET_BITS)
#undef _FILE_OFFSET_BITS
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <strings.h>
#include <unistd.h>
#include <messages.h>
#include <thread.h>
#include <synch.h>
#include <string.h>
#include <door.h>
#include <sys/socket.h>
#include <netdb.h>
#include <ucontext.h>
#include <errno.h>
#include <dlfcn.h>
#include <pwd.h>
#include <grp.h>
#include <priv.h>
#include <pool.h>
#define	_STRUCTURED_PROC 1
#include <sys/procfs.h>


char *
find_argv0(void)
{
	int fh;
	intptr_t offset;
	psinfo_t psbuff;
	static char *argv0 = NULL;
	static mutex_t lock = DEFAULTMUTEX;
	char *chap;

	(void) mutex_lock(&lock);
	if (argv0 != NULL) {
		(void) mutex_unlock(&lock);
		return (argv0);
	}

	fh = open("/proc/self/psinfo", O_RDONLY);
	if (fh == -1) {
		(void) mutex_unlock(&lock);
		return ("??");
	}
	if (read(fh, &psbuff, sizeof (psbuff)) == -1) {
		(void) mutex_unlock(&lock);
		(void) close(fh);
		return ("??");
	}
	(void) close(fh);
	offset = (intptr_t)psbuff.pr_argv;
	argv0 = *(char **)offset;
	/* strip path to name */
	chap = strrchr(argv0, '/');
	if (chap != NULL) argv0 = chap+1;
	(void) mutex_unlock(&lock);
	return (argv0);
}

typedef void (*log_call_t)(char *, size_t);

int loghandle = -1;

void
log_filepipe(char *string, size_t length)
{
	ssize_t writelen;
	if (loghandle != -1) {
		while (length > 0) {
			writelen = write(loghandle, string, length);
			if (writelen == -1) {
				/* invalidate handle, redirect to stderr */
				perror(__FILE__ ":log_filepipe:write");
				(void) printstack(fileno(stderr));
				loghandle = -1;
				continue;
			} else {
				string += writelen;
				length -= writelen;
			}
		}
		(void) fsync(loghandle);
	} else {
		(void) write(fileno(stderr), string, length);
		(void) fsync(fileno(stderr));
	}
}

/* default loggin is to a file/pipe ... stderr */
static log_call_t log_method = log_filepipe;

log_call_t
log_openfile(char *filename)
{
	int fh;
	/*
	 * we want symlink resolution
	 * a pipe will delay here if there is no reader
	 */
	fh = open(filename, O_CREAT | O_WRONLY | O_APPEND,
	    S_IRUSR | S_IWUSR |
	    S_IRGRP | S_IWGRP |
	    S_IROTH | S_IWOTH);

	if (-1 == fh) {
		(void) fprintf(stderr, __FILE__ ":open_file:could not "
		    "open %s for writing ... default to stderr", filename);
		return (log_filepipe);
	}
	loghandle = fh;
	return (log_filepipe);
}

/* To reduce the overhead here we should outsource this to a library */
void
log_door(char *string, size_t length)
{
	door_arg_t parms;
	bzero((char *)&parms, sizeof (door_arg_t));

	parms.data_ptr = string;
	parms.data_size = length;

	if (loghandle != -1) {
		if (-1 != door_call(loghandle, &parms))
			return;
		else {
			perror(__FILE__ ":log_door:door_call");
			(void) printstack(fileno(stderr));
			loghandle = -1;
		}
	}

	(void) write(fileno(stderr), string, length);
}

log_call_t
log_opendoor(char *doorname)
{
	int fd;

	fd = open(doorname, O_RDWR);
	if (fd == -1) {
		perror(__FILE__ ":log_opendoor:open");
		return (log_filepipe);
	}
	return (log_door);
}

/* generic socket address ... covers both IPv4 and IPv6 addresses */
struct sockaddr daddr;

/* Log to a TCP / UDP / Multicast socket */
void
log_socket(char *string, size_t length)
{
	size_t left = length;
	ssize_t sent = 0;

	if (loghandle != -1) {
		while (left > 0) {
			sent = sendto(loghandle, string,
			    (length > left) ? length : left, 0,
			    &daddr, sizeof (daddr));
			if (sent != -1) {
				string += sent;
				length -= sent;
				left -= sent;
				continue;
			} else {
				if (errno == EMSGSIZE) {
					length /= 2;
					continue;
				}
				perror(__FILE__ ":log_door:door_call");
				(void) printstack(fileno(stderr));
				loghandle = -1;
				goto retme;
			}
		}
	}
retme:
	(void) write(fileno(stderr), string, length);
}

log_call_t
log_opensocket(char *sockname)
{
	char *pcolon = strchr(sockname, '@');
	char *pmach;
	enum socktypes { st_tcp, st_udp, st_mcast, st_wtf } socktype;
	in_port_t port;
	struct hostent *hent;
	int erron;
	int trycount;
	log_call_t rv = log_filepipe;
	int domain;
	int type;
	int protocol;
	int reuse = 1;

	if (pcolon == NULL) {
		(void) fprintf(stderr, __FILE__ ":log_opensocket:malformed "
		    "socket definition %s\n", sockname);
		return (rv);
	}
	*pcolon = '\0';
	if (strcasecmp(sockname, "TCP") == 0) socktype = st_tcp;
	else if (strcasecmp(sockname, "UDP") == 0) socktype = st_udp;
	else if (strcasecmp(sockname, "MCP") == 0) socktype = st_mcast;
	else socktype = st_wtf;
	pmach = pcolon+1;
	pcolon = strchr(pmach, ':');
	if (pcolon == NULL) {
		(void) fprintf(stderr, __FILE__ ":log_opensocket:bad "
		    "machine:port definition %s\n", sockname);
		return (rv);
	}
	*pcolon = '\0';
	sockname = pcolon+1;
	port = (in_port_t)atoi(sockname);
	if (port == 0) { /* get serv by name */
		struct servent retval;
		char *buffer = malloc(1024);
		struct servent *ret = getservbyname_r(sockname,
		    socktype == st_tcp ? "tcp" : "udp", &retval, buffer, 1024);
		if (ret == NULL) {
			free(buffer);
			(void) fprintf(stderr, __FILE__ ":log_opensocket:could "
			    "not find a service called %s\n", sockname);
			return (rv);
		}
		/* address is in network order */
		port = (in_port_t)retval.s_port;
		free(buffer);
	} else {
		port = htons(port);
	}

	/* host name lookup */
lookup:
	trycount = 0;
	hent = getipnodebyname(pmach, AF_INET6, AI_DEFAULT, &erron);
	if (hent == NULL) {
		if (erron == HOST_NOT_FOUND) {
			(void) fprintf(stderr, __FILE__ ":log_opensocket:could "
			    "not find host %s\n", pmach);
		} else if (erron == NO_DATA) {
			(void) fprintf(stderr, __FILE__ ":log_opensocket: no "
			    "data for %s could be found", pmach);
		} else if (erron == NO_RECOVERY) {
			(void) fprintf(stderr, __FILE__ ":log_opensocket: "
			    "fatal lookup error\n");
		} else if (erron == TRY_AGAIN) {
			if (++trycount < 5)
				goto lookup;
		}
		return (rv);
	}
	bzero((char *)&daddr, sizeof (daddr));
	if (hent->h_length == 4) { /* IPv4 */
		/* LINTED */
		struct sockaddr_in *ia = (struct sockaddr_in *)&daddr;
		ia->sin_family = PF_INET;
		ia->sin_port = port;
		bcopy(hent->h_addr_list[0], &(ia->sin_addr), hent->h_length);
		protocol = PF_INET;
		domain = AF_INET;
	} else { /* IPv6 */
		/* LINTED */
		struct sockaddr_in6 *ia = (struct sockaddr_in6 *)&daddr;
		ia->sin6_family = PF_INET6;
		ia->sin6_port = port;
		bcopy(hent->h_addr_list[0], &(ia->sin6_addr), hent->h_length);
		protocol = PF_INET6;
		domain = AF_INET6;
	}
	freehostent(hent);

	switch (socktype) {
		case st_tcp:
			type = SOCK_STREAM;
			break;
		default:
			type = SOCK_DGRAM;
	}

	loghandle = socket(domain, type, protocol);
	if (loghandle == -1) {
		(void) fprintf(stderr, __FILE__ ":log_opensocket:could not "
		    "create socket for domain %d, type %d, protocol %d\n",
		    domain, type, protocol);
		return (rv);
	}

	if (-1 == setsockopt(loghandle, SOL_SOCKET, SO_REUSEADDR, &reuse,
	    sizeof (reuse))) {
		(void) fprintf(stderr, __FILE__ ":log_opensocket:could not "
		    "set socket reuseaddress option\n");
		return (rv);
	}
	if (socktype == st_tcp) {
		if (-1 == connect(loghandle, &daddr, sizeof (daddr))) {
			(void) fprintf(stderr, __FILE__ ":log_opensocket:could "
			    "not connect to remote machine %s:%s\n", pmach,
			    sockname);
		return (rv);
		}
	}
	rv = log_socket;

	return (rv);
}

static void
iilog_generic(const char *prefix, const char *message, va_list args)
{
	size_t bs = getpagesize();
	/* XXX: is there a faster/better way of doing this */
	char *buffer = (char *)malloc(bs);
	size_t charsprinted;
	bs--;

	charsprinted = snprintf(buffer, bs, "[%s(%ld, %ld)] %s ",
	    find_argv0(), (long)getpid(), (long)thr_self(), prefix);
	charsprinted += vsnprintf(buffer+charsprinted, bs-charsprinted,
	    message, args);
	charsprinted += snprintf(buffer+charsprinted, bs-charsprinted, "\n");
	log_method(buffer, charsprinted);
	free(buffer);
}

/*
 * Defined but not used.  Comment out for now
 *
 *static void
 *logmessage(const char *prefix, const char *header, ...)
 *{
 *	va_list elts;
 *
 *	va_start(elts, header);
 *	iilog_generic(prefix, header, elts);
 *	va_end(elts);
 *}
 */

static void
ilog_generic(logtype_t type, const char *message, va_list args)
{
	char *lm = NULL;

	switch (type) {
		case lt_progress:
			lm = "*PROGRESS*";
			break;
		case lt_info:
			lm = "*INFO*";
			break;
		case lt_warn:
			lm = "*WARNING*";
			break;
		case lt_error:
			lm = "*ERROR*";
			break;
		default:
			lm = "*UNKNOWN LOGGING TYPE*";
			break;
	}
	iilog_generic(lm, message, args);
}

/*
 * set up the log to be used. Operates on the protocol:(entry)
 * principle. It looks for a function called log_open<protocol>
 * to call to set up the log.
 */
void
log_setup(char *logname)
{
	char *colonpos;
	char lookup[MAXPATHLEN];
	int (*logfn)(char *);

	/* degenerate case */
	if (*logname == '/') {
		(void) log_openfile(logname);
		return;
	}

	colonpos = strchr(logname, ':');
	if (colonpos == NULL) {
		(void) log_openfile(logname);
		return;
	}
	*colonpos = '\0';
	(void) snprintf(lookup, MAXPATHLEN, "log_open%s", logname);
	logfn = (int(*)(char *))dlsym(RTLD_SELF, lookup);
	if (logfn == NULL) {
		(void) printf("unknown protocol %s ... reverting to stderr\n",
		    logname);
		return;
	} else {
		(void) logfn(++colonpos);
	}
}

/*
 * structured message
 * [proc(tid, pid)] SERVICE <svname> method <method> level <service|instance>
 * [instance]
 *
 */
void
log_service(const char *message, ...)
{
	va_list elts;

	va_start(elts, message);
	iilog_generic("SERVICE", message, elts);
	va_end(elts);
}

void
log_monitor(const char *message, ...)
{
	va_list elts;

	va_start(elts, message);
	iilog_generic("MONITOR", message, elts);
	va_end(elts);
}

#define	LOG_FNT(XX)	void \
log_##XX(const char *message, ...) \
{ \
	va_list	elts; \
	va_start(elts, message); \
	ilog_generic(lt_##XX, message, elts); \
	va_end(elts); \
}

LOG_FNT(progress)
LOG_FNT(info)
LOG_FNT(warn)
LOG_FNT(error)
