/*
 * Copyright 2015 Nexenta Systems, Inc. All rights reserved.
 */

#include <unistd.h>
#include <umem.h>
#include <fcntl.h>

#include "krrp_error.h"
#include <sys/krrp.h>
#include "libkrrp_impl.h"
#include "libkrrp.h"

boolean_t
is_krrp_supported(void)
{
	return (access(KRRP_DEVICE, 0) == 0);
}

libkrrp_handle_t *
libkrrp_init(void)
{
	libkrrp_handle_t *hdl;

	hdl = umem_zalloc(sizeof (libkrrp_handle_t), UMEM_DEFAULT);
	if (hdl == NULL)
		return (NULL);

	if ((hdl->libkrrp_fd = open(KRRP_DEVICE, O_RDONLY)) < 0) {
		umem_free(hdl, sizeof (libkrrp_handle_t));
		return (NULL);
	}

	return (hdl);
}

void
libkrrp_fini(libkrrp_handle_t *hdl)
{
	(void) close(hdl->libkrrp_fd);
	umem_free(hdl, sizeof (libkrrp_handle_t));
}

void
libkrrp_reset(libkrrp_handle_t *hdl)
{
	hdl->libkrrp_error.libkrrp_errno = LIBKRRP_ERRNO_OK;
	hdl->libkrrp_error_descr[0] = '\0';
	hdl->libkrrp_error.flags = 0;
}
