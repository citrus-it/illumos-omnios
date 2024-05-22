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

#include <sys/ktest.h>
#include <sys/espi.h>
#include <sys/amdzen/mmioreg.h>
#include <sys/io/fch/espi.h>

void
espi_basic_getconf(ktest_ctx_hdl_t *ctx)
{
	mmio_reg_block_t block = fch_espi_mmio_block();
	uint32_t val;

	val = espi_get_configuration(block);
	mmio_reg_block_unmap(&block);

	cmn_err(CE_WARN, "eSPI getconf: 0x%x", val);

	KT_PASS(ctx);
}

static struct modlmisc espi_basic_modlmisc = {
	.misc_modops = &mod_miscops,
	.misc_linkinfo = "Oxide eSPI test module"
};

static struct modlinkage espi_basic_modlinkage = {
	.ml_rev = MODREV_1,
	.ml_linkage = { &espi_basic_modlmisc, NULL }
};

int
_init()
{
	ktest_module_hdl_t *km;
	ktest_suite_hdl_t *ks;
	int ret;

	VERIFY0(ktest_create_module("espi", &km));
	VERIFY0(ktest_add_suite(km, "basic", &ks));
	VERIFY0(ktest_add_test(ks, "get_configuration", espi_basic_getconf,
	    KTEST_FLAG_NONE));

	if ((ret = ktest_register_module(km)) != 0) {
		ktest_free_module(km);
		return (ret);
	}

	if ((ret = mod_install(&espi_basic_modlinkage)) != 0) {
		ktest_unregister_module("oxide");
		return (ret);
	}

	return (0);
}

int
_fini(void)
{
	ktest_unregister_module("espi");
	return (mod_remove(&espi_basic_modlinkage));
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&espi_basic_modlinkage, modinfop));
}
