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

#include "lint.h"
#include <sys/types.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <limits.h>
#include <errno.h>
#include "pos4obj.h"

int
shm_open(const char *path, int oflag, mode_t mode)
{
	int crflag;
	int fd;
	int lockfd;

	if (__pos4obj_check(path) == -1)
		return (-1);

	/* acquire semaphore lock to have atomic operation */
	if ((lockfd = __pos4obj_lock(path, SHM_LOCK_TYPE)) < 0)
		return (-1);

	fd = __pos4obj_open(path, SHM_DATA_TYPE, oflag, mode, &crflag);

	if (fd < 0) {
		__pos4obj_unlock(path, SHM_LOCK_TYPE, lockfd);
		return (-1);
	}

	/* relase semaphore lock operation */
	__pos4obj_unlock(path, SHM_LOCK_TYPE, lockfd);

	return (fd);
}

int
shm_unlink(const char *path)
{
	int	err;
	int	lockfd;

	if (__pos4obj_check(path) < 0)
		return (-1);

	if ((lockfd = __pos4obj_lock(path, SHM_LOCK_TYPE)) < 0)
		return (-1);

	err = __pos4obj_unlink(path, SHM_DATA_TYPE);

	__pos4obj_unlock(path, SHM_LOCK_TYPE, lockfd);

	return (err);

}
