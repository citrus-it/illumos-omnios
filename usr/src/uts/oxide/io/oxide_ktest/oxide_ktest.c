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
 * Copyright 2025 Oxide Computer Company
 */

#include <sys/ktest.h>

static void
oxide_pciereg_capture(ktest_ctx_hdl_t *ctx)
{
	extern void zen_pcie_populate_dbg_adhoc(void);

	zen_pcie_populate_dbg_adhoc();

	KT_PASS(ctx);
}

static struct modlmisc oxide_modlmisc = {
	.misc_modops = &mod_miscops,
	.misc_linkinfo = "Oxide test module"
};

static struct modlinkage oxide_modlinkage = {
	.ml_rev = MODREV_1,
	.ml_linkage = { &oxide_modlmisc, NULL }
};

int
_init()
{
	ktest_module_hdl_t *km;
	ktest_suite_hdl_t *ks;
	int ret;

	VERIFY0(ktest_create_module("oxide", &km));

	VERIFY0(ktest_add_suite(km, "pcie", &ks));
	VERIFY0(ktest_add_test(ks, "capture", oxide_pciereg_capture,
	    KTEST_FLAG_NONE));

	if ((ret = ktest_register_module(km)) != 0) {
		ktest_free_module(km);
		return (ret);
	}

	if ((ret = mod_install(&oxide_modlinkage)) != 0) {
		ktest_unregister_module("oxide");
		return (ret);
	}

	return (0);
}

int
_fini(void)
{
	ktest_unregister_module("oxide");
	return (mod_remove(&oxide_modlinkage));
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&oxide_modlinkage, modinfop));
}
