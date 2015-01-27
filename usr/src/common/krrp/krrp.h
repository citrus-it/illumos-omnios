/*
 * Copyright 2015 Nexenta Systems, Inc.  All rights reserved.
 */

#ifndef	_KRRP_H
#define	_KRRP_H

#include <sys/types.h>

#include "krrp_ioctl_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define	KRRP_DRIVER  "krrp"
#define	KRRP_DEVICE  "/dev/krrp"
#define	KRRP_KSTAT_ID_STRING_LENGTH 17
#define	KRRP_MIN_PORT 1000
#define	KRRP_MAX_PORT 65535
#define	KRRP_MIN_SESS_PDU_DBLK_DATA_SZ 1 * 1024
#define	KRRP_MAX_SESS_PDU_DBLK_DATA_SZ 128 * 1024

/* Min and Max values of timeout for connect() call*/
#define	KRRP_MIN_CONN_TIMEOUT 5
#define	KRRP_MAX_CONN_TIMEOUT 120

/* Min value of Connection Throughput */
#define	KRRP_MIN_CONN_THROTTLE 1 * 1000 * 1000

/* Min value of Memory Limit in MB */
#define	KRRP_MIN_MAXMEM 100

/* The maximum length of auth digest */
#define	KRRP_AUTH_DIGEST_MAX_LEN 256

typedef enum {
	KRRP_SVC_CFG_TYPE_UNKNOWN,
	KRRP_SVC_CFG_TYPE_SERVER
} krrp_cfg_type_t;

#ifdef __cplusplus
}
#endif

#endif /* _KRRP_H */
