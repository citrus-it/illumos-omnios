/*
 * Copyright 2015 Nexenta Systems, Inc. All rights reserved.
 */
#ifndef	DMU_KRRP_H
#define	DMU_KRRP_H

#ifdef	__cplusplus
extern "C" {
#endif

int dmu_krrp_buffer_write(void *buf, int len,
    dmu_krrp_task_t *krrp_task);
int dmu_krrp_buffer_read(void *buf, int len,
    dmu_krrp_task_t *krrp_task);
int dmu_krrp_arc_bypass(void *buf, int len, void *arg);
int dmu_krrp_get_recv_cookie(const char *pool, const char *token, char *cookie,
    size_t len);
int dmu_krrp_erase_recv_cookie(const char *pool, const char *token);

typedef int (*dmu_krrp_arc_bypass_cb)(void *, int, dmu_krrp_task_t *);
typedef struct {
	dmu_krrp_task_t *krrp_task;
	zio_cksum_t *zc;
	dmu_krrp_arc_bypass_cb cb;
} dmu_krrp_arc_bypass_t;

#ifdef	__cplusplus
}
#endif

#endif
