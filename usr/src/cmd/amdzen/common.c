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
 * Copyright 2025 Oxide Computer Company
 */

/*
 * Utility functions common to amdzen commands.
 */

#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <libdevinfo.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

/*
 * Traditionally we expect to be given a path to a driver minor path hidden in
 * /devices. To make life easier, we want to also support folks using the minor
 * instance numbers which correspond to a particular DF. See if we've been
 * given a path to a character device and if so, just continue straight on with
 * the given argument. Otherwise we attempt to map it to something known.
 */
char *
amdzen_parse_path(const char *driver, char *arg)
{
	struct stat st;
	di_node_t root, node;
	const char *errstr;
	char *path = NULL;
	int df;

	if (stat(arg, &st) == 0 && S_ISCHR(st.st_mode))
		return (arg);

	/* If there is a driver prefix provided, accept and remove it */
	if (strncmp(arg, driver, strlen(driver)) == 0)
		arg += strlen(driver);

	df = (int)strtonumx(arg, 0, INT_MAX, &errstr, 0);
	if (errstr != NULL) {
		warnx("failed to parse DF number '%s': %s", arg, errstr);
		return (NULL);
	}

	/*
	 * XXX
	 * Poor way to load children of amdzen in case we're on DEBUG bits that
	 * will keep unloading them. Any suggestions?
	 */
	root = di_init("/", DINFOSUBTREE);
	if (root != DI_NODE_NIL) {
		char *amdzenpath, *tpath;

		node = di_drv_first_node("amdzen", root);
		if (node == DI_NODE_NIL) {
			warn("failed to find amdzen instance");
			return (NULL);
		}
		amdzenpath = di_devfs_path(node);
		if (asprintf(&tpath, "/devices%s", amdzenpath) < 0) {
			warn("failed to construct full path for %s",
			    amdzenpath);
			/* Carry on regardless, the driver may be loaded */
		} else {
			/* Walk the directory to load child drivers */
			DIR *dir = opendir(tpath);
			while (readdir(dir) != NULL)
				;
			closedir(dir);
		}
		di_devfs_path_free(amdzenpath);
		free(tpath);
		/* We need a new devinfo snapshot */
		di_fini(root);
	}
	/* XXX end poor... */

	root = di_init("/", DINFOSUBTREE | DINFOMINOR);
	if (root == DI_NODE_NIL) {
		warn("failed to initialize libdevinfo while trying to map "
		    "device name %s", driver);
		return (NULL);
	}

	/* These are all single-instance drivers with one minor per DF */
	node = di_drv_first_node(driver, root);
	if (node != DI_NODE_NIL) {
		di_minor_t minor = DI_MINOR_NIL;
		char mname[64];

		(void) snprintf(mname, sizeof (mname), "%s.%d", driver, df);

		while ((minor = di_minor_next(node, minor)) != DI_MINOR_NIL) {
			if (strcmp(di_minor_name(minor), mname) == 0)
				break;
		}

		if (minor == DI_MINOR_NIL) {
			warnx("failed to find minor %s on %s%d",
			    mname, di_driver_name(node), di_instance(node));
		} else {
			char *bpath = di_devfs_minor_path(minor);

			if (bpath == NULL) {
				warn("failed to get minor path for %s%d:%s",
				    di_driver_name(node), di_instance(node),
				    di_minor_name(minor));
			} else if (asprintf(&path, "/devices%s", bpath) < 0) {
				warn("failed to construct full path for %s",
				    bpath);
			}
			di_devfs_path_free(bpath);
		}
	}

	di_fini(root);

	if (path == NULL)
		warnx("failed to map DF %d to a %s instance", df, driver);

	return (path);
}
