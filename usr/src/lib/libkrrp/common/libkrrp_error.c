/*
 * Copyright 2015 Nexenta Systems, Inc. All rights reserved.
 */

#include <libintl.h>
#include <sys/debug.h>
#include <string.h>
#include <stdarg.h>

#include "krrp.h"
#include "krrp_error.h"
#include "libkrrp.h"
#include "libkrrp_impl.h"
#include "libkrrp_error.h"

/* Make lint happy */
#pragma error_messages(off, E_UNDEFINED_SYMBOL, E_YACC_ERROR, \
    E_FUNC_VAR_UNUSED, E_BLOCK_DECL_UNUSED, E_RET_INT_IMPLICITLY, \
    E_NOP_ELSE_STMT)

static struct {
	krrp_errno_t krrp_errno;
	libkrrp_errno_t libkrrp_errno;
} krrp_errs[] = {
	{KRRP_ERRNO_OK, LIBKRRP_ERRNO_OK},
	{KRRP_ERRNO_UNKNOWN, LIBKRRP_ERRNO_UNKNOWN},
#define	LIBKRRP_ERRNO_EXPAND(enum_name) \
	{KRRP_ERRNO_##enum_name, LIBKRRP_ERRNO_##enum_name},
	KRNKRRP_ERRNO_MAP(LIBKRRP_ERRNO_EXPAND)
#undef	LIBKRRP_ERRNO_EXPAND
};

static size_t krrp_errs_sz = sizeof (krrp_errs) / sizeof (krrp_errs[0]);

#define	UNIX_ERRNO_EXPAND(enum_name) \
	{enum_name, "UNIX_ERRNO_"#enum_name},
static struct {
	int errno_num;
	const char *errno_str;
} unix_errnos[] = {
	{0, "UNIX_ERRNO_UNDEFINED"},
	UNIX_ERRNO_MAP(UNIX_ERRNO_EXPAND)
};
#undef UNIX_ERRNO_EXPAND

static size_t unix_errnos_sz = sizeof (unix_errnos) / sizeof (unix_errnos[0]);

#define	KRRP_ERRDESCR_SVC_ENABLE_MAP(X) \
	X(BUSY, 0, LIBKRRP_EMSG_BUSY) \

#define	KRRP_ERRDESCR_SVC_DISABLE_MAP(X) \
	X(BUSY, 0, LIBKRRP_EMSG_BUSY) \

#define	KRRP_ERRDESCR_SVC_GET_CONFIG_MAP(X) \
	X(CFGTYPE, ENOENT, LIBKRRP_EMSG_CFGTYPE_NOENT) \
	X(CFGTYPE, EINVAL, LIBKRRP_EMSG_CFGTYPE_INVAL) \
	X(INVAL, 0, LIBKRRP_EMSG_SRVNOTRUN) \

#define	KRRP_ERRDESCR_SVC_SET_CONFIG_MAP(X) \
	X(CFGTYPE, ENOENT, LIBKRRP_EMSG_CFGTYPE_NOENT) \
	X(CFGTYPE, EINVAL, LIBKRRP_EMSG_CFGTYPE_INVAL) \
	X(ADDR, EINVAL, LIBKRRP_EMSG_ADDR_INVAL) \
	X(CREATEFAIL, 0, LIBKRRP_EMSG_CREATEFAIL, strerror(unix_errno)) \
	X(BINDFAIL, 0, LIBKRRP_EMSG_BINDFAIL, strerror(unix_errno)) \
	X(LISTENFAIL, 0, LIBKRRP_EMSG_LISTENFAIL, strerror(unix_errno)) \
	X(PORT, ENOENT, LIBKRRP_EMSG_LSTPORT_NOENT) \
	X(PORT, EINVAL, LIBKRRP_EMSG_LSTPORT_INVAL, \
	    KRRP_MIN_PORT, KRRP_MAX_PORT) \
	X(BUSY, 0, LIBKRRP_EMSG_SRVRECONF) \

#define	KRRP_ERRDESCR_SESS_CREATE_MAP(X) \
	X(SESSID, ENOENT, LIBKRRP_EMSG_SESSID_NOENT) \
	X(SESSID, EINVAL, LIBKRRP_EMSG_SESSID_INVAL) \
	X(KSTATID, ENOENT, LIBKRRP_EMSG_KSTATID_NOENT) \
	X(KSTATID, EINVAL, LIBKRRP_EMSG_KSTATID_INVAL, \
	    KRRP_KSTAT_ID_STRING_LENGTH - 1) \
	X(SESS, EALREADY, LIBKRRP_EMSG_SESS_ALREADY) \
	X(SESS, EINVAL, LIBKRRP_EMSG_SESS_CREATE_INVAL) \
	X(AUTH, EINVAL, LIBKRRP_EMSG_SESS_CREATE_AUTH_INVAL, \
	    KRRP_AUTH_DIGEST_MAX_LEN - 1) \

#define	KRRP_ERRDESCR_SESS_CREATE_CONN_MAP(X) \
	X(SESSID, ENOENT, LIBKRRP_EMSG_SESSID_NOENT) \
	X(SESS, ENOENT, LIBKRRP_EMSG_SESS_NOENT) \
	X(SESS, EINVAL, LIBKRRP_EMSG_SESS_CREATE_CONN_INVAL) \
	X(SESS, EALREADY, LIBKRRP_EMSG_SESS_CONN_ALREADY) \
	X(ADDR, ENOENT, LIBKRRP_EMSG_HOST_NOENT) \
	X(ADDR, EINVAL, LIBKRRP_EMSG_HOST_INVAL) \
	X(PORT, ENOENT, LIBKRRP_EMSG_PORT_NOENT) \
	X(PORT, EINVAL, LIBKRRP_EMSG_PORT_INVAL, \
	    KRRP_MIN_PORT, KRRP_MAX_PORT) \
	X(CREATEFAIL, 0, LIBKRRP_EMSG_CREATEFAIL, strerror(unix_errno)) \
	X(SETSOCKOPTFAIL, 0, LIBKRRP_EMSG_SETSOCKOPTFAIL, \
	    strerror(unix_errno)) \
	X(CONNFAIL, 0, LIBKRRP_EMSG_CONNFAIL, strerror(unix_errno)) \
	X(SENDFAIL, 0, LIBKRRP_EMSG_SENDFAIL, strerror(unix_errno)) \
	X(RECVFAIL, 0, LIBKRRP_EMSG_RECVFAIL, strerror(unix_errno)) \
	X(UNEXPCLOSE, 0, LIBKRRP_EMSG_UNEXPCLOSE, strerror(unix_errno)) \
	X(UNEXPEND, 0, LIBKRRP_EMSG_UNEXPEND, strerror(unix_errno)) \
	X(AUTH, ENOENT, LIBKRRP_EMSG_AUTH_NOENT) \
	X(AUTH, EINVAL, LIBKRRP_EMSG_SESS_CREATE_CONN_AUTH_INVAL) \
	X(BADRESP, 0, LIBKRRP_EMSG_BADRESP) \
	X(NOMEM, 0, LIBKRRP_EMSG_NOMEM) \
	X(BIGPAYLOAD, 0, LIBKRRP_EMSG_BIGPAYLOAD) \
	X(CONNTIMEOUT, 0, LIBKRRP_EMSG_CONNTIMEOUT_INVAL, \
	    KRRP_MIN_CONN_TIMEOUT, KRRP_MAX_CONN_TIMEOUT) \

#define	KRRP_ERRDESCR_SESS_CONN_THROTTLE_MAP(X) \
	X(SESSID, ENOENT, LIBKRRP_EMSG_SESSID_NOENT) \
	X(SESS, ENOENT, LIBKRRP_EMSG_SESS_NOENT) \
	X(SESS, ENOTACTIVE, LIBKRRP_EMSG_SESS_NOTACTIVE) \
	X(SESS, EINVAL, LIBKRRP_EMSG_SESS_REMOTE_CALL_FAIL) \
	X(THROTTLE, ENOENT, LIBKRRP_EMSG_THROTTLE_NOENT) \
	X(THROTTLE, EINVAL, LIBKRRP_EMSG_THROTTLE_INVAL, \
	    KRRP_MIN_CONN_THROTTLE) \

#define	KRRP_ERRDESCR_SESS_CREATE_PDU_ENGINE_MAP(X) \
	X(SESSID, ENOENT, LIBKRRP_EMSG_SESSID_NOENT) \
	X(SESS, ENOENT, LIBKRRP_EMSG_SESS_NOENT) \
	X(SESS, EALREADY, LIBKRRP_EMSG_SESS_PDUENGINE_ALREADY) \
	X(DBLKSZ, EINVAL, LIBKRRP_EMSG_DBLKSZ_INVAL, \
	    KRRP_MIN_SESS_PDU_DBLK_DATA_SZ, KRRP_MAX_SESS_PDU_DBLK_DATA_SZ) \
	X(DBLKSZ, ENOENT, LIBKRRP_EMSG_DBLKSZ_NOENT) \
	X(MAXMEMSZ, ENOENT, LIBKRRP_EMSG_MAXMEMSZ_NOENT) \
	X(MAXMEMSZ, EINVAL, LIBKRRP_EMSG_MAXMEMSZ_INVAL, KRRP_MIN_MAXMEM) \
	X(NOMEM, 0, LIBKRRP_EMSG_SESS_PDUENGINE_NOMEM) \

#define	KRRP_ERRDESCR_SESS_CREATE_WRITE_STREAM_MAP(X) \
	X(SESSID, ENOENT, LIBKRRP_EMSG_SESSID_NOENT) \
	X(SESS, ENOENT, LIBKRRP_EMSG_SESS_NOENT) \
	X(SESS, EINVAL, LIBKRRP_EMSG_SESS_CREATE_WRITE_STREAM_FAIL) \
	X(SESS, EALREADY, LIBKRRP_EMSG_SESS_STREAM_ALREADY) \
	X(ZFSGCTXFAIL, 0, LIBKRRP_EMSG_ZFSGCTXFAIL) \
	X(CMNSNAP, EINVAL, LIBKRRP_EMSG_CMNSNAP_INVAL) \
	X(DSTDS, ENOENT, LIBKRRP_EMSG_DSTDS_NOENT) \
	X(DSTDS, EINVAL, LIBKRRP_EMSG_DSTDS_INVAL) \
	X(ZCOOKIES, EINVAL, LIBKRRP_EMSG_ZCOOKIES_INVAL) \

#define	KRRP_ERRDESCR_SESS_CREATE_READ_STREAM_MAP(X) \
	X(SESSID, ENOENT, LIBKRRP_EMSG_SESSID_NOENT) \
	X(SESS, ENOENT, LIBKRRP_EMSG_SESS_NOENT) \
	X(SESS, EINVAL, LIBKRRP_EMSG_SESS_CREATE_READ_STREAM_FAIL) \
	X(SESS, EALREADY, LIBKRRP_EMSG_SESS_STREAM_ALREADY) \
	X(FAKEDSZ, ENOENT, LIBKRRP_EMSG_FAKEDSZ_NOENT) \
	X(FAKEDSZ, EINVAL, LIBKRRP_EMSG_FAKEDSZ_INVAL) \
	X(ZFSGCTXFAIL, 0, LIBKRRP_EMSG_ZFSGCTXFAIL) \
	X(SRCDS, ENOENT, LIBKRRP_EMSG_SRCDS_NOENT) \
	X(SRCDS, EINVAL, LIBKRRP_EMSG_SRCDS_INVAL) \
	X(SRCSNAP, EINVAL, LIBKRRP_EMSG_SRCSNAP_INVAL) \
	X(CMNSNAP, EINVAL, LIBKRRP_EMSG_CMNSNAP_INVAL) \
	X(STREAM, EINVAL, LIBKRRP_EMSG_SNAP_NAMES_EQUAL) \
	X(ZCOOKIES, EINVAL, LIBKRRP_EMSG_ZCOOKIES_INVAL) \

#define	KRRP_ERRDESCR_SESS_RUN_MAP(X) \
	X(SESSID, ENOENT, LIBKRRP_EMSG_SESSID_NOENT) \
	X(SESS, ENOENT, LIBKRRP_EMSG_SESS_NOENT) \
	X(SESS, EALREADY, LIBKRRP_EMSG_SESS_STARTED) \
	X(CONN, ENOENT, LIBKRRP_EMSG_SESS_CONN_NOENT) \
	X(PDUENGINE, ENOENT, LIBKRRP_EMSG_SESS_PDUENGINE_NOENT) \
	X(STREAM, ENOENT, LIBKRRP_EMSG_STREAM_NOENT) \
	X(STREAM, EINVAL, LIBKRRP_EMSG_SESS_RUN_ONCE_INCOMPAT) \
	X(SRCDS, ENOENT, LIBKRRP_EMSG_SRCDS_NOTEXIST) \
	X(DSTDS, ENOENT, LIBKRRP_EMSG_DSTDS_NOTEXIST) \
	X(SRCSNAP, ENOENT, LIBKRRP_EMSG_SRCSNAP_NOTEXIST) \
	X(ZFSWRCBADMODE, 0, LIBKRRP_EMSG_ZFSWRCBADMODE) \
	X(ZFSWRCBADUSE, 0, LIBKRRP_EMSG_ZFSWRCBADUSE) \
	X(CMNSNAP, ENOENT, LIBKRRP_EMSG_CMNSNAP_NOTEXIST) \
	X(AUTOSNAP, EINVAL, LIBKRRP_EMSG_AUTOSNAP_INVAL) \

#define	KRRP_ERRDESCR_SESS_SEND_STOP_MAP(X) \
	X(SESSID, ENOENT, LIBKRRP_EMSG_SESSID_NOENT) \
	X(STREAM, EALREADY, LIBKRRP_EMSG_SESS_SEND_STOP_ALREADY) \
	X(SESS, ENOENT, LIBKRRP_EMSG_SESS_NOENT) \
	X(SESS, ENOTACTIVE, LIBKRRP_EMSG_SESS_NOTACTIVE) \
	X(INVAL, 0, LIBKRRP_EMSG_SESS_REMOTE_CALL_FAIL) \

#define	KRRP_ERRDESCR_SESS_DESTROY_MAP(X) \
	X(SESSID, ENOENT, LIBKRRP_EMSG_SESSID_NOENT) \
	X(SESS, ENOENT, LIBKRRP_EMSG_SESS_NOENT) \

#define	KRRP_ERRDESCR_ZFS_GET_RECV_COOKIES_MAP(X) \
	X(DSTDS, EINVAL, LIBKRRP_EMSG_DSTDS_INVAL) \
	X(DSTDS, ENOENT, LIBKRRP_EMSG_DSTDS_NOTEXIST) \
	X(ZCOOKIES, ENOENT, LIBKRRP_EMSG_ZCOOKIES_NOENT) \
	X(ZCOOKIES, 0, LIBKRRP_EMSG_ZCOOKIES_FAIL, \
	    krrp_unix_errno_to_str(unix_errno)) \

#define	KRRP_ERRDESCR_SVC_STATE_MAP(X)
#define	KRRP_ERRDESCR_SESS_STATUS_MAP(X)
#define	KRRP_ERRDESCR_SESS_LIST_MAP(X)


static libkrrp_errno_t
krrp_errno_to_libkrrp_errno(krrp_errno_t krrp_errno)
{
	size_t i;

	for (i = 0; i < krrp_errs_sz; i++) {
		if (krrp_errs[i].krrp_errno == krrp_errno)
			return (krrp_errs[i].libkrrp_errno);
	}

	return (LIBKRRP_ERRNO_UNKNOWN);
}

void
libkrrp_error_set(libkrrp_error_t *error, libkrrp_errno_t libkrrp_errno,
    int unix_errno, uint32_t flags)
{
	VERIFY(error != NULL);

	error->libkrrp_errno = libkrrp_errno;
	error->unix_errno = unix_errno;
	error->flags = flags;
}

int
libkrrp_error_from_nvl(nvlist_t *nvl, libkrrp_error_t *error)
{
	krrp_errno_t krrp_errno;
	int unix_errno;
	uint32_t flags;

	ASSERT(error != NULL);

	if (krrp_param_get(KRRP_PARAM_ERROR_CODE, nvl,
	    (void *) &krrp_errno) != 0)
		return (-1);

	if (krrp_param_get(KRRP_PARAM_ERROR_EXCODE, nvl,
	    (void *) &unix_errno) != 0)
		return (-1);

	if (krrp_param_get(KRRP_PARAM_ERROR_FLAGS, nvl,
	    (void *) &flags) != 0)
		return (-1);

	libkrrp_error_set(error, krrp_errno_to_libkrrp_errno(krrp_errno),
	    unix_errno, flags | LIBKRRP_ERRF_KERNEL);

	return (0);
}

boolean_t
libkrrp_error_cmp(libkrrp_errno_t libkrrp_errno,
    libkrrp_errno_t m_libkrrp_errno, int unix_errno, int m_unix_errno,
    int flags, char *descr, char *m_descr, ...)
{
	va_list ap;

	if ((libkrrp_errno == m_libkrrp_errno) &&
	    (m_unix_errno == 0 || unix_errno == m_unix_errno)) {
		va_start(ap, m_descr);
		(void) vsnprintf(descr, sizeof (libkrrp_error_descr_t),
		    dgettext(TEXT_DOMAIN, m_descr), ap);

		if (flags & LIBKRRP_ERRF_REMOTE) {
			(void) strlcat(descr, " (",
			    sizeof (libkrrp_error_descr_t));
			(void) strlcat(descr, dgettext(TEXT_DOMAIN,
			    LIBKRRP_EMSG_REMOTE_NODE_ERROR),
			    sizeof (libkrrp_error_descr_t));
			(void) strlcat(descr, ")",
			    sizeof (libkrrp_error_descr_t));
		}

		va_end(ap);
		return (B_TRUE);
	}

	return (B_FALSE);
}

const libkrrp_error_t *
libkrrp_error(libkrrp_handle_t *hdl)
{
	VERIFY(hdl != NULL);
	return (&hdl->libkrrp_error);
}

const char *
libkrrp_error_description(libkrrp_handle_t *hdl)
{
	/* LINTED: E_FUNC_SET_NOT_USED */
	libkrrp_errno_t libkrrp_errno;
	/* LINTED: E_FUNC_SET_NOT_USED */
	int unix_errno;
	int flags;
	krrp_ioctl_cmd_t cmd;
	char *descr;

	VERIFY(hdl != NULL);

	descr = hdl->libkrrp_error_descr;
	descr[0] = '\0';

	libkrrp_errno = hdl->libkrrp_error.libkrrp_errno;
	unix_errno = hdl->libkrrp_error.unix_errno;
	flags = hdl->libkrrp_error.flags;
	cmd = hdl->libkrrp_last_cmd;

	if (flags & LIBKRRP_ERRF_KERNEL) {
		switch (cmd) {
#define		KRRP_IOCTL_EXPAND(cmd_m) \
		case KRRP_IOCTL_##cmd_m: \
			SET_ERROR_DESCR(KRRP_ERRDESCR_##cmd_m##_MAP); \
			break;

		KRRP_IOCTL_MAP(KRRP_IOCTL_EXPAND)
#undef		KRRP_IOCTL_EXPAND
		default:
			break;
		}
	} else {
		SET_ERROR_DESCR(LIBKRRP_ERRDESCR_MAP);
	}

	if (descr[0] == '\0') {
		(void) snprintf(descr, sizeof (libkrrp_error_descr_t) - 1,
		    dgettext(TEXT_DOMAIN, LIBKRRP_EMSG_UNKNOWN));
	}

	return (descr);
}

const char
*krrp_unix_errno_to_str(int unix_errno)
{
	size_t i;
	for (i = 0; i < unix_errnos_sz; i++) {
		if (unix_errnos[i].errno_num == unix_errno)
			return (unix_errnos[i].errno_str);
	}

	return ("UNIX_ERRNO_UNKNOWN");
}
