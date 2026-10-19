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
 * A userland client for the control interface of espi(4D).
 */

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <libdevinfo.h>
#include <libgen.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/espi_ctl.h>
#include <sys/types.h>

#include "espi_cmd.h"

#define	ESPI_DRIVER	"espi"
#define	ESPI_CTL_MINOR	"ctl"

const char *progname;

/* The controller selected with -c, if any */
static bool espi_have_unit;
static uint_t espi_unit;

static void
espi_usage(const espi_cmdtab_t *cmdtab)
{
	(void) fprintf(stderr,
	    "Usage: %s [-c controller] <subcommand> <args> ...\n"
	    "Available subcommands:\n", progname);

	for (uint_t cmd = 0; cmdtab[cmd].ec_name != NULL; cmd++) {
		if (cmdtab[cmd].ec_use != NULL)
			cmdtab[cmd].ec_use(stderr);
		else
			(void) fprintf(stderr, "\t%s\n", cmdtab[cmd].ec_name);
	}
}

static int
espi_walk_tab(const espi_cmdtab_t *cmdtab, int argc, char **argv)
{
	uint_t cmd;

	if (argc == 0) {
		warnx("missing required subcommand");
		espi_usage(cmdtab);
		exit(EXIT_USAGE);
	}

	for (cmd = 0; cmdtab[cmd].ec_name != NULL; cmd++) {
		if (strcmp(argv[0], cmdtab[cmd].ec_name) == 0)
			break;
	}

	if (cmdtab[cmd].ec_name == NULL) {
		warnx("unknown subcommand: %s", argv[0]);
		espi_usage(cmdtab);
		exit(EXIT_USAGE);
	}

	argc--;
	argv++;
	optind = 0;

	return (cmdtab[cmd].ec_op(argc, argv));
}

typedef bool (*espi_ctl_walk_f)(uint_t, const char *, void *);

/*
 * A controller's unit address is treated as a number so that it does not
 * matter how the nexus above formats it or how the user writes it.
 */
static bool
espi_node_unit(di_node_t node, uint_t *unitp)
{
	const char *ua = di_bus_addr(node);
	const char *errstr;
	long long unit;

	if (ua == NULL)
		return (false);

	unit = strtonumx(ua, 0, UINT_MAX, &errstr, 16);
	if (errstr != NULL)
		return (false);

	*unitp = (uint_t)unit;
	return (true);
}

static void
espi_ctl_walk(espi_ctl_walk_f func, void *arg)
{
	di_node_t root, node;

	root = di_init_driver(ESPI_DRIVER, DINFOSUBTREE | DINFOMINOR);
	if (root == DI_NODE_NIL)
		err(EXIT_FAILURE, "failed to take a devinfo snapshot");

	for (node = di_drv_first_node(ESPI_DRIVER, root);
	    node != DI_NODE_NIL; node = di_drv_next_node(node)) {
		di_minor_t minor = DI_MINOR_NIL;
		char *mpath, *path;
		uint_t unit;
		bool more;

		while ((minor = di_minor_next(node, minor)) != DI_MINOR_NIL) {
			if (strcmp(di_minor_name(minor), ESPI_CTL_MINOR) == 0)
				break;
		}
		if (minor == DI_MINOR_NIL || !espi_node_unit(node, &unit))
			continue;

		if ((mpath = di_devfs_minor_path(minor)) == NULL) {
			err(EXIT_FAILURE,
			    "failed to get the minor path for %s%d",
			    di_driver_name(node), di_instance(node));
		}
		if (asprintf(&path, "/devices%s", mpath) < 0)
			err(EXIT_FAILURE, "failed to construct a device path");
		di_devfs_path_free(mpath);

		more = func(unit, path, arg);
		free(path);
		if (!more)
			break;
	}

	di_fini(root);
}

typedef struct espi_find {
	uint_t	ef_count;
	char	*ef_path;
} espi_find_t;

static bool
espi_find_cb(uint_t unit, const char *path, void *arg)
{
	espi_find_t *ef = arg;

	if (espi_have_unit && unit != espi_unit)
		return (true);

	if (ef->ef_count++ == 0 && (ef->ef_path = strdup(path)) == NULL)
		err(EXIT_FAILURE, "failed to allocate memory");

	return (!espi_have_unit);
}

/*
 * Open the control node of the controller selected with -c or, if there was
 * none, of the only controller that has one.
 */
static int
espi_open(int oflag)
{
	espi_find_t ef = { 0 };
	int fd;

	espi_ctl_walk(espi_find_cb, &ef);

	if (ef.ef_count == 0) {
		if (espi_have_unit) {
			errx(EXIT_FAILURE,
			    "eSPI controller %u has no control node",
			    espi_unit);
		}
		errx(EXIT_FAILURE, "no eSPI controller has a control node");
	}
	if (ef.ef_count > 1) {
		errx(EXIT_FAILURE, "more than one eSPI controller has a "
		    "control node, select one with -c");
	}

	if ((fd = open(ef.ef_path, oflag)) < 0)
		err(EXIT_FAILURE, "failed to open %s", ef.ef_path);

	free(ef.ef_path);
	return (fd);
}

static bool
espi_list_cb(uint_t unit, const char *path, void *arg)
{
	(void) printf("%-10u %s\n", unit, path);
	return (true);
}

static void
espi_list_usage(FILE *f)
{
	(void) fprintf(f, "\tlist\n");
}

static int
espi_list(int argc, char **argv)
{
	if (argc != 0) {
		warnx("list takes no arguments");
		espi_list_usage(stderr);
		return (EXIT_USAGE);
	}

	(void) printf("%-10s %s\n", "CONTROLLER", "PATH");
	espi_ctl_walk(espi_list_cb, NULL);

	return (0);
}

/*
 * Status
 *
 * The driver records the state of each controller, of its link and of its
 * target as properties on the controller's node, and the drivers for the
 * channels do the same on theirs. We enumerate them and print them as they
 * are.
 */
static void
espi_status_props(di_node_t node, const char *indent)
{
	di_prop_t prop = DI_PROP_NIL;

	while ((prop = di_prop_next(node, prop)) != DI_PROP_NIL) {
		int *ints;
		int64_t *int64s;
		char *strs;
		int n;

		switch (di_prop_type(prop)) {
		case DI_PROP_TYPE_BOOLEAN:
			(void) printf("%s%s\n", indent, di_prop_name(prop));
			continue;
		case DI_PROP_TYPE_INT:
			if ((n = di_prop_ints(prop, &ints)) <= 0)
				continue;
			(void) printf("%s%s:", indent, di_prop_name(prop));
			for (int i = 0; i < n; i++)
				(void) printf(" 0x%x", (uint_t)ints[i]);
			break;
		case DI_PROP_TYPE_INT64:
			if ((n = di_prop_int64(prop, &int64s)) <= 0)
				continue;
			(void) printf("%s%s:", indent, di_prop_name(prop));
			for (int i = 0; i < n; i++) {
				(void) printf(" 0x%" PRIx64,
				    (uint64_t)int64s[i]);
			}
			break;
		case DI_PROP_TYPE_STRING:
			if ((n = di_prop_strings(prop, &strs)) <= 0)
				continue;
			(void) printf("%s%s:", indent, di_prop_name(prop));
			for (int i = 0; i < n; i++) {
				(void) printf(" %s", strs);
				strs += strlen(strs) + 1;
			}
			break;
		default:
			continue;
		}

		(void) printf("\n");
	}
}

/*
 * Print the virtual wire groups that the controller has captured from the
 * target. These come from the control node, so a controller without one is
 * skipped.
 */
static void
espi_status_vw(di_node_t node, const char *indent)
{
	di_minor_t minor = DI_MINOR_NIL;
	char *mpath, *path;
	espi_vw_t vw;
	int fd;

	while ((minor = di_minor_next(node, minor)) != DI_MINOR_NIL) {
		if (strcmp(di_minor_name(minor), ESPI_CTL_MINOR) == 0)
			break;
	}
	if (minor == DI_MINOR_NIL)
		return;

	if ((mpath = di_devfs_minor_path(minor)) == NULL) {
		err(EXIT_FAILURE, "failed to get the minor path for %s%d",
		    di_driver_name(node), di_instance(node));
	}
	if (asprintf(&path, "/devices%s", mpath) < 0)
		err(EXIT_FAILURE, "failed to construct a device path");
	di_devfs_path_free(mpath);

	if ((fd = open(path, O_RDONLY)) < 0) {
		warn("failed to open %s", path);
		free(path);
		return;
	}

	if (ioctl(fd, ESPI_IOC_VW_READ, &vw) != 0) {
		warn("failed to read virtual wires from %s", path);
	} else {
		for (uint_t i = 0; i < vw.ev_count; i++) {
			(void) printf("%svwire-index-%u: 0x%02x\n", indent,
			    vw.ev_index[i], vw.ev_data[i]);
		}
	}

	(void) close(fd);
	free(path);
}

static void
espi_status_usage(FILE *f)
{
	(void) fprintf(f, "\tstatus\n");
}

static int
espi_status(int argc, char **argv)
{
	di_node_t root, node;
	uint_t count = 0;

	if (argc != 0) {
		warnx("status takes no arguments");
		espi_status_usage(stderr);
		return (EXIT_USAGE);
	}

	root = di_init_driver(ESPI_DRIVER,
	    DINFOSUBTREE | DINFOPROP | DINFOMINOR);
	if (root == DI_NODE_NIL)
		err(EXIT_FAILURE, "failed to take a devinfo snapshot");

	for (node = di_drv_first_node(ESPI_DRIVER, root);
	    node != DI_NODE_NIL; node = di_drv_next_node(node)) {
		uint_t unit;

		if (!espi_node_unit(node, &unit))
			continue;
		if (espi_have_unit && unit != espi_unit)
			continue;

		if (count++ != 0)
			(void) printf("\n");
		(void) printf("controller %u (%s%d)\n", unit,
		    di_driver_name(node), di_instance(node));
		espi_status_props(node, "\t");
		espi_status_vw(node, "\t");

		for (di_node_t child = di_child_node(node);
		    child != DI_NODE_NIL; child = di_sibling_node(child)) {
			const char *ua = di_bus_addr(child);

			(void) printf("\t%s@%s\n", di_node_name(child),
			    (ua != NULL) ? ua : "?");
			espi_status_props(child, "\t\t");
		}
	}

	di_fini(root);

	if (count == 0) {
		if (espi_have_unit) {
			errx(EXIT_FAILURE, "eSPI controller %u was not found",
			    espi_unit);
		}
		errx(EXIT_FAILURE, "no eSPI controllers were found");
	}

	return (0);
}

/*
 * Configuration Registers
 */

/*
 * Parse a configuration register offset, which must be aligned and fall
 * within [min, ESPI_CFG_SIZE).
 */
static uint32_t
espi_parse_offset(const char *arg, uint32_t min)
{
	const char *errstr;
	uint32_t off;

	off = (uint32_t)strtonumx(arg, min, ESPI_CFG_SIZE - sizeof (uint32_t),
	    &errstr, 0);
	if (errstr != NULL) {
		errx(EXIT_USAGE,
		    "register offset '%s' is %s (range 0x%x-0x%zx)",
		    arg, errstr, min, ESPI_CFG_SIZE - sizeof (uint32_t));
	}
	if ((off % sizeof (uint32_t)) != 0) {
		errx(EXIT_USAGE, "register offset 0x%x is not 4-byte aligned",
		    off);
	}

	return (off);
}

static void
espi_config_read_usage(FILE *f)
{
	(void) fprintf(f, "\tconfig read [-n count] <offset>\n");
}

static int
espi_config_read(int argc, char **argv)
{
	const char *errstr;
	uint32_t off, count = 1, max;
	int c, fd;

	while ((c = getopt(argc, argv, ":n:")) != -1) {
		switch (c) {
		case 'n':
			count = (uint32_t)strtonumx(optarg, 1,
			    ESPI_CFG_SIZE / sizeof (uint32_t), &errstr, 0);
			if (errstr != NULL) {
				errx(EXIT_USAGE,
				    "register count '%s' is %s (range 1-%zu)",
				    optarg, errstr,
				    ESPI_CFG_SIZE / sizeof (uint32_t));
			}
			break;
		case ':':
			warnx("option -%c requires an argument", optopt);
			espi_config_read_usage(stderr);
			return (EXIT_USAGE);
		case '?':
			warnx("unknown option: -%c", optopt);
			espi_config_read_usage(stderr);
			return (EXIT_USAGE);
		}
	}

	argc -= optind;
	argv += optind;

	if (argc != 1) {
		warnx("expected a register offset");
		espi_config_read_usage(stderr);
		return (EXIT_USAGE);
	}

	off = espi_parse_offset(argv[0], 0);
	max = (ESPI_CFG_SIZE - off) / sizeof (uint32_t);
	if (count > max) {
		errx(EXIT_USAGE, "-n %u is too many, only %u registers follow "
		    "offset 0x%x", count, max, off);
	}

	fd = espi_open(O_RDONLY);

	for (uint32_t i = 0; i < count; i++) {
		espi_cfg_t cfg = { .ec_offset = off + i * sizeof (uint32_t) };

		if (ioctl(fd, ESPI_IOC_CFG_READ, &cfg) != 0) {
			err(EXIT_FAILURE, "failed to read register 0x%x",
			    cfg.ec_offset);
		}

		if (count == 1) {
			(void) printf("0x%08x\n", cfg.ec_value);
		} else {
			(void) printf("0x%03x 0x%08x\n", cfg.ec_offset,
			    cfg.ec_value);
		}
	}

	(void) close(fd);
	return (0);
}

static void
espi_config_write_usage(FILE *f)
{
	(void) fprintf(f, "\tconfig write <offset> <value>\n");
}

static int
espi_config_write(int argc, char **argv)
{
	const char *errstr;
	espi_cfg_t cfg;
	int fd;

	if (argc != 2) {
		warnx("expected a register offset and a value");
		espi_config_write_usage(stderr);
		return (EXIT_USAGE);
	}

	cfg.ec_offset = espi_parse_offset(argv[0], ESPI_CFG_PLATFORM_START);
	cfg.ec_value = (uint32_t)strtonumx(argv[1], 0, UINT32_MAX, &errstr, 0);
	if (errstr != NULL)
		errx(EXIT_USAGE, "value '%s' is %s", argv[1], errstr);

	fd = espi_open(O_RDWR);

	if (ioctl(fd, ESPI_IOC_CFG_WRITE, &cfg) != 0) {
		err(EXIT_FAILURE, "failed to write register 0x%x",
		    cfg.ec_offset);
	}

	(void) close(fd);
	return (0);
}

static const espi_cmdtab_t espi_config_cmds[] = {
	{ "read", espi_config_read, espi_config_read_usage },
	{ "write", espi_config_write, espi_config_write_usage },
	{ NULL, NULL, NULL }
};

static void
espi_config_usage(FILE *f)
{
	espi_config_read_usage(f);
	espi_config_write_usage(f);
}

static int
espi_config(int argc, char **argv)
{
	return (espi_walk_tab(espi_config_cmds, argc, argv));
}

static const espi_cmdtab_t espi_cmds[] = {
	{ "config", espi_config, espi_config_usage },
	{ "list", espi_list, espi_list_usage },
	{ "status", espi_status, espi_status_usage },
	{ NULL, NULL, NULL }
};

int
main(int argc, char **argv)
{
	const char *errstr;
	int c;

	progname = basename(argv[0]);

	while ((c = getopt(argc, argv, ":c:")) != -1) {
		switch (c) {
		case 'c':
			espi_unit = (uint_t)strtonumx(optarg, 0, UINT_MAX,
			    &errstr, 0);
			if (errstr != NULL) {
				errx(EXIT_USAGE, "controller '%s' is %s",
				    optarg, errstr);
			}
			espi_have_unit = true;
			break;
		case ':':
			warnx("option -%c requires an argument", optopt);
			espi_usage(espi_cmds);
			return (EXIT_USAGE);
		case '?':
			warnx("unknown option: -%c", optopt);
			espi_usage(espi_cmds);
			return (EXIT_USAGE);
		}
	}

	argc -= optind;
	argv += optind;

	return (espi_walk_tab(espi_cmds, argc, argv));
}
