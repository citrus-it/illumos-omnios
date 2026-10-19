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

#ifndef _SYS_ESPI_CTL_H
#define	_SYS_ESPI_CTL_H

/*
 * User interface to espi(4D), the eSPI controller nexus driver.
 *
 * A controller that the driver owns has a "ctl" minor node, a general control
 * interface for the controller and its target. The ioctls on it are defined
 * here.
 */

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define	ESPI_IOC		(('e' << 24) | ('s' << 16) | ('p' << 8))

/*
 * ESPI_IOC_CFG_READ reads one of the target's configuration registers with
 * GET_CONFIGURATION. ESPI_IOC_CFG_WRITE writes one of its platform-specific
 * registers with SET_CONFIGURATION and requires the device to be open for
 * writing.
 */
#define	ESPI_IOC_CFG_READ	(ESPI_IOC | 1)
#define	ESPI_IOC_CFG_WRITE	(ESPI_IOC | 2)

/*
 * The target's configuration register space. Registers are 32 bits wide and
 * naturally aligned. Those from ESPI_CFG_PLATFORM_START are defined by the
 * platform rather than the eSPI specification, and only they may be written.
 */
#define	ESPI_CFG_SIZE		0x1000
#define	ESPI_CFG_PLATFORM_START	0x800

/*
 * The argument to ESPI_IOC_CFG_READ and ESPI_IOC_CFG_WRITE. ec_offset names
 * the register. ec_value is filled in by a read and gives the value for a
 * write.
 */
typedef struct espi_cfg {
	uint32_t	ec_offset;
	uint32_t	ec_value;
} espi_cfg_t;

#ifdef __cplusplus
}
#endif

#endif /* _SYS_ESPI_CTL_H */
