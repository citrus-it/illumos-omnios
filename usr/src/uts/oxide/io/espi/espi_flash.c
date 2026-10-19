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
 * eSPI Flash Access Channel Driver
 *
 * This driver attaches to the espi_flash child of an espi(4D) nexus instance
 * whose target has flash behind it, reached over the eSPI flash access
 * channel with the flash attached to the target. It presents the flash as a
 * character device that is read and written at the offset of the flash
 * address, and erased through an ioctl. The requests themselves are issued by
 * the nexus. It splits a read or write into requests that the link allows,
 * while an erase is a single request for one block. This driver is concerned
 * with the user interface and with bounding requests to the device.
 *
 * The flash is programmed in the usual way for NOR flash. The caller erases the
 * blocks it means to change and then writes them, and reads back to verify. The
 * driver does not erase on the caller's behalf, and the target may not allow
 * writes and erases at all, which it reports with an unsuccessful completion
 * that comes back to the caller as EIO. On the boards we have, the
 * target is a sequencer FPGA that permits writes only while the service
 * processor has enabled them and granted the host the flash.
 *
 * Size
 *
 * The eSPI specification offers no way to ask the target how large the flash
 * is. The nexus supplies the size from the board data, and failing that it can
 * be given as a Size property in espi_flash.conf. Without either the size is
 * unknown, in which case the whole 32-bit eSPI flash address space is
 * accessible. When the size is known it is published as the Size property of
 * the minor node, so that stat(2) reports it. The size of the flash fitted is
 * a property of the board.
 *
 * Access
 *
 * Only a privileged process in the global zone may open the device. It may be
 * opened with O_EXCL for exclusive access while the flash is reprogrammed.
 *
 * Operations on the flash are serialised, as they share a buffer and a flash
 * operation can take seconds. The thread doing one is recorded as the owner
 * of the device and others sleep until it has finished, and can be
 * interrupted by a signal while they wait. The nexus serialises individual
 * requests on the link regardless.
 *
 * Partial Transfers
 *
 * A read or write is carried out as a series of requests. If one fails after
 * others have succeeded, the call returns the number of bytes that were
 * transferred and the error is reported only if nothing was. A caller that
 * repeats the call for the remainder sees the error then if it persists.
 */

#include <sys/types.h>
#include <sys/stdbool.h>
#include <sys/conf.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/modctl.h>
#include <sys/devops.h>
#include <sys/cmn_err.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/open.h>
#include <sys/uio.h>
#include <sys/ksynch.h>
#include <sys/cred.h>
#include <sys/zone.h>
#include <sys/sysmacros.h>
#include <sys/kmem.h>
#include <sys/espi_flash.h>

#include "espi_client.h"

/*
 * Data is moved between the user and the flash through a buffer of this size.
 * It is the largest read or write request that eSPI allows, so a full buffer
 * is never less than one request whatever the link has negotiated.
 */
#define	ESPI_FLASH_BUFSZ	4096

#define	ESPI_FLASH_MINOR_NAME	"flash"

/*
 * The standard property through which a character device reports its size.
 */
#define	ESPI_FLASH_PROP_SIZE	"Size"

typedef struct espi_flash {
	dev_info_t		*ef_dip;
	espi_flash_caps_t	ef_caps;
	uint64_t		ef_size;	/* 0 if unknown */
	/*
	 * ef_lock protects the open state and ef_owner. ef_owner is the thread
	 * that is operating on the flash, and so has the use of ef_buf, or
	 * NULL. Threads waiting to become the owner sleep on ef_cv.
	 */
	kmutex_t		ef_lock;
	kcondvar_t		ef_cv;
	bool			ef_open;
	bool			ef_excl;
	kthread_t		*ef_owner;
	uint8_t			*ef_buf;
} espi_flash_t;

static void *espi_flash_state;

/*
 * The end of the device, as an address one past the last byte. The flash
 * address space is 32 bits wide.
 */
static uint64_t
espi_flash_end(const espi_flash_t *ef)
{
	return ((ef->ef_size != 0) ? ef->ef_size : (uint64_t)UINT32_MAX + 1);
}

/*
 * Bound a transfer described by a uio to the device, setting *lenp to the
 * number of bytes that can be transferred before the end of the device is
 * reached, which is 0 if the transfer starts at or beyond the end. Fails with
 * EINVAL if the offset or the length is negative.
 */
static int
espi_flash_bound(const espi_flash_t *ef, const struct uio *uio, size_t *lenp)
{
	const uint64_t end = espi_flash_end(ef);
	uint64_t off;

	if (uio->uio_loffset < 0 || uio->uio_resid < 0)
		return (EINVAL);

	off = (uint64_t)uio->uio_loffset;
	*lenp = (off < end) ? MIN((uint64_t)uio->uio_resid, end - off) : 0;
	return (0);
}

/*
 * Become the owner of the device in order to operate on the flash, waiting
 * for any other thread to finish. Fails with EINTR if a signal arrives first.
 */
static int
espi_flash_enter(espi_flash_t *ef)
{
	mutex_enter(&ef->ef_lock);
	while (ef->ef_owner != NULL) {
		if (cv_wait_sig(&ef->ef_cv, &ef->ef_lock) == 0) {
			mutex_exit(&ef->ef_lock);
			return (EINTR);
		}
	}
	ef->ef_owner = curthread;
	mutex_exit(&ef->ef_lock);

	return (0);
}

static void
espi_flash_exit(espi_flash_t *ef)
{
	mutex_enter(&ef->ef_lock);
	VERIFY3P(ef->ef_owner, ==, curthread);
	ef->ef_owner = NULL;
	cv_signal(&ef->ef_cv);
	mutex_exit(&ef->ef_lock);
}

static int
espi_flash_open(dev_t *devp, int flag, int otyp, cred_t *cr)
{
	espi_flash_t *ef = ddi_get_soft_state(espi_flash_state,
	    getminor(*devp));
	int ret = 0;

	if (ef == NULL)
		return (ENXIO);
	if (otyp != OTYP_CHR)
		return (EINVAL);
	if ((flag & (FNDELAY | FNONBLOCK)) != 0)
		return (EINVAL);
	if (crgetzoneid(cr) != GLOBAL_ZONEID || drv_priv(cr) != 0)
		return (EPERM);

	mutex_enter(&ef->ef_lock);
	if (ef->ef_excl || ((flag & FEXCL) != 0 && ef->ef_open)) {
		ret = EBUSY;
	} else {
		ef->ef_open = true;
		ef->ef_excl = (flag & FEXCL) != 0;
	}
	mutex_exit(&ef->ef_lock);

	return (ret);
}

/*
 * As a non-cloning character device, we are only told about the last close.
 */
static int
espi_flash_close(dev_t dev, int flag, int otyp, cred_t *cr)
{
	espi_flash_t *ef = ddi_get_soft_state(espi_flash_state, getminor(dev));

	if (ef == NULL)
		return (ENXIO);
	if (otyp != OTYP_CHR)
		return (EINVAL);

	mutex_enter(&ef->ef_lock);
	ef->ef_open = false;
	ef->ef_excl = false;
	mutex_exit(&ef->ef_lock);

	return (0);
}

static int
espi_flash_read(dev_t dev, struct uio *uio, cred_t *cr)
{
	espi_flash_t *ef = ddi_get_soft_state(espi_flash_state, getminor(dev));
	const ssize_t resid = uio->uio_resid;
	size_t len;
	int ret;

	if (ef == NULL)
		return (ENXIO);
	if ((uio->uio_fmode & FREAD) == 0)
		return (EBADF);
	if ((uio->uio_fmode & (FNDELAY | FNONBLOCK)) != 0)
		return (EINVAL);

	/* A read that starts at or beyond the end of the device returns 0 */
	if ((ret = espi_flash_bound(ef, uio, &len)) != 0 || len == 0)
		return (ret);

	if ((ret = espi_flash_enter(ef)) != 0)
		return (ret);

	while (len > 0) {
		const uint32_t addr = (uint32_t)uio->uio_loffset;
		const size_t n = MIN(len, ESPI_FLASH_BUFSZ);
		size_t done;
		int uret;

		/*
		 * Pass on whatever was read, even if the read then failed.
		 */
		ret = espi_c_flash_read(ef->ef_dip, addr, ef->ef_buf, n, &done);
		uret = uiomove(ef->ef_buf, done, UIO_READ, uio);
		if (ret == 0)
			ret = uret;
		if (ret != 0)
			break;
		len -= n;
	}

	espi_flash_exit(ef);

	/* See the theory statement on partial transfers */
	if (ret != 0 && uio->uio_resid != resid)
		ret = 0;

	return (ret);
}

static int
espi_flash_write(dev_t dev, struct uio *uio, cred_t *cr)
{
	espi_flash_t *ef = ddi_get_soft_state(espi_flash_state, getminor(dev));
	const ssize_t resid = uio->uio_resid;
	size_t len;
	int ret;

	if (ef == NULL)
		return (ENXIO);
	if ((uio->uio_fmode & FWRITE) == 0)
		return (EBADF);
	if ((uio->uio_fmode & (FNDELAY | FNONBLOCK)) != 0)
		return (EINVAL);

	if ((ret = espi_flash_bound(ef, uio, &len)) != 0)
		return (ret);
	if (len == 0)
		return ((resid == 0) ? 0 : ENOSPC);

	if ((ret = espi_flash_enter(ef)) != 0)
		return (ret);

	while (len > 0) {
		const uint32_t addr = (uint32_t)uio->uio_loffset;
		const size_t n = MIN(len, ESPI_FLASH_BUFSZ);
		size_t copied, done;

		/*
		 * Copy the data in without consuming it, and then consume only
		 * as much as was written.
		 */
		ret = uiocopy(ef->ef_buf, n, UIO_WRITE, uio, &copied);
		if (ret != 0)
			break;
		ret = espi_c_flash_write(ef->ef_dip, addr, ef->ef_buf, n,
		    &done);
		uioskip(uio, done);
		if (ret != 0)
			break;
		len -= n;
	}

	espi_flash_exit(ef);

	/* See the theory statement on partial transfers */
	if (ret != 0 && uio->uio_resid != resid)
		ret = 0;

	return (ret);
}

static int
espi_flash_ioctl(dev_t dev, int cmd, intptr_t arg, int mode, cred_t *cr,
    int *rvalp)
{
	espi_flash_t *ef = ddi_get_soft_state(espi_flash_state, getminor(dev));
	void *datap = (void *)arg;
	const int cflag = mode & FKIOCTL;
	int ret;

	if (ef == NULL)
		return (ENXIO);

	switch (cmd) {
	case ESPI_FLASH_IOC_INFO: {
		espi_flash_info_t info;

		bzero(&info, sizeof (info));
		info.efi_size = ef->ef_size;
		info.efi_payload = ef->ef_caps.efc_payload;
		info.efi_maxread = ef->ef_caps.efc_maxread;
		info.efi_erase_sizes = ef->ef_caps.efc_erase_sizes;

		if (ddi_copyout(&info, datap, sizeof (info), cflag) != 0)
			return (EFAULT);
		return (0);
	}
	case ESPI_FLASH_IOC_ERASE: {
		const uint64_t end = espi_flash_end(ef);
		espi_flash_erase_t er;

		if ((mode & FWRITE) == 0)
			return (EBADF);
		if (ddi_copyin(datap, &er, sizeof (er), cflag) != 0)
			return (EFAULT);

		/*
		 * The nexus checks that the length is an erase block size and
		 * the offset a multiple of it. We check that the block lies
		 * within the device.
		 */
		if (er.efe_length == 0 || er.efe_length > end ||
		    er.efe_offset > end - er.efe_length) {
			return (EINVAL);
		}

		if ((ret = espi_flash_enter(ef)) != 0)
			return (ret);
		ret = espi_c_flash_erase(ef->ef_dip, (uint32_t)er.efe_offset,
		    (uint32_t)er.efe_length);
		espi_flash_exit(ef);
		return (ret);
	}
	default:
		return (ENOTTY);
	}
}

/*
 * The size of the flash. The nexus supplies it if the board data has it.
 * Otherwise we look for a Size property from espi_flash.conf, which is a
 * 32-bit integer and is treated as unsigned.
 */
static uint64_t
espi_flash_get_size(const espi_flash_t *ef)
{
	uint64_t size = ef->ef_caps.efc_size;

	if (size == 0) {
		size = (uint32_t)ddi_prop_get_int(DDI_DEV_T_ANY, ef->ef_dip,
		    DDI_PROP_DONTPASS, ESPI_FLASH_PROP_SIZE, 0);
	}

	if (size > (uint64_t)UINT32_MAX + 1) {
		dev_err(ef->ef_dip, CE_WARN,
		    "ignoring invalid flash size %" PRIu64, size);
		return (0);
	}

	return (size);
}

/*
 * Publish what we know about the flash as properties. The size is a property
 * of the minor node, which is where specfs looks for it. The link limits and
 * erase block sizes are properties of the node.
 */
static bool
espi_flash_set_props(const espi_flash_t *ef)
{
	dev_info_t *dip = ef->ef_dip;
	const dev_t dev = makedevice(ddi_driver_major(dip),
	    ddi_get_instance(dip));

	if (ef->ef_size != 0 && ddi_prop_update_int64(dev, dip,
	    ESPI_FLASH_PROP_SIZE, (int64_t)ef->ef_size) != DDI_PROP_SUCCESS) {
		return (false);
	}

	return (ddi_prop_update_int(DDI_DEV_T_NONE, dip, "flash-payload",
	    (int)ef->ef_caps.efc_payload) == DDI_PROP_SUCCESS &&
	    ddi_prop_update_int(DDI_DEV_T_NONE, dip, "flash-max-read",
	    (int)ef->ef_caps.efc_maxread) == DDI_PROP_SUCCESS &&
	    ddi_prop_update_int(DDI_DEV_T_NONE, dip, "flash-erase-sizes",
	    (int)ef->ef_caps.efc_erase_sizes) == DDI_PROP_SUCCESS);
}

static int
espi_flash_getinfo(dev_info_t *dip, ddi_info_cmd_t cmd, void *arg,
    void **resultp)
{
	const minor_t minor = getminor((dev_t)arg);
	espi_flash_t *ef;

	switch (cmd) {
	case DDI_INFO_DEVT2DEVINFO:
		ef = ddi_get_soft_state(espi_flash_state, minor);
		if (ef == NULL)
			return (DDI_FAILURE);
		*resultp = ef->ef_dip;
		return (DDI_SUCCESS);
	case DDI_INFO_DEVT2INSTANCE:
		*resultp = (void *)(uintptr_t)minor;
		return (DDI_SUCCESS);
	default:
		return (DDI_FAILURE);
	}
}

static int
espi_flash_attach(dev_info_t *dip, ddi_attach_cmd_t cmd)
{
	const int inst = ddi_get_instance(dip);
	espi_flash_t *ef;
	int ret;

	switch (cmd) {
	case DDI_ATTACH:
		break;
	case DDI_RESUME:
		return (DDI_SUCCESS);
	default:
		return (DDI_FAILURE);
	}

	if (ddi_soft_state_zalloc(espi_flash_state, inst) != DDI_SUCCESS) {
		dev_err(dip, CE_WARN, "failed to allocate soft state");
		return (DDI_FAILURE);
	}
	ef = ddi_get_soft_state(espi_flash_state, inst);
	ef->ef_dip = dip;
	ddi_set_driver_private(dip, ef);

	if ((ret = espi_c_flash_caps(dip, &ef->ef_caps)) != 0) {
		dev_err(dip, CE_WARN,
		    "failed to get flash channel capabilities: %d", ret);
		ddi_set_driver_private(dip, NULL);
		ddi_soft_state_free(espi_flash_state, inst);
		return (DDI_FAILURE);
	}

	ef->ef_size = espi_flash_get_size(ef);
	ef->ef_buf = kmem_alloc(ESPI_FLASH_BUFSZ, KM_SLEEP);
	mutex_init(&ef->ef_lock, NULL, MUTEX_DRIVER, NULL);
	cv_init(&ef->ef_cv, NULL, CV_DRIVER, NULL);

	if (ddi_create_minor_node(dip, ESPI_FLASH_MINOR_NAME, S_IFCHR, inst,
	    DDI_PSEUDO, 0) != DDI_SUCCESS) {
		dev_err(dip, CE_WARN, "failed to create minor node");
		goto err;
	}

	if (!espi_flash_set_props(ef)) {
		dev_err(dip, CE_WARN, "failed to create properties");
		goto err;
	}

	ddi_report_dev(dip);
	return (DDI_SUCCESS);

err:
	ddi_prop_remove_all(dip);
	ddi_remove_minor_node(dip, NULL);
	cv_destroy(&ef->ef_cv);
	mutex_destroy(&ef->ef_lock);
	kmem_free(ef->ef_buf, ESPI_FLASH_BUFSZ);
	ddi_set_driver_private(dip, NULL);
	ddi_soft_state_free(espi_flash_state, inst);
	return (DDI_FAILURE);
}

static int
espi_flash_detach(dev_info_t *dip, ddi_detach_cmd_t cmd)
{
	const int inst = ddi_get_instance(dip);
	espi_flash_t *ef;

	switch (cmd) {
	case DDI_DETACH:
		break;
	case DDI_SUSPEND:
		return (DDI_SUCCESS);
	default:
		return (DDI_FAILURE);
	}

	ef = ddi_get_soft_state(espi_flash_state, inst);
	if (ef == NULL || ef->ef_dip != dip) {
		dev_err(dip, CE_WARN, "asked to detach instance with no state");
		return (DDI_FAILURE);
	}

	mutex_enter(&ef->ef_lock);
	if (ef->ef_open) {
		mutex_exit(&ef->ef_lock);
		return (DDI_FAILURE);
	}
	mutex_exit(&ef->ef_lock);

	ddi_prop_remove_all(dip);
	ddi_remove_minor_node(dip, NULL);
	cv_destroy(&ef->ef_cv);
	mutex_destroy(&ef->ef_lock);
	kmem_free(ef->ef_buf, ESPI_FLASH_BUFSZ);
	ddi_set_driver_private(dip, NULL);
	ddi_soft_state_free(espi_flash_state, inst);
	return (DDI_SUCCESS);
}

static struct cb_ops espi_flash_cb_ops = {
	.cb_open = espi_flash_open,
	.cb_close = espi_flash_close,
	.cb_strategy = nodev,
	.cb_print = nodev,
	.cb_dump = nodev,
	.cb_read = espi_flash_read,
	.cb_write = espi_flash_write,
	.cb_ioctl = espi_flash_ioctl,
	.cb_devmap = nodev,
	.cb_mmap = nodev,
	.cb_segmap = nodev,
	.cb_chpoll = nochpoll,
	.cb_prop_op = ddi_prop_op,
	.cb_flag = D_MP | D_64BIT,
	.cb_rev = CB_REV,
	.cb_aread = nodev,
	.cb_awrite = nodev
};

static struct dev_ops espi_flash_dev_ops = {
	.devo_rev = DEVO_REV,
	.devo_refcnt = 0,
	.devo_getinfo = espi_flash_getinfo,
	.devo_identify = nulldev,
	.devo_probe = nulldev,
	.devo_attach = espi_flash_attach,
	.devo_detach = espi_flash_detach,
	.devo_reset = nodev,
	.devo_cb_ops = &espi_flash_cb_ops,
	.devo_quiesce = ddi_quiesce_not_needed
};

static struct modldrv espi_flash_modldrv = {
	.drv_modops = &mod_driverops,
	.drv_linkinfo = "eSPI Flash Access Channel Driver",
	.drv_dev_ops = &espi_flash_dev_ops
};

static struct modlinkage espi_flash_modlinkage = {
	.ml_rev = MODREV_1,
	.ml_linkage = { &espi_flash_modldrv, NULL }
};

int
_init(void)
{
	int ret;

	ret = ddi_soft_state_init(&espi_flash_state, sizeof (espi_flash_t), 1);
	if (ret != 0)
		return (ret);

	if ((ret = mod_install(&espi_flash_modlinkage)) != 0)
		ddi_soft_state_fini(&espi_flash_state);

	return (ret);
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&espi_flash_modlinkage, modinfop));
}

int
_fini(void)
{
	int ret;

	if ((ret = mod_remove(&espi_flash_modlinkage)) == 0)
		ddi_soft_state_fini(&espi_flash_state);

	return (ret);
}
