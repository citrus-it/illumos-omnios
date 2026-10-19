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
 * fwflash(8) backend for flash devices reached over an eSPI flash access
 * channel, which espi_flash(4D) presents as character devices that are read
 * and written at the offset of the flash address and erased through an
 * ioctl.
 *
 * A firmware image is written from the start of the flash. The flash is
 * processed one erase block at a time. A block whose contents already match
 * the image is skipped, and one that differs is erased, written and read
 * back. The last block is erased in full even when the image ends part way
 * through it, so the remainder of that block reads as erased flash
 * afterwards.
 *
 * The transport plugin records the device's espi_flash_info_t as the
 * encapsulated identity, and the verifier plugin uses it to check that an
 * image fits.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/espi_flash.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <libintl.h>
#include <libdevinfo.h>
#include <fwflash/fwflash.h>

/*
 * fwflash requires we declare our driver name as data with this name.
 */
const char drivername[] = "espi_flash";
const int plugin_version = FWPLUGIN_VERSION_2;

/*
 * External data from fwflash.
 */
extern di_node_t rootnode;
extern struct fw_plugin *self;
extern struct vrfyplugin *verifier;

/*
 * The minor node that espi_flash(4D) creates.
 */
#define	ESPIFW_MINOR		"flash"

/*
 * Reads for comparison and for saving the flash to a file go through a buffer
 * of this size.
 */
#define	ESPIFW_BUFLEN		(1024 * 1024)

/*
 * The erase block size assumed for a target that does not advertise any, which
 * is the smallest that the protocol can ask for.
 */
#define	ESPIFW_DEFAULT_ERASE	(4 * 1024)

/*
 * The erase block sizes that can be requested over the channel, smallest first.
 * The target's espi_flash_info_t advertises sizes as a bitmask with bit n set
 * for 2^n KiB blocks.
 */
static const uint32_t espifw_erase_sizes[] = {
	4 * 1024, 32 * 1024, 64 * 1024, 128 * 1024
};

static bool
espifw_erase_size_advertised(const espi_flash_info_t *info, uint32_t size)
{
	return ((info->efi_erase_sizes & (size >> 10)) != 0);
}

/*
 * Choose the erase block size to use. The smallest advertised size keeps down
 * the amount rewritten when only part of the image has changed.
 */
static uint32_t
espifw_erase_size(const espi_flash_info_t *info)
{
	if (info->efi_erase_sizes == 0)
		return (ESPIFW_DEFAULT_ERASE);

	for (uint_t i = 0; i < ARRAY_SIZE(espifw_erase_sizes); i++) {
		if (espifw_erase_size_advertised(info, espifw_erase_sizes[i]))
			return (espifw_erase_sizes[i]);
	}

	logmsg(MSG_WARN, gettext("%s: target advertises no usable erase block "
	    "size (0x%x), assuming %u bytes\n"), drivername,
	    info->efi_erase_sizes, ESPIFW_DEFAULT_ERASE);
	return (ESPIFW_DEFAULT_ERASE);
}

/*
 * Transfer all of a buffer, retrying short transfers.
 */
static bool
espifw_pread(int fd, void *buf, size_t len, off_t off)
{
	uint8_t *p = buf;

	while (len > 0) {
		const ssize_t n = pread(fd, p, len, off);

		if (n < 0) {
			if (errno == EINTR)
				continue;
			return (false);
		}
		if (n == 0) {
			errno = EIO;
			return (false);
		}
		p += n;
		off += n;
		len -= n;
	}

	return (true);
}

static bool
espifw_pwrite(int fd, const void *buf, size_t len, off_t off)
{
	const uint8_t *p = buf;

	while (len > 0) {
		const ssize_t n = pwrite(fd, p, len, off);

		if (n < 0) {
			if (errno == EINTR)
				continue;
			return (false);
		}
		if (n == 0) {
			errno = EIO;
			return (false);
		}
		p += n;
		off += n;
		len -= n;
	}

	return (true);
}

static void
espifw_flashdev_free(struct devicelist *flashdev)
{
	if (flashdev == NULL)
		return;

	if (flashdev->ident != NULL) {
		free(flashdev->ident->vid);
		free(flashdev->ident->pid);
		free(flashdev->ident->encap_ident);
	}
	free(flashdev->ident);
	free(flashdev->drvname);
	free(flashdev->classname);
	free(flashdev->access_devname);
	free(flashdev);
}

/*
 * Describe one espi_flash node. A node whose device cannot be opened, because
 * the driver is not attached for instance, is reported and skipped. Only a
 * failure to allocate memory is treated as fatal.
 */
static int
espifw_add_device(di_node_t node, int idx, bool *skipped)
{
	struct devicelist *flashdev = NULL;
	espi_flash_info_t *info = NULL;
	di_node_t parent;
	char *path;
	int fd;

	*skipped = false;

	if ((path = di_devfs_path(node)) == NULL) {
		logmsg(MSG_ERROR, gettext("%s: failed to get device node "
		    "path: %s\n"), drivername, strerror(errno));
		return (FWFLASH_FAILURE);
	}

	if ((flashdev = calloc(1, sizeof (*flashdev))) == NULL ||
	    (flashdev->ident = calloc(1, sizeof (struct vpr))) == NULL ||
	    (info = calloc(1, sizeof (*info))) == NULL) {
		logmsg(MSG_ERROR, gettext("%s: failed to allocate a device "
		    "entry for %s\n"), drivername, path);
		goto fail;
	}
	flashdev->ident->encap_ident = info;

	if (asprintf(&flashdev->access_devname, "/devices%s:%s", path,
	    ESPIFW_MINOR) == -1) {
		flashdev->access_devname = NULL;
		logmsg(MSG_ERROR, gettext("%s: failed to construct the device "
		    "path for %s\n"), drivername, path);
		goto fail;
	}

	if ((fd = open(flashdev->access_devname, O_RDONLY)) < 0) {
		logmsg(MSG_INFO, gettext("%s: cannot open %s: %s\n"),
		    drivername, flashdev->access_devname, strerror(errno));
		*skipped = true;
		goto fail;
	}

	if (ioctl(fd, ESPI_FLASH_IOC_INFO, info) != 0) {
		logmsg(MSG_WARN, gettext("%s: failed to get flash information "
		    "from %s: %s\n"), drivername, flashdev->access_devname,
		    strerror(errno));
		(void) close(fd);
		*skipped = true;
		goto fail;
	}
	(void) close(fd);

	if ((flashdev->drvname = strdup(drivername)) == NULL ||
	    (flashdev->classname = strdup(drivername)) == NULL ||
	    (flashdev->ident->vid = strdup("Oxide")) == NULL) {
		logmsg(MSG_ERROR, gettext("%s: failed to allocate strings for "
		    "%s\n"), drivername, path);
		goto fail;
	}

	/*
	 * Name the device after its controller, so that the listing
	 * distinguishes the controllers without the full path.
	 */
	parent = di_parent_node(node);
	if (parent == DI_NODE_NIL || di_bus_addr(parent) == NULL) {
		flashdev->ident->pid = strdup("eSPI flash");
	} else {
		(void) asprintf(&flashdev->ident->pid, "eSPI flash (%s@%s)",
		    di_node_name(parent), di_bus_addr(parent));
	}
	if (flashdev->ident->pid == NULL) {
		logmsg(MSG_ERROR, gettext("%s: failed to allocate a device "
		    "name for %s\n"), drivername, path);
		goto fail;
	}

	di_devfs_path_free(path);
	flashdev->index = idx;
	flashdev->plugin = self;
	TAILQ_INSERT_TAIL(fw_devices, flashdev, nextdev);
	return (FWFLASH_SUCCESS);

fail:
	di_devfs_path_free(path);
	if (flashdev != NULL && flashdev->ident == NULL)
		free(info);
	espifw_flashdev_free(flashdev);
	return (FWFLASH_FAILURE);
}

int
fw_identify(int start)
{
	di_node_t node;
	int idx = start;
	uint_t nfound = 0;

	for (node = di_drv_first_node(drivername, rootnode);
	    node != DI_NODE_NIL; node = di_drv_next_node(node)) {
		bool skipped;

		if (espifw_add_device(node, idx, &skipped) == FWFLASH_SUCCESS) {
			idx++;
			nfound++;
		} else if (!skipped) {
			return (FWFLASH_FAILURE);
		}
	}

	return (nfound == 0 ? FWFLASH_FAILURE : FWFLASH_SUCCESS);
}

int
fw_devinfo(struct devicelist *flashdev)
{
	const espi_flash_info_t *info = flashdev->ident->encap_ident;
	bool first = true;

	(void) printf(gettext("Device[%d] %s\n"), flashdev->index,
	    flashdev->access_devname);
	(void) printf(gettext("Class [%s]\n"), flashdev->classname);
	(void) printf(gettext("\tVendor: %s\n\tDevice: %s\n"),
	    flashdev->ident->vid, flashdev->ident->pid);

	if (info->efi_size != 0) {
		(void) printf(gettext("\tSize: %" PRIu64 " bytes\n"),
		    info->efi_size);
	} else {
		(void) printf(gettext("\tSize: unknown\n"));
	}

	(void) printf(gettext("\tWrite request: %u bytes, read request: %u "
	    "bytes\n"), info->efi_payload, info->efi_maxread);

	(void) printf(gettext("\tErase block sizes: "));
	for (uint_t i = 0; i < ARRAY_SIZE(espifw_erase_sizes); i++) {
		if (!espifw_erase_size_advertised(info, espifw_erase_sizes[i]))
			continue;
		(void) printf("%s%u KiB", first ? "" : ", ",
		    espifw_erase_sizes[i] / 1024);
		first = false;
	}
	if (first) {
		(void) printf(gettext("not advertised, %u KiB assumed"),
		    ESPIFW_DEFAULT_ERASE / 1024);
	}
	(void) printf("\n\n\n");

	return (FWFLASH_SUCCESS);
}

/*
 * Save the whole flash to a file. That needs the size to be known.
 */
int
fw_readfw(struct devicelist *flashdev, char *filename)
{
	const espi_flash_info_t *info = flashdev->ident->encap_ident;
	int ret = FWFLASH_FAILURE;
	int dfd = -1, ffd = -1;
	uint64_t off = 0;
	void *buf;

	if (info->efi_size == 0) {
		logmsg(MSG_ERROR, gettext("%s: the size of the flash behind "
		    "%s is not known, so it cannot be read in full\n"),
		    drivername, flashdev->access_devname);
		return (FWFLASH_FAILURE);
	}

	if ((buf = malloc(ESPIFW_BUFLEN)) == NULL) {
		logmsg(MSG_ERROR, gettext("%s: failed to allocate a data "
		    "buffer\n"), drivername);
		return (FWFLASH_FAILURE);
	}

	if ((dfd = open(flashdev->access_devname, O_RDONLY)) < 0) {
		logmsg(MSG_ERROR, gettext("%s: failed to open %s: %s\n"),
		    drivername, flashdev->access_devname, strerror(errno));
		goto out;
	}

	if ((ffd = open(filename, O_CREAT | O_TRUNC | O_WRONLY, 0644)) < 0) {
		logmsg(MSG_ERROR, gettext("%s: failed to open file %s: %s\n"),
		    drivername, filename, strerror(errno));
		goto out;
	}

	logmsg(MSG_INFO, gettext("%s: reading %" PRIu64 " bytes from %s\n"),
	    drivername, info->efi_size, flashdev->access_devname);

	while (off < info->efi_size) {
		const size_t len = MIN(info->efi_size - off, ESPIFW_BUFLEN);

		if (!espifw_pread(dfd, buf, len, off)) {
			logmsg(MSG_ERROR, gettext("%s: failed to read %zu "
			    "bytes at offset %" PRIu64 " from %s: %s\n"),
			    drivername, len, off, flashdev->access_devname,
			    strerror(errno));
			goto out;
		}
		if (!espifw_pwrite(ffd, buf, len, off)) {
			logmsg(MSG_ERROR, gettext("%s: failed to write to %s: "
			    "%s\n"), drivername, filename, strerror(errno));
			goto out;
		}
		off += len;
	}

	if (close(ffd) != 0) {
		ffd = -1;
		logmsg(MSG_ERROR, gettext("%s: failed to finish writing to %s: "
		    "%s\n"), drivername, filename, strerror(errno));
		goto out;
	}
	ffd = -1;

	logmsg(MSG_INFO, gettext("%s: successfully wrote image to %s\n"),
	    drivername, filename);
	ret = FWFLASH_SUCCESS;

out:
	if (ffd >= 0)
		(void) close(ffd);
	if (dfd >= 0)
		(void) close(dfd);
	free(buf);
	return (ret);
}

/*
 * Write the image that fwflash has loaded into the verifier to the flash, one
 * erase block at a time, skipping blocks that already hold the right contents.
 * The device is opened exclusively so that no other process can use the flash
 * while it is being changed.
 */
int
fw_writefw(struct devicelist *flashdev)
{
	const espi_flash_info_t *info = flashdev->ident->encap_ident;
	const uint8_t *img = (const uint8_t *)verifier->fwimage;
	const size_t imgsize = verifier->imgsize;
	const uint32_t ebs = espifw_erase_size(info);
	uint_t nwritten = 0, nsame = 0;
	int ret = FWFLASH_FAILURE;
	int fd = -1;
	void *buf;

	if (imgsize == 0) {
		logmsg(MSG_ERROR, gettext("%s: image %s is empty\n"),
		    drivername, verifier->imgfile);
		return (FWFLASH_FAILURE);
	}

	if ((buf = malloc(ebs)) == NULL) {
		logmsg(MSG_ERROR, gettext("%s: failed to allocate a data "
		    "buffer\n"), drivername);
		return (FWFLASH_FAILURE);
	}

	if ((fd = open(flashdev->access_devname, O_RDWR | O_EXCL)) < 0) {
		logmsg(MSG_ERROR, gettext("%s: failed to open %s: %s\n"),
		    drivername, flashdev->access_devname, strerror(errno));
		goto out;
	}

	logmsg(MSG_INFO, gettext("%s: writing %zu bytes from %s to %s in %u "
	    "byte blocks\n"), drivername, imgsize, verifier->imgfile,
	    flashdev->access_devname, ebs);

	for (uint64_t off = 0; off < imgsize; off += ebs) {
		const size_t len = MIN(imgsize - off, ebs);
		const espi_flash_erase_t erase = {
			.efe_offset = off,
			.efe_length = ebs
		};

		if (!espifw_pread(fd, buf, len, off)) {
			logmsg(MSG_ERROR, gettext("%s: failed to read %zu "
			    "bytes at offset %" PRIu64 ": %s\n"), drivername,
			    len, off, strerror(errno));
			goto out;
		}

		if (memcmp(buf, img + off, len) == 0) {
			nsame++;
			continue;
		}

		logmsg(MSG_INFO, gettext("%s: block at offset 0x%" PRIx64
		    " differs from the image\n"), drivername, off);

		if (ioctl(fd, ESPI_FLASH_IOC_ERASE, &erase) != 0) {
			logmsg(MSG_ERROR, gettext("%s: failed to erase %u "
			    "bytes at offset %" PRIu64 ": %s\n"), drivername,
			    ebs, off, strerror(errno));
			goto out;
		}

		if (!espifw_pwrite(fd, img + off, len, off)) {
			logmsg(MSG_ERROR, gettext("%s: failed to write %zu "
			    "bytes at offset %" PRIu64 ": %s\n"), drivername,
			    len, off, strerror(errno));
			goto out;
		}

		if (!espifw_pread(fd, buf, len, off)) {
			logmsg(MSG_ERROR, gettext("%s: failed to read back %zu "
			    "bytes at offset %" PRIu64 ": %s\n"), drivername,
			    len, off, strerror(errno));
			goto out;
		}

		if (memcmp(buf, img + off, len) != 0) {
			logmsg(MSG_ERROR, gettext("%s: data read back from "
			    "offset %" PRIu64 " does not match the image\n"),
			    drivername, off);
			goto out;
		}

		nwritten++;
	}

	logmsg(MSG_INFO, gettext("%s: %u blocks written, %u already "
	    "matched the image\n"), drivername, nwritten, nsame);
	ret = FWFLASH_SUCCESS;

out:
	if (fd >= 0)
		(void) close(fd);
	free(buf);
	return (ret);
}

void
fw_cleanup(struct devicelist *flashdev)
{
	espifw_flashdev_free(flashdev);
}
