
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

#include <sys/ktest.h>
#include <sys/modctl.h>
#include <sys/kobj.h>
#include <sys/comm_page.h>
#include <sys/stdbool.h>
#include <sys/cpuvar.h>
#include <sys/x_call.h>
#include <sys/x86_archext.h>
#include <sys/amdzen/ccx.h>

static void
comm_page_vars_test(ktest_ctx_hdl_t *ctx)
{
	modctl_t *hdl = NULL;

	if ((hdl = mod_hold_by_name("unix")) == NULL) {
		KT_ERROR(ctx, "failed to hold 'unix' module");
		return;
	}

	const uintptr_t base = kobj_lookup(hdl->mod_mp, "comm_page");
	if (base == 0) {
		KT_ERROR(ctx, "failed to locate 'comm_page' symbol");
		goto cleanup;
	}

	/*
	 * Check field offsets in comm page, ensuring they match up with the
	 * offsets of the variables they represent.
	 */
	typedef struct var_check {
		const char	*name;
		uintptr_t	offset;
	} var_check_t;
	const var_check_t var_checks[] = {
		{
			.name = "tsc_last",
			.offset = offsetof(comm_page_t, cp_tsc_last),
		},
		{
			.name = "tsc_hrtime_base",
			.offset = offsetof(comm_page_t, cp_tsc_hrtime_base),
		},
		{
			.name = "tsc_resume_cap",
			.offset = offsetof(comm_page_t, cp_tsc_resume_cap),
		},
		{
			.name = "tsc_type",
			.offset = offsetof(comm_page_t, cp_tsc_type),
		},
		{
			.name = "tsc_max_delta",
			.offset = offsetof(comm_page_t, cp_tsc_max_delta),
		},
		{
			.name = "hres_lock",
			.offset = offsetof(comm_page_t, cp_hres_lock),
		},
		{
			.name = "nsec_scale",
			.offset = offsetof(comm_page_t, cp_nsec_scale),
		},
		{
			.name = "hrestime_adj",
			.offset = offsetof(comm_page_t, cp_hrestime_adj),
		},
		{
			.name = "hres_last_tick",
			.offset = offsetof(comm_page_t, cp_hres_last_tick),
		},
		{
			.name = "tsc_ncpu",
			.offset = offsetof(comm_page_t, cp_tsc_ncpu),
		},
		{
			.name = "hrestime",
			.offset = offsetof(comm_page_t, cp_hrestime),
		},
		{
			.name = "tsc_sync_tick_delta",
			.offset = offsetof(comm_page_t, cp_tsc_sync_tick_delta),
		},
	};
	for (uint_t i = 0; i < ARRAY_SIZE(var_checks); i++) {
		const var_check_t *var = &var_checks[i];

		const uintptr_t addr = kobj_lookup(hdl->mod_mp, var->name);
		if (addr == 0) {
			KT_ERROR(ctx, "failed to locate '%s' symbol",
			    var->name);
			goto cleanup;
		}
		const uintptr_t var_off = (addr - base);
		if (var_off != var->offset) {
			KT_FAIL(ctx,
			    "unexpected offset for symbol '%s': %lu != %lu",
			    var->name, var_off, var->offset);
			goto cleanup;
		}
	}

	/*
	 * Check that if cp_tsc_ncpu is non-zero, that a tsc_tick_delta-aware
	 * gethrtime has been selected.
	 */
	const comm_page_t *cp = (const comm_page_t *)base;
	if (cp->cp_tsc_ncpu != 0) {
		const uintptr_t *ghrt_func =
		    (const uintptr_t *)kobj_lookup(hdl->mod_mp, "gethrtimef");
		if (ghrt_func == NULL) {
			KT_ERROR(ctx, "failed to locate 'gethrtimef' symbol");
			goto cleanup;
		}
		const uintptr_t ghrt_delta =
		    kobj_lookup(hdl->mod_mp, "tsc_gethrtime_delta");
		if (*ghrt_func != ghrt_delta) {
			KT_FAIL(ctx,
			    "tsc_gethrtime_delta not used for gethrtimef: "
			    "%x != %x\n",
			    ghrt_delta, *ghrt_func);
			goto cleanup;
		}
	}

	KT_PASS(ctx);

cleanup:
	mod_release_mod(hdl);
}

static void
oxide_pciereg_capture(ktest_ctx_hdl_t *ctx)
{
	extern void zen_pcie_populate_dbg_adhoc(void);

	zen_pcie_populate_dbg_adhoc();

	KT_PASS(ctx);
}

extern cpuset_t cpu_ready_set;

typedef struct ptwalk_req {
	uint64_t	pr_tw_set;
	uint64_t	pr_tw_clr;
	uint64_t	pr_ls4_set;
	uint64_t	pr_ls4_clr;
	bool		pr_write;
	uint64_t	*pr_tw;
	uint64_t	*pr_ls4;
} ptwalk_req_t;

static int
ptwalk_xc(xc_arg_t a1, xc_arg_t a2 __unused, xc_arg_t a3 __unused)
{
	ptwalk_req_t *req = (ptwalk_req_t *)a1;
	const processorid_t id = CPU->cpu_id;
	uint64_t v;

	if (req->pr_write) {
		v = rdmsr(MSR_AMD_LS_CFG4);
		v = (v & ~req->pr_ls4_clr) | req->pr_ls4_set;
		wrmsr(MSR_AMD_LS_CFG4, v);

		/*
		 * Always written, even when unchanged, as this is the write
		 * that invalidates the translation caches on this core.
		 */
		v = rdmsr(MSR_AMD_TW_CFG);
		v = (v & ~req->pr_tw_clr) | req->pr_tw_set;
		wrmsr(MSR_AMD_TW_CFG, v);
	}

	req->pr_tw[id] = rdmsr(MSR_AMD_TW_CFG);
	req->pr_ls4[id] = rdmsr(MSR_AMD_LS_CFG4);

	return (0);
}

static bool
ptwalk_run(ktest_ctx_hdl_t *ctx, ptwalk_req_t *req, const char *desc)
{
	const size_t sz = max_ncpus * sizeof (uint64_t);
	cpuset_t set;
	uint64_t tw = 0, ls4 = 0;
	uint_t ncpu = 0;
	bool ok = true;

	if (chiprev_family(cpuid_getchiprev(CPU)) != X86_PF_AMD_MILAN) {
		KT_SKIP(ctx, "not a Milan processor");
		return (false);
	}

	req->pr_tw = kmem_zalloc(sz, KM_SLEEP);
	req->pr_ls4 = kmem_zalloc(sz, KM_SLEEP);

	kpreempt_disable();
	set = cpu_ready_set;
	xc_call((xc_arg_t)req, 0, 0, CPUSET2BV(set), ptwalk_xc);
	kpreempt_enable();

	for (processorid_t id = 0; id < max_ncpus; id++) {
		if (CPU_IN_SET(set, id) == 0)
			continue;
		if (ncpu == 0) {
			tw = req->pr_tw[id];
			ls4 = req->pr_ls4[id];
		}
		ncpu++;
		if ((req->pr_tw[id] & req->pr_tw_set) != req->pr_tw_set ||
		    (req->pr_tw[id] & req->pr_tw_clr) != 0) {
			KT_FAIL(ctx, "%s: cpu %d TW_CFG 0x%lx does not "
			    "reflect set 0x%lx clr 0x%lx", desc, id,
			    req->pr_tw[id], req->pr_tw_set, req->pr_tw_clr);
			ok = false;
			break;
		}
		if ((req->pr_ls4[id] & req->pr_ls4_set) != req->pr_ls4_set ||
		    (req->pr_ls4[id] & req->pr_ls4_clr) != 0) {
			KT_FAIL(ctx, "%s: cpu %d LS_CFG4 0x%lx does not "
			    "reflect set 0x%lx clr 0x%lx", desc, id,
			    req->pr_ls4[id], req->pr_ls4_set, req->pr_ls4_clr);
			ok = false;
			break;
		}
		if (req->pr_tw[id] != tw || req->pr_ls4[id] != ls4) {
			KT_FAIL(ctx, "%s: cpu %d TW_CFG 0x%lx LS_CFG4 0x%lx "
			    "differ from the first CPU's 0x%lx 0x%lx", desc,
			    id, req->pr_tw[id], req->pr_ls4[id], tw, ls4);
			ok = false;
			break;
		}
	}

	if (ok) {
		cmn_err(CE_NOTE, "ptwalk %s: TW_CFG 0x%lx LS_CFG4 0x%lx on "
		    "%u CPUs", desc, tw, ls4, ncpu);
	}

	kmem_free(req->pr_tw, sz);
	kmem_free(req->pr_ls4, sz);

	return (ok);
}

static uint64_t
ptwalk_tw_trial_bits(void)
{
	uint64_t v = 0;

	v = AMD_TW_CFG_SET_EN_A_BIT_COALESCING(v, 1);
	v = AMD_TW_CFG_SET_DIS_NESTED_COALESCED_LEAF_CACHE(v, 1);
	v = AMD_TW_CFG_SET_DIS_NESTED_PDE_CACHE(v, 1);
	v = AMD_TW_CFG_SET_DIS_GUEST_PDE_CACHE(v, 1);
	v = AMD_TW_CFG_SET_DIS_HOST_NATIVE_PDE_CACHE(v, 1);
	v = AMD_TW_CFG_SET_DIS_32K_PAGES(v, 1);

	return (v);
}

/* Report the current values, which must be identical on every CPU. */
static void
ptwalk_read(ktest_ctx_hdl_t *ctx)
{
	ptwalk_req_t req = { .pr_write = false };

	if (ptwalk_run(ctx, &req, "read"))
		KT_PASS(ctx);
}

/* AMD variation 1a: no PDE caching for native walks. */
static void
ptwalk_pde_cache_native(ktest_ctx_hdl_t *ctx)
{
	ptwalk_req_t req = { .pr_write = true };

	req.pr_tw_set = AMD_TW_CFG_SET_DIS_HOST_NATIVE_PDE_CACHE(0, 1);
	req.pr_tw_clr = ptwalk_tw_trial_bits() & ~req.pr_tw_set;
	if (ptwalk_run(ctx, &req, "pde_cache_native"))
		KT_PASS(ctx);
}

/* AMD variation 1b: no PDE caching for native, guest or nested walks. */
static void
ptwalk_pde_cache_nested(ktest_ctx_hdl_t *ctx)
{
	ptwalk_req_t req = { .pr_write = true };
	uint64_t v = 0;

	v = AMD_TW_CFG_SET_DIS_HOST_NATIVE_PDE_CACHE(v, 1);
	v = AMD_TW_CFG_SET_DIS_GUEST_PDE_CACHE(v, 1);
	v = AMD_TW_CFG_SET_DIS_NESTED_PDE_CACHE(v, 1);
	req.pr_tw_set = v;
	req.pr_tw_clr = ptwalk_tw_trial_bits() & ~v;
	if (ptwalk_run(ctx, &req, "pde_cache_nested"))
		KT_PASS(ctx);
}

/*
 * AMD variation 2: every INVLPG or remapper TLB invalidation flushes the
 * whole TLB. TW_CFG is rewritten unchanged so that the walk caches start
 * from a clean slate.
 */
static void
ptwalk_remap_flush_all(ktest_ctx_hdl_t *ctx)
{
	ptwalk_req_t req = { .pr_write = true };

	req.pr_ls4_set = AMD_LS_CFG4_SET_REMAP_FLUSH_ALL_ALWAYS(0, 1);
	if (ptwalk_run(ctx, &req, "remap_flush_all"))
		KT_PASS(ctx);
}

/* AMD variation 3: no page coalescing. */
static void
ptwalk_no_coalesce(ktest_ctx_hdl_t *ctx)
{
	ptwalk_req_t req = { .pr_write = true };
	uint64_t v = 0;

	v = AMD_TW_CFG_SET_DIS_NESTED_COALESCED_LEAF_CACHE(v, 1);
	v = AMD_TW_CFG_SET_DIS_32K_PAGES(v, 1);
	req.pr_tw_set = v;
	req.pr_tw_clr = ptwalk_tw_trial_bits() & ~v;
	if (ptwalk_run(ctx, &req, "no_coalesce"))
		KT_PASS(ctx);
}

/* Clear every bit this suite can set, in both registers. */
static void
ptwalk_reset(ktest_ctx_hdl_t *ctx)
{
	ptwalk_req_t req = { .pr_write = true };

	req.pr_tw_clr = ptwalk_tw_trial_bits();
	req.pr_ls4_clr = AMD_LS_CFG4_SET_REMAP_FLUSH_ALL_ALWAYS(0, 1);
	if (ptwalk_run(ctx, &req, "reset"))
		KT_PASS(ctx);
}

static struct modlmisc oxide_ktest_modlmisc = {
	.misc_modops = &mod_miscops,
	.misc_linkinfo = "Oxide ktest module"
};

static struct modlinkage oxide_ktest_modlinkage = {
	.ml_rev = MODREV_1,
	.ml_linkage = { &oxide_ktest_modlmisc, NULL }
};

int
_init()
{
	int ret;
	ktest_module_hdl_t *km = NULL;
	ktest_suite_hdl_t *ks = NULL;

	VERIFY0(ktest_create_module("oxide", &km));

	VERIFY0(ktest_add_suite(km, "comm_page", &ks));
	VERIFY0(ktest_add_test(ks, "comm_page_vars_test",
	    comm_page_vars_test, 0));

	VERIFY0(ktest_add_suite(km, "pcie", &ks));
	VERIFY0(ktest_add_test(ks, "capture", oxide_pciereg_capture,
	    KTEST_FLAG_NONE));

	VERIFY0(ktest_add_suite(km, "ptwalk", &ks));
	VERIFY0(ktest_add_test(ks, "read", ptwalk_read, KTEST_FLAG_NONE));
	VERIFY0(ktest_add_test(ks, "1a_pde_cache_native",
	    ptwalk_pde_cache_native, KTEST_FLAG_NONE));
	VERIFY0(ktest_add_test(ks, "1b_pde_cache_nested",
	    ptwalk_pde_cache_nested, KTEST_FLAG_NONE));
	VERIFY0(ktest_add_test(ks, "2_remap_flush_all", ptwalk_remap_flush_all,
	    KTEST_FLAG_NONE));
	VERIFY0(ktest_add_test(ks, "3_no_coalesce", ptwalk_no_coalesce,
	    KTEST_FLAG_NONE));
	VERIFY0(ktest_add_test(ks, "reset", ptwalk_reset, KTEST_FLAG_NONE));

	if ((ret = ktest_register_module(km)) != 0) {
		ktest_free_module(km);
		return (ret);
	}

	if ((ret = mod_install(&oxide_ktest_modlinkage)) != 0) {
		ktest_unregister_module("oxide");
		return (ret);
	}

	return (0);
}

int
_fini(void)
{
	ktest_unregister_module("oxide");
	return (mod_remove(&oxide_ktest_modlinkage));
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&oxide_ktest_modlinkage, modinfop));
}
