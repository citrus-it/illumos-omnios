/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2020 Beckhoff Automation GmbH & Co. KG
 * Author: Corvin K<C3><B6>hne <c.koehne@beckhoff.com>
 */

#ifndef _PCI_PASSTHRU_H_
#define _PCI_PASSTHRU_H_

#include <vmmapi.h>

#include "pci_emul.h"

struct passthru_softc;

typedef int (*cfgread_handler)(struct passthru_softc *sc,
    struct pci_devinst *pi, int coff, int bytes, uint32_t *rv);
typedef int (*cfgwrite_handler)(struct passthru_softc *sc,
    struct pci_devinst *pi, int coff, int bytes, uint32_t val);

uint32_t read_config(const struct pcisel *sel, long reg, int width);
void write_config(const struct pcisel *sel, long reg, int width, uint32_t data);
int set_pcir_handler(struct passthru_softc *sc, int reg, int len,
    cfgread_handler rhandler, cfgwrite_handler whandler);

#ifndef	__FreeBSD__
/*
 * This is not the right place for these, but it is also not the right place
 * for {read,write}_config().
 */
void pci_illumos_init(void);
void pci_illumos_fini(void);
int pci_illumos_find_lpc(struct pcisel *const);
#endif

#endif /* _PCI_PASSTHRU_H_ */
