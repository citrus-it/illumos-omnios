/*
 * Copyright 2015 Nexenta Systems, Inc.  All rights reserved.
 */

#ifndef	_KRRP_IOCTL_H_
#define	_KRRP_IOCTL_H_

#include <sys/sysmacros.h>
#include <sys/types.h>
#include <sys/kmem.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/modctl.h>
#include <sys/class.h>
#include <sys/cmn_err.h>

#include <krrp_error.h>
#include <krrp_ioctl_common.h>

#ifdef __cplusplus
extern "C" {
#endif

int krrp_ioctl_validate_cmd(krrp_ioctl_cmd_t cmd);
int krrp_ioctl_process(krrp_ioctl_cmd_t cmd, nvlist_t *input,
    nvlist_t *output, krrp_error_t *error);

#ifdef __cplusplus
}
#endif

#endif /* _KRRP_IOCTL_H_ */
