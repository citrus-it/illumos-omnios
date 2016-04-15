/*
 * Copyright 2016 Nexenta Systems, Inc.  All rights reserved.
 */

#ifndef	_KRRP_PARAMS_H_
#define	_KRRP_PARAMS_H_

#ifndef _KERNEL
#include <libnvpair.h>
#else
#include <sys/nvpair.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define	KRRP_PARAM_MAP(X) \
	X(SVC_ENABLED, BOOLEAN) \
	X(SRV_RUNNING, BOOLEAN) \
\
	X(LISTENING_ADDRESS, STRING) \
	X(REMOTE_HOST, STRING) \
	X(PORT, UINT16) \
	X(CONN_TIMEOUT, UINT32) \
\
	X(DBLK_DATA_SIZE, UINT32) \
	X(DBLK_HEAD_SIZE, UINT32) \
	X(DBLKS_PER_PDU, UINT16) \
	X(MAX_MEMORY, UINT16) \
	X(USE_PREALLOCATION, BOOLEAN) \
	X(DISABLE_FLOW_CONTROL, BOOLEAN) \
	X(FAKE_MODE, BOOLEAN) \
	X(FAKE_DATA_SIZE, UINT64) \
\
	X(SESS_ID, STRING) \
	X(SESS_KSTAT_ID, STRING) \
	X(SESS_COMPOUND, BOOLEAN) \
	X(SESS_SENDER, BOOLEAN) \
	X(SESS_RUNNING, BOOLEAN) \
	X(SESS_STARTED, BOOLEAN) \
	X(SESSIONS, NVLIST_ARRAY) \
\
	X(ERROR_SRC, UINT32) \
	X(ERROR_CODE, INT32) \
	X(ERROR_NAME, STRING) \
	X(ERROR_EXCODE, INT32) \
	X(ERROR_FLAGS, UINT32) \
	X(ERROR_MSG, STRING) \
\
	X(SRC_DATASET, STRING) \
	X(DST_DATASET, STRING) \
	X(SRC_SNAPSHOT, STRING) \
	X(COMMON_SNAPSHOT, STRING) \
	X(FORCE_RECEIVE, BOOLEAN) \
	X(SEND_RECURSIVE, BOOLEAN) \
	X(SEND_PROPERTIES, BOOLEAN) \
	X(INCLUDE_ALL_SNAPSHOTS, BOOLEAN) \
	X(ENABLE_STREAM_CHKSUM, BOOLEAN) \
	X(STREAM_EMBEDDED_BLOCKS, BOOLEAN) \
	X(IGNORE_PROPS_LIST, NVLIST) \
	X(REPLACE_PROPS_LIST, NVLIST) \
	X(RESUME_TOKEN, STRING) \
\
	X(ONLY_ONCE, BOOLEAN) \
	X(GRACEFUL_SHUTDOWN, BOOLEAN) \
	X(UEVENT_TYPE, UINT32) \
\
	X(AUTH_REQUIRED, BOOLEAN) \
	X(AUTH_DATA, STRING) \
\
	X(CFG_TYPE, INT32) \
\
	X(THROTTLE, UINT32) \
\
	X(SESS_PRIVATE_DATA, NVLIST) \
\
	X(STREAM_KEEP_SNAPS, UINT32) \
\
	X(STREAM_DISCARD_HEAD, BOOLEAN) \
	X(STREAM_LEAVE_TAIL, BOOLEAN) \

typedef enum {
	KRRP_PARAM_UNKNOWN = 0,
#define	KRRP_PARAM_EXPAND(enum_name, dtype) KRRP_PARAM_##enum_name,
	KRRP_PARAM_MAP(KRRP_PARAM_EXPAND)
#undef KRRP_PARAM_EXPAND
	KRRP_PARAM_LAST /* To exclude lint-errors */
} krrp_param_t;

typedef struct krrp_param_array_s {
	nvlist_t	**array;
	uint_t		nelem;
} krrp_param_array_t;

int krrp_param_get(krrp_param_t, nvlist_t *, void *);
int krrp_param_put(krrp_param_t, nvlist_t *, void *);
boolean_t krrp_param_exists(krrp_param_t, nvlist_t *);

#ifdef __cplusplus
}
#endif

#endif /* _KRRP_PARAMS_H_ */
