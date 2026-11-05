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
#include "mtlib.h"
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <limits.h>
#include <pthread.h>
#include <thread.h>
#include <string.h>
#include <dirent.h>
#include <stdio.h>
#include <dlfcn.h>
#include <atomic.h>
#include <md5.h>
#include "pos4obj.h"

#define	HASHSTRLEN	32

static	char	*__pos4obj_name(const char *, const char *);
static	void	__pos4obj_md5toa(unsigned char *, unsigned char *);
static	void	__pos4obj_clean(char *);

static	char	objroot[] = "/tmp/";
static	long int	name_max = 0;

int
__open_nc(const char *path, int oflag, mode_t mode)
{
	int		cancel_state;
	int		val;
	struct stat64	statbuf;

	/*
	 * Ensure path is not a symlink to somewhere else. This provides
	 * a modest amount of protection against easy security attacks.
	 */
	if (lstat64(path, &statbuf) == 0) {
		if (S_ISLNK(statbuf.st_mode)) {
			errno = EINVAL;
			return (-1);
		}
	}

	(void) pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &cancel_state);
	val = open64(path, oflag, mode);
	(void) pthread_setcancelstate(cancel_state, NULL);

	return (val);
}

int
__close_nc(int fildes)
{
	int	cancel_state;
	int	val;

	(void) pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &cancel_state);
	val = close(fildes);
	(void) pthread_setcancelstate(cancel_state, NULL);

	return (val);
}

/*
 * This is to avoid loading libmd.so.1 unless we absolutely have to.
 */
typedef void (*md5_calc_t)(unsigned char *, unsigned char *, unsigned int);
static md5_calc_t real_md5_calc = NULL;
static mutex_t md5_lock = DEFAULTMUTEX;

static void
load_md5_calc(void)
{
	void *md5_handle = dlopen("libmd.so.1", RTLD_LAZY);
	md5_calc_t md5_calc = (md5_handle == NULL)? NULL :
	    (md5_calc_t)dlsym(md5_handle, "md5_calc");

	lmutex_lock(&md5_lock);
	if (real_md5_calc == NULL) {
		if (md5_calc == NULL)
			real_md5_calc = (md5_calc_t)(-1);
		else {
			real_md5_calc = md5_calc;
			md5_handle = NULL;	/* don't dlclose it */
		}
		membar_producer();
	}
	lmutex_unlock(&md5_lock);

	if (md5_handle)
		(void) dlclose(md5_handle);
}

static char *
__pos4obj_name(const char *path, const char *type)
{
	int	shortpath = 1;
	int	olderrno;
	size_t	len;
	char	*dfile;
	unsigned char	hashbuf[HASHSTRLEN + 1];
	unsigned char	md5_digest[MD5_DIGEST_LENGTH];

	/*
	 * If the path is path_max - strlen(type) characters or less,
	 * the name of the file to use will be the path prefixed by
	 * the type.
	 *
	 * In the special case where the path is longer than
	 * path_max - strlen(type) characters, we create a string based on the
	 * MD5 hash of the path. We prefix that string with a '.' to
	 * make it obscure, and create a directory in objroot with
	 * that name. In that directory, we create a directory named
	 * after the type of object requested.  Inside the type
	 * directory, the filename will be the path of the object. This
	 * prevents collisions in all namespaces.
	 *
	 * Example:
	 * Let objroot = "/tmp/", path = "/<longpath>", and type = ".MQD"
	 * Let the MD5 hash of "<longpath>" = "<hash>"
	 *
	 * The desired file is /tmp/.<hash>/.MQD/<longpath>
	 */

	/*
	 * Do not include the leading '/' in the path length.
	 * Assumes __pos4obj_check(path) has already been called.
	 */
	if ((strlen(path) - 1) > (name_max - strlen(type)))
		shortpath = 0;

	if (shortpath) {
		/*
		 * strlen(path) includes leading slash as space for NUL.
		 */
		len = strlen(objroot) + strlen(type) + strlen(path);
	} else {
		/*
		 * Long path name. Add 3 for extra '/', '.' and '\0'
		 */
		len = strlen(objroot) + HASHSTRLEN + strlen(type) +
		    strlen(path) + 3;
	}

	if ((dfile = malloc(len)) == NULL)
		return (NULL);

	(void) memset(dfile, 0, len);
	(void) strcpy(dfile, objroot);

	if (shortpath) {
		(void) strcat(dfile, type);
		(void) strcat(dfile, path + 1);
		return (dfile);
	}

	/*
	 * If we can successfully load it, call md5_calc().
	 * Otherwise, (this "can't happen") return NULL.
	 */
	if (real_md5_calc == NULL)
		load_md5_calc();
	if (real_md5_calc == (md5_calc_t)(-1)) {
		free(dfile);
		return (NULL);
	}

	real_md5_calc(md5_digest, (unsigned char *)path + 1, strlen(path + 1));
	__pos4obj_md5toa(hashbuf, md5_digest);
	(void) strcat(dfile, ".");
	(void) strcat(dfile, (const char *)hashbuf);

	/*
	 * Errno must be preserved across the following calls to
	 * mkdir.  This needs to be done to prevent incorrect error
	 * reporting in certain cases. When we attempt to open a
	 * non-existent object without the O_CREAT flag, it will
	 * always create a lock file first.  The lock file is created
	 * and then the open is attempted, but fails with ENOENT. The
	 * lock file is then destroyed. In the following code path, we
	 * are finding the absolute path to the lock file after
	 * already having attempted the open (which set errno to
	 * ENOENT). The following calls to mkdir will return -1 and
	 * set errno to EEXIST, since the hash and type directories
	 * were created when the lock file was created. The correct
	 * errno is the ENOENT from the attempted open of the desired
	 * object.
	 */
	olderrno = errno;

	/*
	 * Create hash directory. Use 777 permissions so everyone can use it.
	 */
	if (mkdir(dfile, S_IRWXU|S_IRWXG|S_IRWXO) == 0) {
		if (chmod(dfile, S_IRWXU|S_IRWXG|S_IRWXO) == -1) {
			free(dfile);
			return (NULL);
		}
	} else {
		if (errno != EEXIST) {
			free(dfile);
			return (NULL);
		}
	}

	(void) strcat(dfile, "/");
	(void) strcat(dfile, type);

	/*
	 * Create directory for requested type. Use 777 perms so everyone
	 * can use it.
	 */
	if (mkdir(dfile, S_IRWXU|S_IRWXG|S_IRWXO) == 0) {
		if (chmod(dfile, S_IRWXU|S_IRWXG|S_IRWXO) == -1) {
			free(dfile);
			return (NULL);
		}
	} else {
		if (errno != EEXIST) {
			free(dfile);
			return (NULL);
		}
	}

	errno = olderrno;
	(void) strcat(dfile, path);
	return (dfile);
}

/*
 * Takes a 128-bit MD5 digest and transforms to a sequence of 32 ASCII
 * characters. Output is the hexadecimal representation of the digest.
 *
 * The output buffer must be at least HASHSTRLEN + 1 characters
 * long.  HASHSTRLEN is the size of the MD5 digest (128 bits)
 * divided by the number of bits used per char of output (4). The
 * extra character at the end is for the NUL terminating character.
 */

static void
__pos4obj_md5toa(unsigned char *dest, unsigned char *src)
{
	int i;
	uint32_t *p;

	/* LINTED pointer cast may result in improper alignment */
	p = (uint32_t *)src;

	for (i = 0; i < (MD5_DIGEST_LENGTH / 4); i++)
		(void) snprintf((char *)dest + (i * 8), 9, "%.8x", *p++);

	dest[HASHSTRLEN] = '\0';
}

/*
 * This open function assume that there is no simultaneous
 * open/unlink operation is going on. The caller is supposed
 * to ensure that both open in O_CREAT mode happen atomically.
 * It returns the crflag as 1 if file is created else 0.
 */
int
__pos4obj_open(const char *name, char *type, int oflag,
		mode_t mode, int *crflag)
{
	int fd;
	char *dfile;

	errno = 0;
	*crflag = 0;

	if ((dfile = __pos4obj_name(name, type)) == NULL) {
		return (-1);
	}

	oflag |= O_CLOEXEC;

	if (!(oflag & O_CREAT)) {
		if ((fd = __open_nc(dfile, oflag, mode)) == -1)
			__pos4obj_clean(dfile);

		free(dfile);
		return (fd);
	}

	/*
	 * We need to make sure that crflag is set iff we actually create
	 * the file.  We do this by or'ing in O_EXCL, and attempting an
	 * open.  If that fails with an EEXIST, and O_EXCL wasn't specified
	 * by the caller, then the file seems to exist;  we'll try an
	 * open with O_CREAT cleared.  If that succeeds, then the file
	 * did indeed exist.  If that fails with an ENOENT, however, the
	 * file was removed between the opens;  we need to take another
	 * lap.
	 */
	for (;;) {
		if ((fd = __open_nc(dfile, (oflag | O_EXCL), mode)) == -1) {
			if (errno == EEXIST && !(oflag & O_EXCL)) {
				fd = __open_nc(dfile, oflag & ~O_CREAT, mode);

				if (fd == -1 && errno == ENOENT)
					continue;
				break;
			}
		} else {
			*crflag = 1;
		}
		break;
	}

	free(dfile);
	return (fd);
}


int
__pos4obj_unlink(const char *name, const char *type)
{
	int	err;
	char	*dfile;

	if ((dfile = __pos4obj_name(name, type)) == NULL) {
		return (-1);
	}

	err = unlink(dfile);

	__pos4obj_clean(dfile);

	free(dfile);

	return (err);
}

static int
__pos4obj_setlock(int fd, short type)
{
	struct flock	fl;
	int		ret;

	(void) memset(&fl, 0, sizeof (fl));
	fl.l_type = type;
	fl.l_whence = SEEK_SET;

	while ((ret = fcntl(fd, F_OFD_SETLKW, &fl)) == -1 && errno == EINTR)
		;

	return (ret);
}

/*
 * Serialise operations on a named object. On success this returns a descriptor
 * which must be passed to __pos4obj_unlock(), otherwise -1.
 *
 * The lock is an OFD lock on a per-object file. Waiters block in the kernel
 * rather than polling, and the lock is released if the holder dies, so a file
 * left behind by a process which was killed is harmless. A child that inherits
 * the descriptor from a fork() in another thread keeps the lock alive if the
 * holder then dies without unlocking.
 *
 * The file is removed on unlock unless it belongs to another user, since /tmp
 * is sticky. Once the lock is held, we have to check that the file is still
 * the one in the namespace, and try again if it is not.
 *
 * The file is created with mode 0666 regardless of umask so that other users
 * can lock it. An existing file may be mode restricted for a short time while
 * the creator gets to the fchmod(), or indefinitely if it came from an older
 * libc, so EACCES is retried for a limited time. /tmp is world writable, so
 * an existing file must not be followed if it is a symbolic link and is never
 * modified other than to lock it.
 */
int
__pos4obj_lock(const char *name, const char *ltype)
{
	struct stat64	fst, pst;
	char		*dfile;
	int		cancel_state;
	int		err;
	int		fd;
	int		limit = 64;

	if ((dfile = __pos4obj_name(name, ltype)) == NULL) {
		return (-1);
	}

	(void) pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &cancel_state);

	for (;;) {
		fd = __open_nc(dfile, O_RDWR | O_CREAT | O_EXCL | O_CLOEXEC,
		    0666);
		if (fd >= 0) {
			(void) fchmod(fd, 0666);
		} else if (errno == EEXIST) {
			fd = __open_nc(dfile, O_RDWR | O_NOFOLLOW | O_CLOEXEC,
			    0);
			if (fd < 0 && errno == ENOENT)
				continue;
			if (fd < 0 && errno == EACCES && limit-- > 0) {
				(void) sleep(1);
				continue;
			}
		}

		if (fd < 0)
			break;

		if (fstat64(fd, &fst) != 0) {
			goto fail;
		} else if (!S_ISREG(fst.st_mode)) {
			errno = EINVAL;
			goto fail;
		}

		if (__pos4obj_setlock(fd, F_WRLCK) != 0)
			goto fail;

		if (lstat64(dfile, &pst) == 0 && pst.st_dev == fst.st_dev &&
		    pst.st_ino == fst.st_ino) {
			break;
		}

		/* The holder we waited for removed the file. */
		(void) __close_nc(fd);
		continue;

fail:
		err = errno;
		(void) __close_nc(fd);
		errno = err;
		fd = -1;
		break;
	}

	(void) pthread_setcancelstate(cancel_state, NULL);
	free(dfile);
	return (fd);
}

/*
 * Release a lock taken with __pos4obj_lock(). The file is removed before the
 * lock is released so that the next holder never has to deal with it going
 * away. The lock is released explicitly, since a child created by a fork() in
 * another thread could have a copy of the descriptor. errno is preserved
 * because callers use this on their error paths, and removing the file is
 * allowed to fail if it was created by another user.
 */
void
__pos4obj_unlock(const char *path, const char *type, int fd)
{
	int	olderrno = errno;

	(void) __pos4obj_unlink(path, type);
	(void) __pos4obj_setlock(fd, F_UNLCK);
	(void) __close_nc(fd);

	errno = olderrno;
}

/*
 * Removes unused hash and type directories that may exist in specified path.
 */
static void
__pos4obj_clean(char *path)
{
	char	*p;
	int	olderrno;

	/*
	 * path is either
	 * 1) /<objroot>/<type><path>  or
	 * 2) /<objroot>/.<hash>/<type>/<path>
	 *
	 * In case 1, there is nothing to clean.
	 *
	 * Detect case 2 by looking for a '/' after /objroot/ and
	 * remove the two trailing directories, if empty.
	 */
	if (strchr(path + strlen(objroot), '/') == NULL)
		return;

	/*
	 * Preserve errno across calls to rmdir. See block comment in
	 * __pos4obj_name() for explanation.
	 */
	olderrno = errno;

	if ((p = strrchr(path, '/')) == NULL)
		return;
	*p = '\0';

	(void) rmdir(path);

	if ((p = strrchr(path, '/')) == NULL)
		return;
	*p = '\0';

	(void) rmdir(path);

	errno = olderrno;
}


/*
 * Check that path starts with a /, does not contain a / within it
 * and is not longer than PATH_MAX or NAME_MAX
 */
int
__pos4obj_check(const char *path)
{
	long int	i;

	/*
	 * This assumes that __pos4obj_check() is called before
	 * any of the other functions in this file
	 */
	if (name_max == 0 || name_max == -1) {
		name_max = pathconf(objroot, _PC_NAME_MAX);
		if (name_max == -1)
			return (-1);
	}

	if (*path++ != '/') {
		errno = EINVAL;
		return (-1);
	}

	for (i = 0; *path != '\0'; i++) {
		if (*path++ == '/') {
			errno = EINVAL;
			return (-1);
		}
	}

	if (i > PATH_MAX || i > name_max) {
		errno = ENAMETOOLONG;
		return (-1);
	}

	return (0);
}
