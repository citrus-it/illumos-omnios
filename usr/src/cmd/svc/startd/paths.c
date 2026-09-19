/*
 * This file and its contents are supplied under the terms of the
 * Common Development and Distribution License ("CDDL"), version 1.0.
 * You may only use this file in accordance with the terms of version
 * 1.0 of the CDDL.
 *
 * A full copy of the text of the CDDL should have accompanied this
 * source.  A copy of the CDDL is also available via the Internet at
 * http://www.illumos.org/license/CDDL.
 */

/*
 * Copyright 2026 Oxide Computer Company
 */

/*
 * paths.c - managed path processing
 *
 * A service may include one or more managed_paths blocks in its manifest,
 * at both service and instance level. Each block holds an ordered list of
 * directory entries which svc.startd ensures exist, and optionally are
 * empty, before the service's start method runs.
 *
 * The entries are stored in a framework property group called
 * "managed_paths" as sets of numbered properties (path_0, user_0 and so
 * on) and are processed in index order. Service level entries are
 * processed before instance level entries, and so the property group is
 * read from each snaplevel of the running snapshot rather than through
 * the usual composed view. The composed view would hide the service
 * level group whenever the instance also defines one.
 *
 * Each directory is created if it does not already exist, together with
 * any missing intermediate directories. Intermediates which have to be
 * created are owned by root:root with mode 0755. Ones which already
 * exist are not modified. The final directory always has the requested
 * ownership and permissions applied, whether or not it already existed.
 * Ownership defaults to the user and group of the method credential
 * which applies to the start method. Symbolic links are rejected
 * anywhere in the path so that this code, which runs with full
 * privilege, cannot be redirected elsewhere in the filesystem. An entry
 * may also request that an existing directory is emptied before the
 * service starts, for services which expect a fresh directory (PID files
 * and the like), and may ask for the expanded path to be placed in the
 * method environment.
 *
 * Failures here are treated like an invalid method context. The error is
 * logged to the instance log and the instance is placed in maintenance.
 */

#include <sys/stat.h>
#include <sys/types.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <librestart.h>
#include <libscf.h>
#include <libuutil.h>
#include <limits.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "startd.h"

/* Default permissions for a managed directory */
#define	MP_DEFAULT_MODE		0770
/* Permissions for any intermediate directories which are created */
#define	MP_INTERMEDIATE_MODE	0755
/* Limit on recursion depth when emptying a directory */
#define	MP_EMPTY_MAXDEPTH	128
/* Limit on rescan passes when emptying a directory */
#define	MP_EMPTY_MAXPASSES	3

typedef struct mp_ctx {
	const restarter_inst_t	*mpc_inst;
	scf_property_t		*mpc_prop;
	scf_value_t		*mpc_val;
	char			*mpc_buf;
	size_t			mpc_bufsz;
	char			mpc_pname[32];
} mp_ctx_t;

/*
 * Build the numbered property name for fmt and index in the context's
 * scratch buffer. Returns false, with the error logged, if the name does
 * not fit.
 */
static bool
mp_build_pname(mp_ctx_t *ctx, const char *fmt, uint_t index)
{
	int len;

	len = snprintf(ctx->mpc_pname, sizeof (ctx->mpc_pname), fmt, index);
	if (len < 0 || (size_t)len >= sizeof (ctx->mpc_pname)) {
		log_instance(ctx->mpc_inst, B_TRUE,
		    "Managed path property name for index %u is too long.",
		    index);
		return (false);
	}

	return (true);
}

/*
 * Look up the numbered astring property built from fmt and index in pg.
 * Returns 1 if the property was found, with a copy of its value in
 * *valp, 0 if the property does not exist, and -1 on any other error,
 * which is logged.
 */
static int
mp_get_astring(mp_ctx_t *ctx, scf_propertygroup_t *pg, const char *fmt,
    uint_t index, char **valp)
{
	*valp = NULL;

	if (!mp_build_pname(ctx, fmt, index))
		return (-1);

	if (scf_pg_get_property(pg, ctx->mpc_pname, ctx->mpc_prop) != 0) {
		if (scf_error() == SCF_ERROR_NOT_FOUND)
			return (0);
		log_instance(ctx->mpc_inst, B_TRUE,
		    "Could not read managed path property '%s': %s.",
		    ctx->mpc_pname, scf_strerror(scf_error()));
		return (-1);
	}

	if (scf_property_get_value(ctx->mpc_prop, ctx->mpc_val) != 0 ||
	    scf_value_get_astring(ctx->mpc_val, ctx->mpc_buf,
	    ctx->mpc_bufsz) < 0) {
		log_instance(ctx->mpc_inst, B_TRUE,
		    "Managed path property '%s' is invalid: %s.",
		    ctx->mpc_pname, scf_strerror(scf_error()));
		return (-1);
	}

	*valp = safe_strdup(ctx->mpc_buf);
	return (1);
}

/*
 * As for mp_get_astring(), but for a boolean property. *valp is left
 * unchanged when the property does not exist.
 */
static int
mp_get_boolean(mp_ctx_t *ctx, scf_propertygroup_t *pg, const char *fmt,
    uint_t index, bool *valp)
{
	uint8_t v;

	if (!mp_build_pname(ctx, fmt, index))
		return (-1);

	if (scf_pg_get_property(pg, ctx->mpc_pname, ctx->mpc_prop) != 0) {
		if (scf_error() == SCF_ERROR_NOT_FOUND)
			return (0);
		log_instance(ctx->mpc_inst, B_TRUE,
		    "Could not read managed path property '%s': %s.",
		    ctx->mpc_pname, scf_strerror(scf_error()));
		return (-1);
	}

	if (scf_property_get_value(ctx->mpc_prop, ctx->mpc_val) != 0 ||
	    scf_value_get_boolean(ctx->mpc_val, &v) != 0) {
		log_instance(ctx->mpc_inst, B_TRUE,
		    "Managed path property '%s' is invalid: %s.",
		    ctx->mpc_pname, scf_strerror(scf_error()));
		return (-1);
	}

	*valp = (v != 0);
	return (1);
}

/*
 * Return an initial buffer size for a reentrant passwd or group lookup.
 */
static size_t
mp_lookup_bufsz(int name)
{
	long sz = sysconf(name);

	return (sz > 0 ? (size_t)sz : 1024);
}

/*
 * Resolve a user name, or a numeric id in text form, to a uid, following
 * the same convention as a method credential.
 */
static int
mp_resolve_user(const mp_ctx_t *ctx, const char *str, uid_t *uidp)
{
	struct passwd pw, *pwp;
	char *buf;
	size_t bufsz;

	if (isdigit((uchar_t)str[0])) {
		const char *errstr;
		unsigned long long id;

		id = strtounum(str, 0, UID_MAX, &errstr);
		if (errstr != NULL) {
			log_instance(ctx->mpc_inst, B_TRUE,
			    "Managed path user \"%s\" is %s.", str, errstr);
			return (-1);
		}
		*uidp = (uid_t)id;
		return (0);
	}

	bufsz = mp_lookup_bufsz(_SC_GETPW_R_SIZE_MAX);
	for (;;) {
		buf = startd_alloc(bufsz);
		do {
			errno = 0;
			pwp = getpwnam_r(str, &pw, buf, bufsz);
		} while (pwp == NULL && errno == EINTR);
		if (pwp != NULL || errno != ERANGE)
			break;
		startd_free(buf, bufsz);
		bufsz *= 2;
	}

	if (pwp == NULL) {
		startd_free(buf, bufsz);
		log_instance(ctx->mpc_inst, B_TRUE,
		    "Managed path user \"%s\" was not found.", str);
		return (-1);
	}

	*uidp = pwp->pw_uid;
	startd_free(buf, bufsz);
	return (0);
}

/*
 * As for mp_resolve_user(), but for a group.
 */
static int
mp_resolve_group(const mp_ctx_t *ctx, const char *str, gid_t *gidp)
{
	struct group gr, *grp;
	char *buf;
	size_t bufsz;

	if (isdigit((uchar_t)str[0])) {
		const char *errstr;
		unsigned long long id;

		id = strtounum(str, 0, UID_MAX, &errstr);
		if (errstr != NULL) {
			log_instance(ctx->mpc_inst, B_TRUE,
			    "Managed path group \"%s\" is %s.", str, errstr);
			return (-1);
		}
		*gidp = (gid_t)id;
		return (0);
	}

	bufsz = mp_lookup_bufsz(_SC_GETGR_R_SIZE_MAX);
	for (;;) {
		buf = startd_alloc(bufsz);
		do {
			errno = 0;
			grp = getgrnam_r(str, &gr, buf, bufsz);
		} while (grp == NULL && errno == EINTR);
		if (grp != NULL || errno != ERANGE)
			break;
		startd_free(buf, bufsz);
		bufsz *= 2;
	}

	if (grp == NULL) {
		startd_free(buf, bufsz);
		log_instance(ctx->mpc_inst, B_TRUE,
		    "Managed path group \"%s\" was not found.", str);
		return (-1);
	}

	*gidp = grp->gr_gid;
	startd_free(buf, bufsz);
	return (0);
}

/*
 * A method credential can have a group of -1, meaning the user's primary
 * group from the passwd database. Resolve that in the same way for
 * directory ownership.
 */
static int
mp_default_gid(const mp_ctx_t *ctx, uid_t uid, gid_t *gidp)
{
	struct passwd pw, *pwp;
	char *buf;
	size_t bufsz;

	bufsz = mp_lookup_bufsz(_SC_GETPW_R_SIZE_MAX);
	for (;;) {
		buf = startd_alloc(bufsz);
		do {
			errno = 0;
			pwp = getpwuid_r(uid, &pw, buf, bufsz);
		} while (pwp == NULL && errno == EINTR);
		if (pwp != NULL || errno != ERANGE)
			break;
		startd_free(buf, bufsz);
		bufsz *= 2;
	}

	if (pwp == NULL) {
		startd_free(buf, bufsz);
		log_instance(ctx->mpc_inst, B_TRUE, "Could not determine the "
		    "default group for managed path user %lu.",
		    (ulong_t)uid);
		return (-1);
	}

	*gidp = pwp->pw_gid;
	startd_free(buf, bufsz);
	return (0);
}

/*
 * Remove the contents of the directory open on dirfd, which is always
 * closed before returning. Symbolic links are never followed, they are
 * removed like any other non-directory entry. Returns 0 on success or an
 * errno value. This is best effort in the face of concurrent activity.
 * Entries added by others while cleaning is in progress may remain.
 * A mounted filesystem below the directory is not descended into and
 * causes the removal to fail.
 */
static int
mp_empty_dir(int dirfd, uint_t depth)
{
	struct stat dst;
	DIR *dirp;
	uint_t pass;
	int err = 0;

	if (depth > MP_EMPTY_MAXDEPTH) {
		(void) close(dirfd);
		return (ENAMETOOLONG);
	}

	if (fstat(dirfd, &dst) != 0) {
		err = errno;
		(void) close(dirfd);
		return (err);
	}

	if ((dirp = fdopendir(dirfd)) == NULL) {
		err = errno;
		(void) close(dirfd);
		return (err);
	}

	for (pass = 0; pass < MP_EMPTY_MAXPASSES && err == 0; pass++) {
		const struct dirent *dp;
		bool removed = false;

		rewinddir(dirp);

		while (err == 0 && (dp = readdir(dirp)) != NULL) {
			struct stat st;
			int cfd;

			if (strcmp(dp->d_name, ".") == 0 ||
			    strcmp(dp->d_name, "..") == 0) {
				continue;
			}

			if (fstatat(dirfd, dp->d_name, &st,
			    AT_SYMLINK_NOFOLLOW) != 0) {
				if (errno == ENOENT)
					continue;
				err = errno;
				break;
			}

			if (!S_ISDIR(st.st_mode)) {
				if (unlinkat(dirfd, dp->d_name, 0) != 0) {
					if (errno == ENOENT)
						continue;
					err = errno;
					break;
				}
				removed = true;
				continue;
			}

			/*
			 * Do not descend into another filesystem. The mount
			 * point cannot be removed in any case, and its
			 * contents are not part of the managed path.
			 */
			if (st.st_dev != dst.st_dev) {
				err = EBUSY;
				break;
			}

			cfd = openat(dirfd, dp->d_name,
			    O_RDONLY | O_DIRECTORY | O_NOFOLLOW);
			if (cfd < 0) {
				if (errno == ENOENT)
					continue;
				err = errno;
				break;
			}

			if ((err = mp_empty_dir(cfd, depth + 1)) != 0)
				break;

			/*
			 * A subdirectory with new content after we emptied
			 * it has had something added externally. We
			 * don't fail in this case.
			 */
			if (unlinkat(dirfd, dp->d_name, AT_REMOVEDIR) != 0 &&
			    errno != ENOENT && errno != EEXIST &&
			    errno != ENOTEMPTY) {
				err = errno;
				break;
			}
			removed = true;
		}

		if (!removed)
			break;
	}

	(void) closedir(dirp);
	return (err);
}

/*
 * Ensure that the directory at path exists, creating it and any missing
 * intermediate directories as needed, and apply the requested ownership
 * and permissions to the final component. When empty is set and the
 * final directory already existed, its contents are removed.
 */
static int
mp_ensure_dir(const mp_ctx_t *ctx, const char *path, uid_t uid, gid_t gid,
    mode_t mode, bool empty)
{
	char *copy, *comp, *next, *lasts;
	bool created = false;
	int fd, err;

	if (path[0] != '/') {
		log_instance(ctx->mpc_inst, B_TRUE,
		    "Managed path \"%s\" is not an absolute path.", path);
		return (-1);
	}

	if ((fd = open("/", O_RDONLY | O_DIRECTORY)) < 0) {
		log_instance(ctx->mpc_inst, B_TRUE,
		    "Could not open the root directory: %s.", strerror(errno));
		return (-1);
	}

	copy = safe_strdup(path);

	comp = strtok_r(copy, "/", &lasts);
	if (comp == NULL) {
		log_instance(ctx->mpc_inst, B_TRUE,
		    "Managed path \"%s\" is not valid.", path);
		goto fail;
	}

	for (; comp != NULL; comp = next) {
		bool final;
		int nfd;

		next = strtok_r(NULL, "/", &lasts);
		final = (next == NULL);

		if (strcmp(comp, ".") == 0 || strcmp(comp, "..") == 0) {
			log_instance(ctx->mpc_inst, B_TRUE, "Managed path "
			    "\"%s\" may not contain \".\" or \"..\" "
			    "components.", path);
			goto fail;
		}

		created = false;
		if (mkdirat(fd, comp,
		    final ? mode : MP_INTERMEDIATE_MODE) == 0) {
			created = true;
		} else if (errno != EEXIST) {
			log_instance(ctx->mpc_inst, B_TRUE, "Could not create "
			    "component \"%s\" of managed path \"%s\": %s.",
			    comp, path, strerror(errno));
			goto fail;
		}

		nfd = openat(fd, comp, O_RDONLY | O_DIRECTORY | O_NOFOLLOW);
		if (nfd < 0) {
			int oerr = errno;
			struct stat st;

			/*
			 * openat(2) reports ENOTDIR rather than ELOOP for a
			 * symbolic link when O_DIRECTORY is also set, so we
			 * peek at the type to log the correct error.
			 */
			if (fstatat(fd, comp, &st, AT_SYMLINK_NOFOLLOW) == 0 &&
			    S_ISLNK(st.st_mode)) {
				log_instance(ctx->mpc_inst, B_TRUE,
				    "Component \"%s\" of managed path \"%s\" "
				    "is a symbolic link.", comp, path);
			} else {
				log_instance(ctx->mpc_inst, B_TRUE,
				    "Could not open component \"%s\" of "
				    "managed path \"%s\": %s.", comp, path,
				    strerror(oerr));
			}
			goto fail;
		}
		(void) close(fd);
		fd = nfd;

		/*
		 * The ownership and permissions of the final directory are
		 * always applied, even when it already existed. Created
		 * intermediates are set to root:root 0755 in case the
		 * umask interfered or a parent directory has the set-gid
		 * bit. Existing intermediates are left unchanged.
		 */
		if (final) {
			if (fchown(fd, uid, gid) != 0 ||
			    fchmod(fd, mode) != 0) {
				log_instance(ctx->mpc_inst, B_TRUE,
				    "Could not set ownership or permissions on "
				    "managed path \"%s\": %s.", path,
				    strerror(errno));
				goto fail;
			}
		} else if (created) {
			if (fchown(fd, 0, 0) != 0 ||
			    fchmod(fd, MP_INTERMEDIATE_MODE) != 0) {
				log_instance(ctx->mpc_inst, B_TRUE,
				    "Could not set ownership or permissions on "
				    "component \"%s\" of managed path "
				    "\"%s\": %s.", comp, path,
				    strerror(errno));
				goto fail;
			}
		}
	}

	if (empty && !created) {
		/* mp_empty_dir() consumes the file descriptor. */
		err = mp_empty_dir(fd, 0);
		fd = -1;
		if (err != 0) {
			log_instance(ctx->mpc_inst, B_TRUE,
			    "Could not empty managed path \"%s\": %s.", path,
			    strerror(err));
			goto fail;
		}
	}

	free(copy);
	if (fd >= 0)
		(void) close(fd);
	return (0);

fail:
	free(copy);
	if (fd >= 0)
		(void) close(fd);
	return (-1);
}

/*
 * Place the expanded path in the method environment under the name
 * given by the entry's env property. If the variable is already present,
 * whether from the method_environment or from an earlier managed path
 * entry, the path is appended to the existing value with a ':'
 * separator. A variable which is part of the base environment that
 * set_smf_env() provides to every method, such as PATH, is copied in
 * before the path is appended so that appending behaves as documented in
 * smf_method(7).
 */
static int
mp_env_add(const mp_ctx_t *ctx, struct method_context *mcp, const char *name,
    const char *path)
{
	const char *base;
	char *nv;
	size_t i, namelen;

	namelen = strlen(name);
	if (namelen == 0 || strchr(name, '=') != NULL ||
	    strncmp(name, "SMF_", 4) == 0) {
		log_instance(ctx->mpc_inst, B_TRUE, "Invalid environment "
		    "variable \"%s\" for managed path \"%s\".", name, path);
		return (-1);
	}

	if (mcp->env == NULL) {
		mcp->env_sz = 10;
		mcp->env = uu_zalloc(sizeof (*mcp->env) * mcp->env_sz);
		if (mcp->env == NULL)
			goto nomem;
	}

	for (i = 0; i < mcp->env_sz && mcp->env[i] != NULL; i++) {
		if (strncmp(mcp->env[i], name, namelen) == 0 &&
		    mcp->env[i][namelen] == '=') {
			break;
		}
	}

	if (i < mcp->env_sz && mcp->env[i] != NULL) {
		if ((nv = uu_msprintf("%s:%s", mcp->env[i], path)) == NULL)
			goto nomem;
		free(mcp->env[i]);
		mcp->env[i] = nv;
		return (0);
	}

	base = global_env_value(name);
	if (base != NULL)
		nv = uu_msprintf("%s=%s:%s", name, base, path);
	else
		nv = uu_msprintf("%s=%s", name, path);
	if (nv == NULL)
		goto nomem;

	/* Grow the array if this entry would take the terminating NULL. */
	if (i + 1 >= mcp->env_sz) {
		char **env;

		env = uu_zalloc(sizeof (*mcp->env) * mcp->env_sz * 2);
		if (env == NULL) {
			uu_free(nv);
			goto nomem;
		}
		(void) memcpy(env, mcp->env,
		    sizeof (*mcp->env) * mcp->env_sz);
		free(mcp->env);
		mcp->env = env;
		mcp->env_sz *= 2;
	}

	mcp->env[i] = nv;
	return (0);

nomem:
	log_instance(ctx->mpc_inst, B_TRUE,
	    "Could not add \"%s\" to the method environment: %s.", name,
	    strerror(ENOMEM));
	return (-1);
}

/*
 * Process the managed path entry with the given index, if it exists.
 * *foundp is set to reflect whether the entry was present. Returns 0 on
 * success, including when the entry does not exist, and -1 on failure.
 */
static int
mp_process_entry(mp_ctx_t *ctx, scf_propertygroup_t *pg, uint_t index,
    scf_snapshot_t *snap, struct method_context *mcp, bool *foundp)
{
	char *rawpath = NULL, *user = NULL, *group = NULL, *modestr = NULL;
	char *env = NULL, *path = NULL;
	bool empty = false;
	uid_t uid;
	gid_t gid;
	mode_t mode = MP_DEFAULT_MODE;
	int r, ret = -1;

	*foundp = false;

	r = mp_get_astring(ctx, pg, SCF_PROPERTY_MP_PATH_FMT, index,
	    &rawpath);
	if (r <= 0)
		return (r);
	*foundp = true;

	if (mp_get_astring(ctx, pg, SCF_PROPERTY_MP_USER_FMT, index,
	    &user) < 0 ||
	    mp_get_astring(ctx, pg, SCF_PROPERTY_MP_GROUP_FMT, index,
	    &group) < 0 ||
	    mp_get_astring(ctx, pg, SCF_PROPERTY_MP_MODE_FMT, index,
	    &modestr) < 0 ||
	    mp_get_astring(ctx, pg, SCF_PROPERTY_MP_ENV_FMT, index,
	    &env) < 0 ||
	    mp_get_boolean(ctx, pg, SCF_PROPERTY_MP_EMPTY_FMT, index,
	    &empty) < 0) {
		goto out;
	}

	/*
	 * An empty env value is a synonym for not placing the path in the
	 * environment. Import does not store one, but the property can be
	 * set directly.
	 */
	if (env != NULL && *env == '\0') {
		free(env);
		env = NULL;
	}

	if (expand_method_tokens(rawpath, ctx->mpc_inst->ri_m_inst, snap,
	    METHOD_START, &path) != 0) {
		log_instance(ctx->mpc_inst, B_TRUE,
		    "Could not expand managed path \"%s\": %s.", rawpath,
		    path == NULL ? "unknown error" : path);
		goto out;
	}

	if (modestr != NULL) {
		const char *errstr;
		unsigned long long m;

		m = strtounumx(modestr, 0, 07777, &errstr, 8);
		if (errstr != NULL) {
			log_instance(ctx->mpc_inst, B_TRUE,
			    "Mode \"%s\" for managed path \"%s\" is %s.",
			    modestr, path, errstr);
			goto out;
		}
		mode = (mode_t)m;
	}

	uid = mcp->uid;
	gid = mcp->gid;

	if (user != NULL && mp_resolve_user(ctx, user, &uid) != 0)
		goto out;
	if (group != NULL && mp_resolve_group(ctx, group, &gid) != 0)
		goto out;

	/*
	 * A method credential group of -1 means the user's primary group
	 * from the passwd database. Resolve it the same way here.
	 */
	if (gid == (gid_t)-1 && mp_default_gid(ctx, uid, &gid) != 0)
		goto out;

	if (mp_ensure_dir(ctx, path, uid, gid, mode, empty) != 0)
		goto out;

	if (env != NULL && mp_env_add(ctx, mcp, env, path) != 0)
		goto out;

	log_framework(LOG_DEBUG, "%s: applied managed path \"%s\"\n",
	    ctx->mpc_inst->ri_i.i_fmri, path);

	ret = 0;
out:
	free(rawpath);
	free(user);
	free(group);
	free(modestr);
	free(env);
	free(path);
	return (ret);
}

/*
 * Process every entry in one managed_paths property group, in index
 * order, stopping at the first index with no path property.
 */
static int
mp_process_pg(mp_ctx_t *ctx, scf_propertygroup_t *pg, scf_snapshot_t *snap,
    struct method_context *mcp)
{
	uint_t index;

	for (index = 0; ; index++) {
		bool found;

		if (mp_process_entry(ctx, pg, index, snap, mcp, &found) != 0)
			return (-1);
		if (!found)
			return (0);
	}
}

/*
 * Apply the managed paths for an instance which is about to run its
 * start method. Directories described by the service level property
 * group are processed first, followed by those at the instance level.
 *
 * The method context provides the default directory ownership and
 * receives any environment variable additions, and then set_smf_env()
 * merges them into the method environment. When the instance has managed
 * paths and *mcpp is NULL, the context is gathered here and returned for
 * the caller to use and free. Instances without managed paths never have
 * their context gathered.
 *
 * Errors have already been logged when this function returns -1. The
 * caller is expected to place the instance in maintenance.
 */
int
managed_paths_apply(const restarter_inst_t *inst, scf_snapshot_t *snap,
    const char *mname, const char *method, struct method_context **mcpp)
{
	scf_instance_t *scf_inst = inst->ri_m_inst;
	scf_handle_t *h = scf_instance_handle(scf_inst);
	scf_propertygroup_t *svcpg, *instpg;
	bool have_svc = false, have_inst = false;
	mp_ctx_t ctx;
	int ret = -1;

	svcpg = safe_scf_pg_create(h);
	instpg = safe_scf_pg_create(h);

	(void) memset(&ctx, 0, sizeof (ctx));
	ctx.mpc_inst = inst;
	ctx.mpc_prop = safe_scf_property_create(h);
	ctx.mpc_val = safe_scf_value_create(h);
	ctx.mpc_bufsz = max_scf_value_size + 1;
	ctx.mpc_buf = startd_alloc(ctx.mpc_bufsz);

	/*
	 * Locate the managed_paths property groups at the service and
	 * instance levels. When a snapshot is available its snaplevels
	 * are walked and the instance level is the base level and the
	 * service level follows it.
	 */
	if (snap != NULL) {
		scf_snaplevel_t *lvl = safe_scf_snaplevel_create(h);
		scf_snaplevel_t *nlvl = safe_scf_snaplevel_create(h);

		if (scf_snapshot_get_base_snaplevel(snap, lvl) != 0) {
			log_instance(inst, B_TRUE, "Could not read the "
			    "snaplevels of the running snapshot: %s.",
			    scf_strerror(scf_error()));
			scf_snaplevel_destroy(lvl);
			scf_snaplevel_destroy(nlvl);
			goto out;
		}

		for (;;) {
			scf_snaplevel_t *tmp;
			scf_propertygroup_t *pg;
			bool *havep;

			if (scf_snaplevel_get_instance_name(lvl, ctx.mpc_buf,
			    ctx.mpc_bufsz) >= 0) {
				pg = instpg;
				havep = &have_inst;
			} else if (scf_error() ==
			    SCF_ERROR_CONSTRAINT_VIOLATED) {
				/* This is a service level snaplevel. */
				pg = svcpg;
				havep = &have_svc;
			} else {
				log_instance(inst, B_TRUE, "Could not read "
				    "the snaplevels of the running "
				    "snapshot: %s.",
				    scf_strerror(scf_error()));
				scf_snaplevel_destroy(lvl);
				scf_snaplevel_destroy(nlvl);
				goto out;
			}

			if (scf_snaplevel_get_pg(lvl, SCF_PG_MANAGED_PATHS,
			    pg) == 0) {
				*havep = true;
			} else if (scf_error() != SCF_ERROR_NOT_FOUND) {
				log_instance(inst, B_TRUE, "Could not read "
				    "the managed_paths property group: %s.",
				    scf_strerror(scf_error()));
				scf_snaplevel_destroy(lvl);
				scf_snaplevel_destroy(nlvl);
				goto out;
			}

			if (scf_snaplevel_get_next_snaplevel(lvl, nlvl) != 0) {
				if (scf_error() == SCF_ERROR_NOT_FOUND)
					break;
				log_instance(inst, B_TRUE, "Could not read "
				    "the snaplevels of the running "
				    "snapshot: %s.",
				    scf_strerror(scf_error()));
				scf_snaplevel_destroy(lvl);
				scf_snaplevel_destroy(nlvl);
				goto out;
			}
			tmp = lvl;
			lvl = nlvl;
			nlvl = tmp;
		}

		scf_snaplevel_destroy(lvl);
		scf_snaplevel_destroy(nlvl);
	} else {
		scf_service_t *svc = safe_scf_service_create(h);

		if (scf_instance_get_parent(scf_inst, svc) != 0) {
			log_instance(inst, B_TRUE,
			    "Could not get the parent service: %s.",
			    scf_strerror(scf_error()));
			scf_service_destroy(svc);
			goto out;
		}

		if (scf_service_get_pg(svc, SCF_PG_MANAGED_PATHS,
		    svcpg) == 0) {
			have_svc = true;
		} else if (scf_error() != SCF_ERROR_NOT_FOUND) {
			log_instance(inst, B_TRUE, "Could not read the "
			    "managed_paths property group: %s.",
			    scf_strerror(scf_error()));
			scf_service_destroy(svc);
			goto out;
		}
		scf_service_destroy(svc);

		if (scf_instance_get_pg(scf_inst, SCF_PG_MANAGED_PATHS,
		    instpg) == 0) {
			have_inst = true;
		} else if (scf_error() != SCF_ERROR_NOT_FOUND) {
			log_instance(inst, B_TRUE, "Could not read the "
			    "managed_paths property group: %s.",
			    scf_strerror(scf_error()));
			goto out;
		}
	}

	if (!have_svc && !have_inst) {
		ret = 0;
		goto out;
	}

	if (*mcpp == NULL) {
		mc_error_t *m_error;

		m_error = restarter_get_method_context(
		    RESTARTER_METHOD_CONTEXT_VERSION, scf_inst, snap, mname,
		    method, mcpp);
		if (m_error != NULL) {
			log_instance(inst, B_TRUE, "%s", m_error->msg);
			restarter_mc_error_destroy(m_error);
			goto out;
		}
	}

	if (have_svc && mp_process_pg(&ctx, svcpg, snap, *mcpp) != 0)
		goto out;
	if (have_inst && mp_process_pg(&ctx, instpg, snap, *mcpp) != 0)
		goto out;

	ret = 0;
out:
	startd_free(ctx.mpc_buf, ctx.mpc_bufsz);
	scf_value_destroy(ctx.mpc_val);
	scf_property_destroy(ctx.mpc_prop);
	scf_pg_destroy(instpg);
	scf_pg_destroy(svcpg);
	return (ret);
}
