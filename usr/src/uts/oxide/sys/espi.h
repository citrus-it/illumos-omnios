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
 * Copyright 2024 Oxide Computer Company
 */

#ifndef _SYS_ESPI_H
#define	_SYS_ESPI_H

#include <sys/stdbool.h>
#include <sys/types.h>
#include <sys/uart.h>
#include <sys/amdzen/mmioreg.h>

#ifdef __cplusplus
extern "C" {
#endif

extern int espi_acquire(mmio_reg_block_t);
extern void espi_release(mmio_reg_block_t);
extern bool espi_readable(mmio_reg_block_t);
extern bool espi_writable(mmio_reg_block_t);
extern void espi_flush(mmio_reg_block_t);
extern int espi_tx(mmio_reg_block_t, uint8_t, uint8_t *, size_t *);
extern int espi_rx(mmio_reg_block_t, uint8_t, uint8_t *, size_t *);

extern uint32_t espi_get_configuration(mmio_reg_block_t);

#ifdef __cplusplus
}
#endif

#endif /* _SYS_ESPI_H */
