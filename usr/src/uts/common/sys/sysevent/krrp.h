/*
 * Copyright 2015 Nexenta Systems, Inc.  All rights reserved.
 */

#ifndef _SYS_SYSEVENT_KRRP_H
#define	_SYS_SYSEVENT_KRRP_H

#ifdef __cplusplus
extern "C" {
#endif

#define	KRRP_EVENT_CHANNEL		"com.nexenta:krrp"
#define	KRRP_EVENT_VENDOR		"com.nexenta"
#define	KRRP_EVENT_PUBLISHER		"krrp"

#define	EC_KRRP				"EC_krrp"

#define	ESC_KRRP_SESS_SEND_DONE		"ESC_KRRP_sess_send_done"
#define	ESC_KRRP_SESS_ERROR		"ESC_KRRP_sess_error"
#define	ESC_KRRP_SERVER_ERROR		"ESC_KRRP_server_error"

#ifdef __cplusplus
}
#endif

#endif /* _SYS_SYSEVENT_KRRP_H */
