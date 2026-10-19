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
 * Generic fwflash(8) verifier for flash reached through espi_flash(4D). The
 * contents of such a flash belong to whatever device boots from it, and this
 * plugin knows nothing about their format. All it checks is that the image
 * will fit, using the espi_flash_info_t that the transport plugin stored as the
 * device's encapsulated identity.
 */

#include <sys/types.h>
#include <sys/espi_flash.h>
#include <libintl.h>
#include <fwflash/fwflash.h>

extern struct vrfyplugin *verifier;

/*
 * Declare the name of our vendor. This is required by the fwflash plugin
 * interface. It must be a character array, as fwflash looks it up with dlsym.
 */
char vendor[] = "GENERIC";

int
vendorvrfy(struct devicelist *dvp)
{
	const espi_flash_info_t *info = dvp->ident->encap_ident;

	if (verifier->imgsize == 0) {
		logmsg(MSG_ERROR, gettext("%s: firmware image %s is empty\n"),
		    verifier->vendor, verifier->imgfile);
		return (FWFLASH_FAILURE);
	}

	if (info->efi_size != 0 && verifier->imgsize > info->efi_size) {
		logmsg(MSG_ERROR, gettext("%s: firmware image %s (%u bytes) "
		    "is larger than the flash (%" PRIu64 " bytes)\n"),
		    verifier->vendor, verifier->imgfile, verifier->imgsize,
		    info->efi_size);
		return (FWFLASH_FAILURE);
	}

	logmsg(MSG_INFO, gettext("%s: image %s of %u bytes fits %s\n"),
	    verifier->vendor, verifier->imgfile, verifier->imgsize,
	    dvp->access_devname);

	verifier->flashbuf = 0;
	return (FWFLASH_SUCCESS);
}
