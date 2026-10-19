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

#ifndef _SYS_ESPI_FLASH_H
#define	_SYS_ESPI_FLASH_H

/*
 * User interface to espi_flash(4D), the driver for a flash device attached to
 * an eSPI target and reached over the eSPI flash access channel.
 *
 * The device is read and written with read(2) and write(2) at the offset of
 * the flash address, and pread(2) and pwrite(2). The flash is not erased before
 * a write. Blocks must be erased explicitly with ESPI_FLASH_IOC_ERASE first
 * where the device requires it.
 */

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define	ESPI_FLASH_IOC		(('e' << 24) | ('f' << 16) | ('l' << 8))
#define	ESPI_FLASH_IOC_INFO	(ESPI_FLASH_IOC | 1)
#define	ESPI_FLASH_IOC_ERASE	(ESPI_FLASH_IOC | 2)

/*
 * ESPI_FLASH_IOC_INFO describes the device. efi_size is 0 if the size of the
 * flash is not known, in which case the whole 32-bit eSPI flash address space
 * is accessible. efi_erase_sizes has bit n set if blocks of 2^n KiB can be
 * erased. If it is 0 the target did not advertise any erase block sizes and
 * the driver accepts any size the protocol can encode.
 */
typedef struct espi_flash_info {
	uint64_t	efi_size;
	uint32_t	efi_payload;		/* max bytes per write */
	uint32_t	efi_maxread;		/* max bytes per read */
	uint32_t	efi_erase_sizes;
	uint32_t	efi_pad;
} espi_flash_info_t;

/*
 * ESPI_FLASH_IOC_ERASE erases one block. efe_length must be an erase block
 * size the device supports and efe_offset must be a multiple of it.
 */
typedef struct espi_flash_erase {
	uint64_t	efe_offset;
	uint64_t	efe_length;
} espi_flash_erase_t;

#ifdef __cplusplus
}
#endif

#endif /* _SYS_ESPI_FLASH_H */
