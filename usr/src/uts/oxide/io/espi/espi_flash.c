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
 * whose target has a flash behind it, reached over the eSPI flash access
 * channel with the flash attached to the target. It presents the flash as a
 * character device that is read and written at the offset of the flash
 * address, and erased through an ioctl. The transfers themselves are done by
 * the nexus (see espi_client.h), which splits them into requests the link
 * allows. This driver is concerned with the user interface, with bounding
 * requests to the device, and with describing the flash through the DDI UFM
 * framework.
 *
 * The flash is programmed in the usual way for NOR flash. The caller erases the
 * blocks it means to change and then writes them, and reads back to verify. The
 * driver does not erase on the caller's behalf, and the target may refuse
 * writes and erases altogether, which it reports with an unsuccessful
 * completion that comes back to the caller as EIO. On the boards we have, the
 * target is a sequencer FPGA that permits writes only while the service
 * processor has enabled them and granted the host the flash.
 *
 * Size
 *
 * The eSPI specification offers no way to ask the target how large the flash
 * is. The size comes from a flash-size property on our node, which the nexus
 * sets from the board data, or which can be given in espi_flash.conf. Without
 * one the size is unknown, in which case the whole 32-bit eSPI flash address
 * space is offered and no UFM image is registered. A target that reports its
 * flash size through a platform-specific capability register would be another
 * source for this.
 *
 * Exclusive Access
 *
 * The device may be opened with O_EXCL to keep other openers out while the
 * flash is reprogrammed. The nexus serialises individual requests regardless.
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
#include <sys/sysmacros.h>
#include <sys/kmem.h>
#include <sys/ddi_ufm.h>
#include <sys/espi_flash.h>

#include "espi_client.h"

/*
 * Data is moved between the user and the flash through a buffer of this size.
 */
#define	ESPI_FLASH_BUFSZ	4096

#define	ESPI_FLASH_MINOR_NAME	"flash"

typedef struct espi_flash {
	dev_info_t		*ef_dip;
	espi_flash_caps_t	ef_caps;
	uint64_t		ef_size;	/* 0 if unknown */
	/*
	 * ef_lock protects the open state. ef_iolock serialises transfers,
	 * which share ef_buf.
	 */
	kmutex_t		ef_lock;
	bool			ef_open;
	bool			ef_excl;
	kmutex_t		ef_iolock;
	uint8_t			*ef_buf;
	ddi_ufm_handle_t	*ef_ufmh;
} espi_flash_t;

static void *espi_flash_state;

/*
 * The end of the device, as an address one past the last byte. The flash
 * address space is 32 bits wide.
 */
static uint64_t
espi_flash_end(const espi_flash_t *ef)
{
	return (ef->ef_size != 0 ? ef->ef_size : (uint64_t)UINT32_MAX + 1);
}

/*
 * Bound a transfer of *lenp bytes at off to the device. Returns false if it
 * lies entirely outside it.
 */
static bool
espi_flash_bound(const espi_flash_t *ef, offset_t off, size_t *lenp)
{
	const uint64_t end = espi_flash_end(ef);

	if (off < 0 || (uint64_t)off >= end)
		return (false);

	*lenp = MIN(*lenp, end - (uint64_t)off);
	return (true);
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
	size_t len;
	int ret = 0;

	if (ef == NULL)
		return (ENXIO);

	len = uio->uio_resid;
	if (!espi_flash_bound(ef, uio->uio_loffset, &len))
		return (0);

	mutex_enter(&ef->ef_iolock);
	while (len > 0) {
		const uint32_t addr = (uint32_t)uio->uio_loffset;
		const size_t n = MIN(len, ESPI_FLASH_BUFSZ);

		ret = espi_c_flash_read(ef->ef_dip, addr, ef->ef_buf, n);
		if (ret != 0)
			break;
		ret = uiomove(ef->ef_buf, n, UIO_READ, uio);
		if (ret != 0)
			break;
		len -= n;
	}
	mutex_exit(&ef->ef_iolock);

	return (ret);
}

static int
espi_flash_write(dev_t dev, struct uio *uio, cred_t *cr)
{
	espi_flash_t *ef = ddi_get_soft_state(espi_flash_state, getminor(dev));
	size_t len;
	int ret = 0;

	if (ef == NULL)
		return (ENXIO);

	len = uio->uio_resid;
	if (!espi_flash_bound(ef, uio->uio_loffset, &len))
		return (ENOSPC);

	mutex_enter(&ef->ef_iolock);
	while (len > 0) {
		const uint32_t addr = (uint32_t)uio->uio_loffset;
		const size_t n = MIN(len, ESPI_FLASH_BUFSZ);

		ret = uiomove(ef->ef_buf, n, UIO_WRITE, uio);
		if (ret != 0)
			break;
		ret = espi_c_flash_write(ef->ef_dip, addr, ef->ef_buf, n);
		if (ret != 0)
			break;
		len -= n;
	}
	mutex_exit(&ef->ef_iolock);

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
		const espi_flash_info_t info = {
			.efi_size = ef->ef_size,
			.efi_payload = ef->ef_caps.efc_payload,
			.efi_maxread = ef->ef_caps.efc_maxread,
			.efi_erase_sizes = ef->ef_caps.efc_erase_sizes
		};

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

		mutex_enter(&ef->ef_iolock);
		ret = espi_c_flash_erase(ef->ef_dip, (uint32_t)er.efe_offset,
		    (uint32_t)er.efe_length);
		mutex_exit(&ef->ef_iolock);
		return (ret);
	}
	default:
		return (ENOTTY);
	}
}

/*
 * UFM
 *
 * The flash is presented as a single image with a single slot, so that its
 * contents can be read with the generic tools. There is no version information
 * at this level. It belongs to whatever is stored in the flash.
 */

static int
espi_flash_ufm_fill_image(ddi_ufm_handle_t *ufmh, void *arg, uint_t imgno,
    ddi_ufm_image_t *img)
{
	if (imgno != 0)
		return (EINVAL);

	ddi_ufm_image_set_desc(img, "flash");
	ddi_ufm_image_set_nslots(img, 1);
	return (0);
}

static int
espi_flash_ufm_fill_slot(ddi_ufm_handle_t *ufmh, void *arg, uint_t imgno,
    uint_t slotno, ddi_ufm_slot_t *slot)
{
	const espi_flash_t *ef = arg;

	if (imgno != 0 || slotno != 0)
		return (EINVAL);

	ddi_ufm_slot_set_version(slot, "unknown");
	ddi_ufm_slot_set_attrs(slot, DDI_UFM_ATTR_ACTIVE |
	    DDI_UFM_ATTR_READABLE | DDI_UFM_ATTR_WRITEABLE);
	ddi_ufm_slot_set_imgsize(slot, ef->ef_size);
	return (0);
}

static int
espi_flash_ufm_getcaps(ddi_ufm_handle_t *ufmh, void *arg, ddi_ufm_cap_t *caps)
{
	*caps = DDI_UFM_CAP_REPORT | DDI_UFM_CAP_READIMG;
	return (0);
}

static int
espi_flash_ufm_readimg(ddi_ufm_handle_t *ufmh, void *arg, uint_t imgno,
    uint_t slotno, uint64_t len, uint64_t offset, void *buf, uint64_t *nread)
{
	espi_flash_t *ef = arg;
	int ret;

	if (imgno != 0 || slotno != 0)
		return (EINVAL);
	if (offset > ef->ef_size || len > ef->ef_size - offset)
		return (EINVAL);

	mutex_enter(&ef->ef_iolock);
	ret = espi_c_flash_read(ef->ef_dip, (uint32_t)offset, buf, len);
	mutex_exit(&ef->ef_iolock);

	if (ret == 0)
		*nread = len;
	return (ret);
}

static ddi_ufm_ops_t espi_flash_ufm_ops = {
	.ddi_ufm_op_fill_image = espi_flash_ufm_fill_image,
	.ddi_ufm_op_fill_slot = espi_flash_ufm_fill_slot,
	.ddi_ufm_op_getcaps = espi_flash_ufm_getcaps,
	.ddi_ufm_op_readimg = espi_flash_ufm_readimg
};

/*
 * The nexus supplies the size as a 64-bit property. A property set in
 * espi_flash.conf is a 32-bit integer, which is accepted too, and treated as
 * unsigned.
 */
static uint64_t
espi_flash_get_size(const espi_flash_t *ef)
{
	int64_t size = ddi_prop_get_int64(DDI_DEV_T_ANY, ef->ef_dip,
	    DDI_PROP_DONTPASS, "flash-size", 0);

	if (size == 0) {
		size = (uint32_t)ddi_prop_get_int(DDI_DEV_T_ANY, ef->ef_dip,
		    DDI_PROP_DONTPASS, "flash-size", 0);
	}

	if (size < 0 || size > (int64_t)UINT32_MAX + 1) {
		dev_err(ef->ef_dip, CE_WARN, "ignoring invalid flash-size "
		    "property value %" PRId64, size);
		return (0);
	}

	return ((uint64_t)size);
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

	if ((ret = espi_c_flash_caps(dip, &ef->ef_caps)) != 0) {
		dev_err(dip, CE_WARN, "failed to get flash channel "
		    "capabilities: %d", ret);
		ddi_soft_state_free(espi_flash_state, inst);
		return (DDI_FAILURE);
	}

	ef->ef_size = espi_flash_get_size(ef);
	ef->ef_buf = kmem_alloc(ESPI_FLASH_BUFSZ, KM_SLEEP);
	mutex_init(&ef->ef_lock, NULL, MUTEX_DRIVER, NULL);
	mutex_init(&ef->ef_iolock, NULL, MUTEX_DRIVER, NULL);

	if (ddi_create_minor_node(dip, ESPI_FLASH_MINOR_NAME, S_IFCHR, inst,
	    DDI_PSEUDO, 0) != DDI_SUCCESS) {
		dev_err(dip, CE_WARN, "failed to create minor node");
		goto err;
	}

	if (ef->ef_size != 0) {
		if (ddi_ufm_init(dip, DDI_UFM_CURRENT_VERSION,
		    &espi_flash_ufm_ops, &ef->ef_ufmh, ef) != 0) {
			dev_err(dip, CE_WARN, "!failed to register with the "
			    "UFM framework");
			ef->ef_ufmh = NULL;
		} else {
			ddi_ufm_update(ef->ef_ufmh);
		}
	}

	dev_err(dip, CE_CONT, "?flash size %" PRIu64 " bytes%s, write %u, "
	    "read %u, erase sizes 0x%x\n", ef->ef_size,
	    ef->ef_size == 0 ? " (unknown)" : "", ef->ef_caps.efc_payload,
	    ef->ef_caps.efc_maxread, ef->ef_caps.efc_erase_sizes);
	ddi_report_dev(dip);
	return (DDI_SUCCESS);

err:
	ddi_remove_minor_node(dip, NULL);
	mutex_destroy(&ef->ef_iolock);
	mutex_destroy(&ef->ef_lock);
	kmem_free(ef->ef_buf, ESPI_FLASH_BUFSZ);
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

	if (ef->ef_ufmh != NULL)
		ddi_ufm_fini(ef->ef_ufmh);
	ddi_remove_minor_node(dip, NULL);
	mutex_destroy(&ef->ef_iolock);
	mutex_destroy(&ef->ef_lock);
	kmem_free(ef->ef_buf, ESPI_FLASH_BUFSZ);
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
