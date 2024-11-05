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
#include <sys/hexdump.h>
#include <sys/amdzen/mmioreg.h>
#include <sys/io/fch/espi.h>

static int
espi_hexdump_cb(void *arg __unused, uint64_t addr, const char *str,
        size_t len __unused)
{
	cmn_err(CE_WARN, "%s", str);
	return (0);
}

static void
espi_hexdump(const uint8_t *buf, size_t len)
{
	(void) hexdump(buf, len, HDF_ASCII, espi_hexdump_cb, NULL);
}

static void
espi_intr_status(void)
{
	mmio_reg_block_t block = fch_espi_mmio_block();
	uint32_t r = espi_intstatus(block);

	//XXX - use %b?
	const struct {
		bool set;
		const char *descr;
	} stats[] = {
		{ FCH_ESPI_S0_INT_STS_GET_RXMSG(r), "RXMSG" },
		{ FCH_ESPI_S0_INT_STS_GET_DNCMD(r), "DNCMD" },
		{ FCH_ESPI_S0_INT_STS_GET_PROTOERR(r), "PROTOERR" },
		{ FCH_ESPI_S0_INT_STS_GET_RXMSG_OFLOW(r), "RXMSG_OFLOW" },
		{ FCH_ESPI_S0_INT_STS_GET_ILL_LEN(r), "ILL_LEN" },
		{ FCH_ESPI_S0_INT_STS_GET_ILL_TAG(r), "ILL_TAG" },
		{ FCH_ESPI_S0_INT_STS_GET_USF_CPL(r), "UNSUCCESSFUL_CPL" },
		{ FCH_ESPI_S0_INT_STS_GET_UNK_CYC(r), "UNKNOWN CYCLE TYPE" },
		{ FCH_ESPI_S0_INT_STS_GET_UNK_RSP(r), "UNKNOWN RESP CODE" },
		{ FCH_ESPI_S0_INT_STS_GET_NFATAL_ERR(r), "NON-FATAL ERROR" },
		{ FCH_ESPI_S0_INT_STS_GET_FATAL_ERR(r), "FATAL ERROR" },
		{ FCH_ESPI_S0_INT_STS_GET_NO_RSP(r), "NO RESPONSE" },
		{ FCH_ESPI_S0_INT_STS_GET_CRC_ERR(r), "CRC ERROR" },
		{ FCH_ESPI_S0_INT_STS_GET_WAIT_TMT(r), "WAIT TIMEOUT" },
		{ FCH_ESPI_S0_INT_STS_GET_BUS_ERR(r), "BUS ERROR" }
	};
	char buf[0x400] = "";

	for (uint_t i = 0; i < ARRAY_SIZE(stats); i++) {
		if (!stats[i].set)
			continue;
		(void) strlcat(buf, stats[i].descr, sizeof (buf));
		(void) strlcat(buf, ",", sizeof (buf));
	}

	cmn_err(CE_WARN, "eSPI interrupt status: 0x%x", r);
	cmn_err(CE_WARN, " --> %s", buf);

	mmio_reg_block_unmap(&block);
}

static void
espi_query_config_test(ktest_ctx_hdl_t *ctx)
{
	mmio_reg_block_t block = fch_espi_mmio_block();
	uint32_t val;

	const struct {
		uint16_t reg;
		const char *descr;
	} registers[] = {
		{ ESPI_REG_IDENT, "ident" },
		{ ESPI_REG_GEN_CAP, "general" },
		{ ESPI_REG_CHAN0_CAP, "periph" },
		{ ESPI_REG_CHAN1_CAP, "vwire" },
		{ ESPI_REG_CHAN2_CAP, "OOB" },
		{ ESPI_REG_CHAN3_CAP, "flash1" },
		{ ESPI_REG_CHAN3_CAP2, "flash2" },
	};

	if (espi_acquire(block) != 0) {
		KT_ERROR(ctx, "Could not acquire semaphore");
		goto out;
	}

	for (uint_t i = 0; i < ARRAY_SIZE(registers); i++) {
		val = espi_get_configuration(block, registers[i].reg);
		cmn_err(CE_WARN, "eSPI cfg[%02x/%-7s]: 0x%x",
		    registers[i].reg, registers[i].descr, val);
	}

	espi_release(block);

	KT_PASS(ctx);
out:
	mmio_reg_block_unmap(&block);
}

static void
espi_query_intstatus_test(ktest_ctx_hdl_t *ctx)
{
	espi_intr_status();
	KT_PASS(ctx);
}

static void
espi_query_readable_test(ktest_ctx_hdl_t *ctx)
{
	mmio_reg_block_t block = fch_espi_mmio_block();

	cmn_err(CE_WARN, "eSPI readable status: %d",
	    espi_readable(block));

	mmio_reg_block_unmap(&block);
	KT_PASS(ctx);
}

static void
espi_query_writable_test(ktest_ctx_hdl_t *ctx)
{
	mmio_reg_block_t block = fch_espi_mmio_block();

	cmn_err(CE_WARN, "eSPI writable status: %d",
	    espi_writable(block));

	mmio_reg_block_unmap(&block);
	KT_PASS(ctx);
}

static void
espi_basic_peripheral_test(ktest_ctx_hdl_t *ctx)
{
	mmio_reg_block_t block = fch_espi_mmio_block();
	uint32_t val;

	if (espi_acquire(block) != 0) {
		KT_ERROR(ctx, "Could not acquire semaphore");
		mmio_reg_block_unmap(&block);
		return;
	}

	val = espi_get_configuration(block, ESPI_REG_IDENT);
	KT_ASSERT3UG(ESPI_REG_IDENT_VERSION(val), ==, 1, ctx, out);

	val = espi_get_configuration(block, ESPI_REG_GEN_CAP);
	KT_ASSERT3UG(ESPI_REG_GEN_CAP_PERIPH(val), ==, 1, ctx, out);

	val = espi_get_configuration(block, ESPI_REG_CHAN0_CAP);
	KT_ASSERT3UG(ESPI_REG_CHAN0_CAP_EN(val), ==, 1, ctx, out);

	KT_ASSERT3UG(ESPI_REG_CHAN0_CAP_READY(val), ==, 1, ctx, out);

	KT_PASS(ctx);
out:
	espi_release(block);
	mmio_reg_block_unmap(&block);
}

static void
espi_adhoc_flush_test(ktest_ctx_hdl_t *ctx)
{
	mmio_reg_block_t block = fch_espi_mmio_block();

	espi_flush(block);
	mmio_reg_block_unmap(&block);

	KT_PASS(ctx);
}

static void
espi_adhoc_tx_test(ktest_ctx_hdl_t *ctx)
{
	mmio_reg_block_t block = fch_espi_mmio_block();
	uchar_t *bytes = NULL;
	size_t len = 0;

	ktest_get_input(ctx, &bytes, &len);

	if (espi_acquire(block) != 0) {
		KT_ERROR(ctx, "Could not acquire semaphore");
	} else {
		int ret = espi_tx(block, bytes, &len);
		if (ret == 0) {
			KT_PASS(ctx);
		} else {
			KT_FAIL(ctx, "Err %d", ret);
		}
		espi_release(block);
	}
	mmio_reg_block_unmap(&block);

	espi_intr_status();
}

static void
espi_adhoc_rx_test(ktest_ctx_hdl_t *ctx)
{
	mmio_reg_block_t block = fch_espi_mmio_block();

	if (espi_acquire(block) != 0) {
		KT_ERROR(ctx, "Could not acquire semaphore");
	} else {
		uint8_t buf[0x1000];
		size_t len = sizeof (buf);

		if (espi_rx(block, buf, &len) != 0) {
			KT_FAIL(ctx, "Receive failed");
		} else {
			espi_hexdump(buf, len);
			KT_PASS(ctx);
		}
		espi_release(block);
	}
	mmio_reg_block_unmap(&block);
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

	VERIFY0(ktest_add_suite(km, "query", &ks));
	VERIFY0(ktest_add_test(ks, "config", espi_query_config_test,
	    KTEST_FLAG_NONE));
	VERIFY0(ktest_add_test(ks, "intstatus", espi_query_intstatus_test,
	    KTEST_FLAG_NONE));
	VERIFY0(ktest_add_test(ks, "readable", espi_query_readable_test,
	    KTEST_FLAG_NONE));
	VERIFY0(ktest_add_test(ks, "writable", espi_query_writable_test,
	    KTEST_FLAG_NONE));

	VERIFY0(ktest_add_suite(km, "adhoc", &ks));
	VERIFY0(ktest_add_test(ks, "tx", espi_adhoc_tx_test,
	    KTEST_FLAG_INPUT));
	VERIFY0(ktest_add_test(ks, "rx", espi_adhoc_rx_test,
	    KTEST_FLAG_NONE));
	VERIFY0(ktest_add_test(ks, "flush", espi_adhoc_flush_test,
	    KTEST_FLAG_NONE));

	VERIFY0(ktest_add_suite(km, "basic", &ks));
	VERIFY0(ktest_add_test(ks, "peripheral", espi_basic_peripheral_test,
	    KTEST_FLAG_NONE));

	if ((ret = ktest_register_module(km)) != 0) {
		ktest_free_module(km);
		return (ret);
	}

	if ((ret = mod_install(&espi_basic_modlinkage)) != 0) {
		ktest_unregister_module("espi");
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
