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

#ifndef _IO_ESPI_ESPI_CLIENT_H
#define	_IO_ESPI_ESPI_CLIENT_H

/*
 * Routines provided by the espi nexus driver to the drivers for its children.
 * Each takes the child's own dev_info_t, from which the nexus locates the
 * controller and checks that the child is the one for the channel in question.
 */

#include <sys/types.h>
#include <sys/dditypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Flash Access Channel
 *
 * The flash is addressed with 32-bit addresses. Reads and writes of any length
 * are split by the nexus into requests that the link parameters allow. An
 * erase covers a single block whose size must be one the protocol can encode
 * and, if the target advertised any, one of those.
 */
typedef struct espi_flash_caps {
	uint32_t	efc_payload;		/* max bytes per write */
	uint32_t	efc_maxread;		/* max bytes per read */
	uint32_t	efc_erase_sizes;	/* bit n set for 2^n KiB */
} espi_flash_caps_t;

extern int espi_c_flash_caps(dev_info_t *, espi_flash_caps_t *);
extern int espi_c_flash_read(dev_info_t *, uint32_t, void *, size_t);
extern int espi_c_flash_write(dev_info_t *, uint32_t, const void *, size_t);
extern int espi_c_flash_erase(dev_info_t *, uint32_t, uint32_t);

#ifdef __cplusplus
}
#endif

#endif /* _IO_ESPI_ESPI_CLIENT_H */
