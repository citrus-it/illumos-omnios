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
 * A device driver that provides user access to the AMD Host System Management
 * Port (HSMP) for debugging purposes.
 */

#include <sys/types.h>
#include <sys/file.h>
#include <sys/errno.h>
#include <sys/open.h>
#include <sys/cred.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/stat.h>
#include <sys/conf.h>
#include <sys/devops.h>
#include <sys/x86_archext.h>
#include <sys/cpuvar.h>
#include <sys/cmn_err.h>
#include <sys/policy.h>
#include <sys/kstat.h>
#include <sys/stdbool.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/smp_impldefs.h>
#include <amdzen_client.h>
#include <sys/amdzen/hsmp.h>

#include "uhsmp.h"

uint_t uhsmp_reply_retry_count = 100;
uint_t uhsmp_reply_retry_delay = 10;	/* ticks */

/*
 * How often, in seconds, the periodic worker refreshes the telemetry kstats by
 * sampling HSMP. A value of zero disables periodic sampling, leaving the kstats
 * with whatever was captured at attach time.
 */
uint_t uhsmp_kstat_period = 60;

/*
 * The set of values exposed through the per-DF "metrics" named kstat. Not every
 * member is present on every part. Availability depends on the HSMP interface
 * version.
 */
typedef enum {
	UHSMP_KS_SOCKET_POWER_MW,
	UHSMP_KS_FCLK_MHZ,
	UHSMP_KS_MCLK_MHZ,
	UHSMP_KS_CCLK_THROTTLE_MHZ,
	UHSMP_KS_C0_RESIDENCY,
	UHSMP_KS_SOCKET_FREQ_MHZ,
	UHSMP_KS_SOCKET_FREQ_SOURCE,
	UHSMP_KS_LAST_SAMPLE,
	UHSMP_KS_SAMPLE_ERRORS,
	/*
	 * The following are decoded from the metric table and are only present
	 * when one is available. The first group is common to all metric table
	 * formats; the rest are specific to one format.
	 */
	UHSMP_KS_MT_VERSION,
	UHSMP_KS_MT_ACC_COUNTER,
	UHSMP_KS_MT_TIMESTAMP,
	UHSMP_KS_MT_SOCKET_POWER_LIMIT,
	UHSMP_KS_MT_MAX_SOCKET_POWER_LIMIT,
	UHSMP_KS_MT_MAX_DRAM_BANDWIDTH,
	UHSMP_KS_MT_PROCHOT_RESIDENCY,
	UHSMP_KS_MT_PPT_RESIDENCY,
	UHSMP_KS_MT_SOCKET_POWER,
	UHSMP_KS_MT_MAX_SOCKET_TEMP,
	UHSMP_KS_MT_CCLK_FREQ_LIMIT,
	UHSMP_KS_MT_FCLK_FREQUENCY,
	UHSMP_KS_MT_UCLK_FREQUENCY,
	UHSMP_KS_MT_SOCKET_C0_RESIDENCY,
	UHSMP_KS_MT_DRAM_BW_UTIL,
	UHSMP_KS_MT_NUM_ACTIVE_CCDS,
	UHSMP_KS_MT_SOCKET_C0_RESIDENCY_ACC,
	UHSMP_KS_NSTAT
} uhsmp_kstat_idx_t;

/*
 * The metric table format, which depends on the processor family. Used both to
 * decode the named subset and to size the raw table mapping.
 */
typedef enum {
	UHSMP_MT_NONE = 0,
	UHSMP_MT_GENOA,
	UHSMP_MT_TURIN
} uhsmp_mt_fmt_t;

/*
 * Per-DF (per-socket) state. Each DF has its own HSMP mailbox and its own set
 * of kstats.
 */
typedef struct {
	uint_t ud_dfno;
	kstat_t *ud_ksp;
	kstat_named_t *ud_named[UHSMP_KS_NSTAT];
	/*
	 * Metric table state. ud_mt_fmt is UHSMP_MT_NONE when this part has no
	 * metric table or we were unable to map it.
	 */
	uhsmp_mt_fmt_t ud_mt_fmt;
	uint32_t ud_mt_version;
	caddr_t ud_mt_va;
	paddr_t ud_mt_pa;
	size_t ud_mt_len;
	kstat_t *ud_mt_ksp;
} uhsmp_df_t;

typedef struct {
	dev_info_t *uhsmp_dip;
	x86_processor_family_t uhsmp_fam;
	uint_t uhsmp_ndfs;
	uint_t uhsmp_ifver;
	uint_t uhsmp_maxfn;
	kmutex_t uhsmp_lock;
	kmutex_t uhsmp_kstat_lock;
	uhsmp_df_t *uhsmp_dfs;
	ddi_periodic_t uhsmp_periodic;
} uhsmp_t;

/*
 * This provides a mapping between the interface version, as reported by the
 * HSMP "GetInterfaceVersion" function, and the number of available functions.
 * The versions start at 1 and AMD documentation does not mention version 6
 * which was presumably never released. If we encounter it we will log a
 * warning and fail to attach.
 */
static const uint_t uhsmp_ifver_maxfn[] = {
	[1] = HSMP_IFVER1_FUNCS,
	[2] = HSMP_IFVER2_FUNCS,
	[3] = HSMP_IFVER3_FUNCS,
	[4] = HSMP_IFVER4_FUNCS,
	[5] = HSMP_IFVER5_FUNCS,
	[7] = HSMP_IFVER7_FUNCS
};

static uhsmp_t uhsmp_data;

static int
uhsmp_open(dev_t *devp, int flags, int otype, cred_t *credp)
{
	minor_t m;
	uhsmp_t *uhsmp = &uhsmp_data;

	if (crgetzoneid(credp) != GLOBAL_ZONEID ||
	    secpolicy_hwmanip(credp) != 0) {
		return (EPERM);
	}

	if ((flags & (FEXCL | FNDELAY | FNONBLOCK)) != 0)
		return (EINVAL);

	if (otype != OTYP_CHR)
		return (EINVAL);

	m = getminor(*devp);
	if (m >= uhsmp->uhsmp_ndfs)
		return (ENXIO);

	return (0);
}

static int
uhsmp_cmd(uhsmp_t *uhsmp, uint_t dfno, uhsmp_cmd_t *cmd)
{
	const smn_reg_t id = SMN_HSMP_MSGID(uhsmp->uhsmp_fam);
	const smn_reg_t resp = SMN_HSMP_RESP;
	const smn_reg_t args[] = {
		SMN_HSMP_ARG(0),
		SMN_HSMP_ARG(1),
		SMN_HSMP_ARG(2),
		SMN_HSMP_ARG(3),
		SMN_HSMP_ARG(4),
		SMN_HSMP_ARG(5),
		SMN_HSMP_ARG(6),
		SMN_HSMP_ARG(7)
	};
	int ret = 0;

	cmd->uc_response = 0;
	mutex_enter(&uhsmp->uhsmp_lock);
	if ((ret = amdzen_c_smn_write(dfno, resp, cmd->uc_response)) != 0)
		goto out;
	for (size_t i = 0; i < ARRAY_SIZE(args); i++) {
		ret = amdzen_c_smn_write(dfno, args[i], cmd->uc_args[i]);
		if (ret != 0)
			goto out;
	}
	if ((ret = amdzen_c_smn_write(dfno, id, cmd->uc_id)) != 0)
		goto out;
	for (uint_t i = 0; i < uhsmp_reply_retry_count; i++) {
		ret = amdzen_c_smn_read(dfno, resp, &cmd->uc_response);
		if (ret != 0)
			break;
		if (cmd->uc_response != 0)
			break;
		delay(uhsmp_reply_retry_delay);
	}
	if (cmd->uc_response == 0) {
		ret = ETIMEDOUT;
		goto out;
	}
	for (size_t i = 0; i < ARRAY_SIZE(args); i++) {
		ret = amdzen_c_smn_read(dfno, args[i], &cmd->uc_args[i]);
		if (ret != 0)
			goto out;
	}

out:
	mutex_exit(&uhsmp->uhsmp_lock);
	return (ret);
}

typedef struct {
	const char *uki_name;
	uchar_t uki_type;
} uhsmp_kstat_info_t;

static const uhsmp_kstat_info_t uhsmp_kstat_info[UHSMP_KS_NSTAT] = {
	[UHSMP_KS_SOCKET_POWER_MW] =
	    { "socket_power_mw", KSTAT_DATA_UINT32 },
	[UHSMP_KS_FCLK_MHZ] =
	    { "fclk_mhz", KSTAT_DATA_UINT32 },
	[UHSMP_KS_MCLK_MHZ] =
	    { "mclk_mhz", KSTAT_DATA_UINT32 },
	[UHSMP_KS_CCLK_THROTTLE_MHZ] =
	    { "cclk_throttle_limit_mhz", KSTAT_DATA_UINT32 },
	[UHSMP_KS_C0_RESIDENCY] =
	    { "c0_residency_pct", KSTAT_DATA_UINT32 },
	[UHSMP_KS_SOCKET_FREQ_MHZ] =
	    { "socket_freq_mhz", KSTAT_DATA_UINT32 },
	[UHSMP_KS_SOCKET_FREQ_SOURCE] =
	    { "socket_freq_source", KSTAT_DATA_UINT32 },
	[UHSMP_KS_LAST_SAMPLE] =
	    { "last_sample", KSTAT_DATA_UINT64 },
	[UHSMP_KS_SAMPLE_ERRORS] =
	    { "sample_errors", KSTAT_DATA_UINT64 },
	[UHSMP_KS_MT_VERSION] =
	    { "metric_table_version", KSTAT_DATA_UINT32 },
	[UHSMP_KS_MT_ACC_COUNTER] =
	    { "mt_accumulation_counter", KSTAT_DATA_UINT32 },
	[UHSMP_KS_MT_TIMESTAMP] =
	    { "mt_timestamp", KSTAT_DATA_UINT64 },
	[UHSMP_KS_MT_SOCKET_POWER_LIMIT] =
	    { "mt_socket_power_limit", KSTAT_DATA_UINT32 },
	[UHSMP_KS_MT_MAX_SOCKET_POWER_LIMIT] =
	    { "mt_max_socket_power_limit", KSTAT_DATA_UINT32 },
	[UHSMP_KS_MT_MAX_DRAM_BANDWIDTH] =
	    { "mt_max_dram_bandwidth", KSTAT_DATA_UINT32 },
	[UHSMP_KS_MT_PROCHOT_RESIDENCY] =
	    { "mt_prochot_residency_acc", KSTAT_DATA_UINT32 },
	[UHSMP_KS_MT_PPT_RESIDENCY] =
	    { "mt_ppt_residency_acc", KSTAT_DATA_UINT32 },
	[UHSMP_KS_MT_SOCKET_POWER] =
	    { "mt_socket_power", KSTAT_DATA_UINT32 },
	[UHSMP_KS_MT_MAX_SOCKET_TEMP] =
	    { "mt_max_socket_temperature", KSTAT_DATA_UINT32 },
	[UHSMP_KS_MT_CCLK_FREQ_LIMIT] =
	    { "mt_cclk_frequency_limit", KSTAT_DATA_UINT32 },
	[UHSMP_KS_MT_FCLK_FREQUENCY] =
	    { "mt_fclk_frequency", KSTAT_DATA_UINT32 },
	[UHSMP_KS_MT_UCLK_FREQUENCY] =
	    { "mt_uclk_frequency", KSTAT_DATA_UINT32 },
	[UHSMP_KS_MT_SOCKET_C0_RESIDENCY] =
	    { "mt_socket_c0_residency", KSTAT_DATA_UINT32 },
	[UHSMP_KS_MT_DRAM_BW_UTIL] =
	    { "mt_dram_bandwidth_utilization", KSTAT_DATA_UINT32 },
	[UHSMP_KS_MT_NUM_ACTIVE_CCDS] =
	    { "mt_num_active_ccds", KSTAT_DATA_UINT32 },
	[UHSMP_KS_MT_SOCKET_C0_RESIDENCY_ACC] =
	    { "mt_socket_c0_residency_acc", KSTAT_DATA_UINT64 },
};

static void
uhsmp_kn_set32(uhsmp_df_t *df, uhsmp_kstat_idx_t idx, uint32_t val)
{
	ASSERT(MUTEX_HELD(&uhsmp_data.uhsmp_kstat_lock));
	if (df->ud_named[idx] != NULL)
		df->ud_named[idx]->value.ui32 = val;
}

static void
uhsmp_kn_set64(uhsmp_df_t *df, uhsmp_kstat_idx_t idx, uint64_t val)
{
	ASSERT(MUTEX_HELD(&uhsmp_data.uhsmp_kstat_lock));
	if (df->ud_named[idx] != NULL)
		df->ud_named[idx]->value.ui64 = val;
}

static void
uhsmp_kn_add64(uhsmp_df_t *df, uhsmp_kstat_idx_t idx, uint64_t val)
{
	ASSERT(MUTEX_HELD(&uhsmp_data.uhsmp_kstat_lock));
	if (df->ud_named[idx] != NULL)
		df->ud_named[idx]->value.ui64 += val;
}

/*
 * Determine whether a given kstat member is available on this part. The
 * discrete HSMP functions are gated on the interface version in the same way
 * as the generic ioctl, while the housekeeping members are always present.
 */
static bool
uhsmp_kstat_present(uhsmp_t *uhsmp, uhsmp_df_t *df, uhsmp_kstat_idx_t idx)
{
	switch (idx) {
	case UHSMP_KS_SOCKET_POWER_MW:
		return (uhsmp->uhsmp_maxfn >= HSMP_CMD_GETSOCKETPOWER);
	case UHSMP_KS_FCLK_MHZ:
	case UHSMP_KS_MCLK_MHZ:
		return (uhsmp->uhsmp_maxfn >= HSMP_CMD_GETFCLKMCLK);
	case UHSMP_KS_CCLK_THROTTLE_MHZ:
		return (uhsmp->uhsmp_maxfn >= HSMP_CMD_GETCCLKTHROTTLELIMIT);
	case UHSMP_KS_C0_RESIDENCY:
		return (uhsmp->uhsmp_maxfn >= HSMP_CMD_GETC0PERCENT);
	case UHSMP_KS_SOCKET_FREQ_MHZ:
	case UHSMP_KS_SOCKET_FREQ_SOURCE:
		return (uhsmp->uhsmp_maxfn >= HSMP_CMD_GETSOCKETFREQLIMIT);
	case UHSMP_KS_LAST_SAMPLE:
	case UHSMP_KS_SAMPLE_ERRORS:
		return (true);
	/*
	 * The metric table members. The first group is common to all formats;
	 * the others are gated on the specific format that is in use.
	 */
	case UHSMP_KS_MT_VERSION:
	case UHSMP_KS_MT_ACC_COUNTER:
	case UHSMP_KS_MT_TIMESTAMP:
	case UHSMP_KS_MT_SOCKET_POWER_LIMIT:
	case UHSMP_KS_MT_MAX_SOCKET_POWER_LIMIT:
	case UHSMP_KS_MT_MAX_DRAM_BANDWIDTH:
	case UHSMP_KS_MT_PROCHOT_RESIDENCY:
	case UHSMP_KS_MT_PPT_RESIDENCY:
		return (df->ud_mt_fmt != UHSMP_MT_NONE);
	case UHSMP_KS_MT_SOCKET_POWER:
	case UHSMP_KS_MT_MAX_SOCKET_TEMP:
	case UHSMP_KS_MT_CCLK_FREQ_LIMIT:
	case UHSMP_KS_MT_FCLK_FREQUENCY:
	case UHSMP_KS_MT_UCLK_FREQUENCY:
	case UHSMP_KS_MT_SOCKET_C0_RESIDENCY:
	case UHSMP_KS_MT_DRAM_BW_UTIL:
		return (df->ud_mt_fmt == UHSMP_MT_GENOA);
	case UHSMP_KS_MT_NUM_ACTIVE_CCDS:
	case UHSMP_KS_MT_SOCKET_C0_RESIDENCY_ACC:
		return (df->ud_mt_fmt == UHSMP_MT_TURIN);
	default:
		return (false);
	}
}

static bool
uhsmp_create_kstats(uhsmp_t *uhsmp, uhsmp_df_t *df)
{
	kstat_t *ksp;
	kstat_named_t *knp;
	uint_t n = 0, j = 0;

	for (uhsmp_kstat_idx_t i = 0; i < UHSMP_KS_NSTAT; i++) {
		if (uhsmp_kstat_present(uhsmp, df, i))
			n++;
	}

	ksp = kstat_create("uhsmp", df->ud_dfno, "metrics", "misc",
	    KSTAT_TYPE_NAMED, n, 0);
	if (ksp == NULL) {
		dev_err(uhsmp->uhsmp_dip, CE_WARN,
		    "!failed to create metrics kstat for DF %u", df->ud_dfno);
		return (false);
	}

	knp = ksp->ks_data;
	for (uhsmp_kstat_idx_t i = 0; i < UHSMP_KS_NSTAT; i++) {
		if (!uhsmp_kstat_present(uhsmp, df, i))
			continue;
		kstat_named_init(&knp[j], uhsmp_kstat_info[i].uki_name,
		    uhsmp_kstat_info[i].uki_type);
		df->ud_named[i] = &knp[j];
		j++;
	}

	ksp->ks_lock = &uhsmp->uhsmp_kstat_lock;
	ksp->ks_private = df;
	kstat_install(ksp);
	df->ud_ksp = ksp;

	return (true);
}

/*
 * Issue a single argument-less HSMP "get" function and report whether it
 * completed successfully. On success the results are left in cmd->uc_args.
 */
static bool
uhsmp_sample_fn(uhsmp_t *uhsmp, uint_t dfno, uint32_t fn, uhsmp_cmd_t *cmd)
{
	bzero(cmd, sizeof (*cmd));
	cmd->uc_id = fn;
	if (uhsmp_cmd(uhsmp, dfno, cmd) != 0)
		return (false);
	return (cmd->uc_response == HSMP_RESPONSE_OK);
}

/*
 * Map this DF's metric table, if the part has one. The SMU maintains the table
 * at a fixed physical address that we obtain via HSMP_CMD_GETMETRICTABLEADDR
 * and we keep it mapped for the lifetime of the driver. The mapping is strictly
 * ordered and uncached so that each read sees the values that the SMU last
 * wrote. Failure is not fatal; we simply do not expose the table for this DF.
 */
static void
uhsmp_map_metric_table(uhsmp_t *uhsmp, uhsmp_df_t *df)
{
	uhsmp_cmd_t cmd;
	uhsmp_mt_fmt_t fmt;
	size_t len;
	uint64_t pa;

	switch (uhsmp->uhsmp_fam) {
	case X86_PF_AMD_GENOA:
	case X86_PF_AMD_BERGAMO:
		fmt = UHSMP_MT_GENOA;
		len = sizeof (hsmp_metric_table_t);
		break;
	case X86_PF_AMD_TURIN:
	case X86_PF_AMD_DENSE_TURIN:
		fmt = UHSMP_MT_TURIN;
		len = sizeof (hsmp_metric_table_f1a_m50_t);
		break;
	default:
		return;
	}

	if (uhsmp->uhsmp_maxfn < HSMP_CMD_GETMETRICTABLEADDR)
		return;

	if (uhsmp_sample_fn(uhsmp, df->ud_dfno,
	    HSMP_CMD_GETMETRICTABLEVER, &cmd)) {
		df->ud_mt_version = cmd.uc_args[0];
	}

	if (!uhsmp_sample_fn(uhsmp, df->ud_dfno,
	    HSMP_CMD_GETMETRICTABLEADDR, &cmd)) {
		dev_err(uhsmp->uhsmp_dip, CE_WARN,
		    "!failed to read metric table address for DF %u",
		    df->ud_dfno);
		return;
	}

	pa = (uint64_t)cmd.uc_args[0] | ((uint64_t)cmd.uc_args[1] << 32);
	if (pa == 0)
		return;

	df->ud_mt_va = psm_map_phys((paddr_t)pa, len, PROT_READ);
	if (df->ud_mt_va == NULL) {
		dev_err(uhsmp->uhsmp_dip, CE_WARN,
		    "!failed to map metric table for DF %u at 0x%llx",
		    df->ud_dfno, (u_longlong_t)pa);
		return;
	}

	df->ud_mt_pa = (paddr_t)pa;
	df->ud_mt_len = len;
	df->ud_mt_fmt = fmt;
}

static bool
uhsmp_create_mt_kstat(uhsmp_t *uhsmp, uhsmp_df_t *df)
{
	kstat_t *ksp;

	if (df->ud_mt_fmt == UHSMP_MT_NONE)
		return (true);

	ksp = kstat_create("uhsmp", df->ud_dfno, "metric_table", "misc",
	    KSTAT_TYPE_RAW, (uint_t)df->ud_mt_len, 0);
	if (ksp == NULL) {
		dev_err(uhsmp->uhsmp_dip, CE_WARN, "!failed to create metric "
		    "table kstat for DF %u", df->ud_dfno);
		return (false);
	}

	ksp->ks_lock = &uhsmp->uhsmp_kstat_lock;
	ksp->ks_private = df;
	kstat_install(ksp);
	df->ud_mt_ksp = ksp;

	return (true);
}

/*
 * Decode the socket-wide subset of the metric table into named kstat members.
 * The raw table itself is exposed in full through a separate KSTAT_TYPE_RAW
 * kstat; this is just a convenience view of the most useful values. The caller
 * has already refreshed ud_mt_ksp->ks_data from the mapped table.
 */
static void
uhsmp_decode_mt(uhsmp_df_t *df)
{
	ASSERT(MUTEX_HELD(&uhsmp_data.uhsmp_kstat_lock));

	uhsmp_kn_set32(df, UHSMP_KS_MT_VERSION, df->ud_mt_version);

	switch (df->ud_mt_fmt) {
	case UHSMP_MT_GENOA: {
		const hsmp_metric_table_t *t = df->ud_mt_ksp->ks_data;

		uhsmp_kn_set32(df, UHSMP_KS_MT_ACC_COUNTER,
		    t->hmt_accumulation_counter);
		uhsmp_kn_set64(df, UHSMP_KS_MT_TIMESTAMP, t->hmt_timestamp);
		uhsmp_kn_set32(df, UHSMP_KS_MT_SOCKET_POWER_LIMIT,
		    t->hmt_socket_power_limit);
		uhsmp_kn_set32(df, UHSMP_KS_MT_MAX_SOCKET_POWER_LIMIT,
		    t->hmt_max_socket_power_limit);
		uhsmp_kn_set32(df, UHSMP_KS_MT_MAX_DRAM_BANDWIDTH,
		    t->hmt_max_dram_bandwidth);
		uhsmp_kn_set32(df, UHSMP_KS_MT_PROCHOT_RESIDENCY,
		    t->hmt_prochot_residency_acc);
		uhsmp_kn_set32(df, UHSMP_KS_MT_PPT_RESIDENCY,
		    t->hmt_ppt_residency_acc);
		uhsmp_kn_set32(df, UHSMP_KS_MT_SOCKET_POWER,
		    t->hmt_socket_power);
		uhsmp_kn_set32(df, UHSMP_KS_MT_MAX_SOCKET_TEMP,
		    t->hmt_max_socket_temperature);
		uhsmp_kn_set32(df, UHSMP_KS_MT_CCLK_FREQ_LIMIT,
		    t->hmt_cclk_frequency_limit);
		uhsmp_kn_set32(df, UHSMP_KS_MT_FCLK_FREQUENCY,
		    t->hmt_fclk_frequency);
		uhsmp_kn_set32(df, UHSMP_KS_MT_UCLK_FREQUENCY,
		    t->hmt_uclk_frequency);
		uhsmp_kn_set32(df, UHSMP_KS_MT_SOCKET_C0_RESIDENCY,
		    t->hmt_socket_c0_residency);
		uhsmp_kn_set32(df, UHSMP_KS_MT_DRAM_BW_UTIL,
		    t->hmt_dram_bandwidth_utilization);
		break;
	}
	case UHSMP_MT_TURIN: {
		const hsmp_metric_table_f1a_m50_t *t = df->ud_mt_ksp->ks_data;
		const hsmp_metric_table_f1a_m50_iod_t *iod = &t->hmt_iod;

		uhsmp_kn_set32(df, UHSMP_KS_MT_ACC_COUNTER,
		    iod->hmti_accumulation_counter);
		uhsmp_kn_set64(df, UHSMP_KS_MT_TIMESTAMP, iod->hmti_timestamp);
		uhsmp_kn_set32(df, UHSMP_KS_MT_SOCKET_POWER_LIMIT,
		    iod->hmti_socket_power_limit);
		uhsmp_kn_set32(df, UHSMP_KS_MT_MAX_SOCKET_POWER_LIMIT,
		    iod->hmti_max_socket_power_limit);
		uhsmp_kn_set32(df, UHSMP_KS_MT_MAX_DRAM_BANDWIDTH,
		    iod->hmti_max_dram_bandwidth);
		uhsmp_kn_set32(df, UHSMP_KS_MT_PROCHOT_RESIDENCY,
		    iod->hmti_prochot_residency_acc);
		uhsmp_kn_set32(df, UHSMP_KS_MT_PPT_RESIDENCY,
		    iod->hmti_ppt_residency_acc);
		uhsmp_kn_set32(df, UHSMP_KS_MT_NUM_ACTIVE_CCDS,
		    iod->hmti_num_active_ccds);
		uhsmp_kn_set64(df, UHSMP_KS_MT_SOCKET_C0_RESIDENCY_ACC,
		    iod->hmti_socket_c0_residency_acc);
		break;
	}
	default:
		break;
	}
}

/*
 * Sample all of the supported telemetry for a single DF and publish it into
 * that DF's kstats. The mailbox transactions are performed without the kstat
 * lock held (uhsmp_cmd() takes uhsmp_lock for each); only the brief publish at
 * the end is done under the kstat lock. A value whose command fails is left at
 * its previous reading rather than being cleared.
 */
static void
uhsmp_sample_df(uhsmp_t *uhsmp, uhsmp_df_t *df)
{
	const uint_t dfno = df->ud_dfno;
	uhsmp_cmd_t cmd;
	uint32_t power = 0, fclk = 0, mclk = 0, cclk = 0, c0 = 0;
	uint32_t freq_mhz = 0, freq_src = 0;
	bool hpower = false, hfclk = false, hcclk = false, hc0 = false;
	bool hfreq = false, refreshed = false, mt_ok = false;
	uint64_t errors = 0;

	if (uhsmp->uhsmp_maxfn >= HSMP_CMD_GETSOCKETPOWER) {
		if (uhsmp_sample_fn(uhsmp, dfno, HSMP_CMD_GETSOCKETPOWER,
		    &cmd)) {
			power = cmd.uc_args[0];
			hpower = refreshed = true;
		} else {
			errors++;
		}
	}

	if (uhsmp->uhsmp_maxfn >= HSMP_CMD_GETFCLKMCLK) {
		if (uhsmp_sample_fn(uhsmp, dfno, HSMP_CMD_GETFCLKMCLK, &cmd)) {
			fclk = cmd.uc_args[0];
			mclk = cmd.uc_args[1];
			hfclk = refreshed = true;
		} else {
			errors++;
		}
	}

	if (uhsmp->uhsmp_maxfn >= HSMP_CMD_GETCCLKTHROTTLELIMIT) {
		if (uhsmp_sample_fn(uhsmp, dfno, HSMP_CMD_GETCCLKTHROTTLELIMIT,
		    &cmd)) {
			cclk = cmd.uc_args[0];
			hcclk = refreshed = true;
		} else {
			errors++;
		}
	}

	if (uhsmp->uhsmp_maxfn >= HSMP_CMD_GETC0PERCENT) {
		if (uhsmp_sample_fn(uhsmp, dfno, HSMP_CMD_GETC0PERCENT, &cmd)) {
			c0 = cmd.uc_args[0];
			hc0 = refreshed = true;
		} else {
			errors++;
		}
	}

	if (uhsmp->uhsmp_maxfn >= HSMP_CMD_GETSOCKETFREQLIMIT) {
		if (uhsmp_sample_fn(uhsmp, dfno, HSMP_CMD_GETSOCKETFREQLIMIT,
		    &cmd)) {
			freq_mhz = HSMP_SOCKET_FREQ_GET_MHZ(cmd.uc_args[0]);
			freq_src = HSMP_SOCKET_FREQ_GET_SOURCE(cmd.uc_args[0]);
			hfreq = refreshed = true;
		} else {
			errors++;
		}
	}

	/*
	 * Refresh the metric table. The SMU writes it into the mapped DRAM in
	 * response to this command; we copy it out under the kstat lock below.
	 */
	if (df->ud_mt_fmt != UHSMP_MT_NONE) {
		if (uhsmp_sample_fn(uhsmp, dfno, HSMP_CMD_GETMETRICTABLE, &cmd))
			mt_ok = refreshed = true;
		else
			errors++;
	}

	/*
	 * Publish under the kstat lock. The lock is taken even when nothing was
	 * refreshed so that sample_errors is still updated, which is what makes
	 * a persistently failing mailbox visible.
	 */
	mutex_enter(&uhsmp->uhsmp_kstat_lock);
	if (hpower)
		uhsmp_kn_set32(df, UHSMP_KS_SOCKET_POWER_MW, power);
	if (hfclk) {
		uhsmp_kn_set32(df, UHSMP_KS_FCLK_MHZ, fclk);
		uhsmp_kn_set32(df, UHSMP_KS_MCLK_MHZ, mclk);
	}
	if (hcclk)
		uhsmp_kn_set32(df, UHSMP_KS_CCLK_THROTTLE_MHZ, cclk);
	if (hc0)
		uhsmp_kn_set32(df, UHSMP_KS_C0_RESIDENCY, c0);
	if (hfreq) {
		uhsmp_kn_set32(df, UHSMP_KS_SOCKET_FREQ_MHZ, freq_mhz);
		uhsmp_kn_set32(df, UHSMP_KS_SOCKET_FREQ_SOURCE, freq_src);
	}
	if (mt_ok) {
		bcopy(df->ud_mt_va, df->ud_mt_ksp->ks_data, df->ud_mt_len);
		uhsmp_decode_mt(df);
	}
	uhsmp_kn_add64(df, UHSMP_KS_SAMPLE_ERRORS, errors);
	/*
	 * Only advance the sample time when something was read this pass. If
	 * every command failed, refreshed is false and last_sample is left
	 * alone so that consumers can still tell the data has gone stale.
	 */
	if (refreshed) {
		uhsmp_kn_set64(df, UHSMP_KS_LAST_SAMPLE,
		    (uint64_t)gethrtime());
	}
	mutex_exit(&uhsmp->uhsmp_kstat_lock);
}

/*
 * The periodic worker entry point. It runs in a context where blocking is
 * permitted (DDI_IPL_0) which is required as the HSMP mailbox may stall.
 */
static void
uhsmp_sample(void *arg)
{
	uhsmp_t *uhsmp = arg;

	for (uint_t i = 0; i < uhsmp->uhsmp_ndfs; i++)
		uhsmp_sample_df(uhsmp, &uhsmp->uhsmp_dfs[i]);
}

static int
uhsmp_ioctl(dev_t dev, int cmd, intptr_t arg, int mode, cred_t *credp,
    int *rvalp)
{
	uhsmp_t *uhsmp = &uhsmp_data;
	uhsmp_cmd_t ucmd;
	uint_t dfno;
	int ret;

	if (cmd != UHSMP_GENERIC_COMMAND)
		return (ENOTTY);

	/* The only currently supported command requires read/write */
	if ((mode & (FREAD|FWRITE)) != (FREAD|FWRITE))
		return (EINVAL);

	dfno = getminor(dev);
	if (dfno >= uhsmp->uhsmp_ndfs)
		return (ENXIO);

	if (crgetzoneid(credp) != GLOBAL_ZONEID ||
	    secpolicy_hwmanip(credp) != 0) {
		return (EPERM);
	}

	if (ddi_copyin((void *)arg, &ucmd, sizeof (ucmd), mode & FKIOCTL) != 0)
		return (EFAULT);

	if (ucmd.uc_id > uhsmp->uhsmp_maxfn)
		return (EINVAL);

	ret = uhsmp_cmd(uhsmp, dfno, &ucmd);

	if (ret == 0 && ddi_copyout(&ucmd, (void *)arg, sizeof (ucmd),
	    mode & FKIOCTL) != 0) {
		ret = EFAULT;
	}

	return (ret);
}

static int
uhsmp_close(dev_t dev, int flag, int otyp, cred_t *credp)
{
	return (0);
}

static void
uhsmp_cleanup(uhsmp_t *uhsmp)
{
	/*
	 * Cancel the periodic worker first so that nothing is sampling while we
	 * tear down the kstats. This is done without holding any lock that the
	 * worker itself acquires and blocks until any in-flight callback has
	 * finished.
	 */
	if (uhsmp->uhsmp_periodic != NULL) {
		ddi_periodic_delete(uhsmp->uhsmp_periodic);
		uhsmp->uhsmp_periodic = NULL;
	}

	if (uhsmp->uhsmp_dfs != NULL) {
		for (uint_t i = 0; i < uhsmp->uhsmp_ndfs; i++) {
			uhsmp_df_t *df = &uhsmp->uhsmp_dfs[i];

			if (df->ud_ksp != NULL)
				kstat_delete(df->ud_ksp);
			if (df->ud_mt_ksp != NULL)
				kstat_delete(df->ud_mt_ksp);
			if (df->ud_mt_va != NULL)
				psm_unmap_phys(df->ud_mt_va, df->ud_mt_len);
		}
		kmem_free(uhsmp->uhsmp_dfs,
		    sizeof (uhsmp_df_t) * uhsmp->uhsmp_ndfs);
		uhsmp->uhsmp_dfs = NULL;
	}

	ddi_remove_minor_node(uhsmp->uhsmp_dip, NULL);
	uhsmp->uhsmp_ndfs = 0;
	uhsmp->uhsmp_dip = NULL;
	mutex_destroy(&uhsmp->uhsmp_lock);
	mutex_destroy(&uhsmp->uhsmp_kstat_lock);
}

static int
uhsmp_attach(dev_info_t *dip, ddi_attach_cmd_t cmd)
{
	uhsmp_t *uhsmp = &uhsmp_data;
	int ret;

	if (cmd == DDI_RESUME)
		return (DDI_SUCCESS);
	else if (cmd != DDI_ATTACH)
		return (DDI_FAILURE);

	if (uhsmp->uhsmp_dip != NULL) {
		dev_err(dip, CE_WARN,
		    "!uhsmp is already attached to a dev_info_t: %p",
		    uhsmp->uhsmp_dip);
		return (DDI_FAILURE);
	}

	uhsmp->uhsmp_fam = chiprev_family(cpuid_getchiprev(CPU));

	switch (uarchrev_uarch(cpuid_getuarchrev(CPU))) {
	case X86_UARCH_AMD_ZEN3:
	case X86_UARCH_AMD_ZEN4:
	case X86_UARCH_AMD_ZEN5:
		break;
	default:
		return (DDI_FAILURE);
	}

	uhsmp->uhsmp_dip = dip;
	mutex_init(&uhsmp->uhsmp_lock, NULL, MUTEX_DRIVER, NULL);
	mutex_init(&uhsmp->uhsmp_kstat_lock, NULL, MUTEX_DRIVER, NULL);

	/*
	 * Determine if HSMP is available by sending a test message and
	 * checking that it completes successfully in a reasonable amount of
	 * time. Working HSMP depends on some SMU setup having been done.
	 */
#define	HSMP_TESTVAL	0x1234567
	uhsmp_cmd_t testcmd = {
		.uc_id = HSMP_CMD_TESTMESSAGE,
		.uc_args[0] = HSMP_TESTVAL
	};
	if ((ret = uhsmp_cmd(uhsmp, 0, &testcmd)) != 0) {
		dev_err(dip, CE_CONT, "?UHSMP test error %d\n", ret);
		goto err;
	}
	if (testcmd.uc_response != HSMP_RESPONSE_OK ||
	    testcmd.uc_args[0] != HSMP_TESTVAL + 1) {
		dev_err(dip, CE_CONT, "?UHSMP test failed. "
		    "Response 0x%x, returned value 0x%x (expected 0x%x)\n",
		    testcmd.uc_response, testcmd.uc_args[0],
		    HSMP_TESTVAL + 1);
		goto err;
	}

	/* Determine the number of available HSMP functions */
	uhsmp_cmd_t vercmd = {
		.uc_id = HSMP_CMD_GETIFVERSION
	};
	if ((ret = uhsmp_cmd(uhsmp, 0, &vercmd)) != 0) {
		dev_err(dip, CE_CONT, "?UHSMP version command error %d", ret);
		goto err;
	}
	if (testcmd.uc_response != HSMP_RESPONSE_OK) {
		dev_err(dip, CE_CONT,
		    "?UHSMP version command failed. Response 0x%x",
		    testcmd.uc_response);
		goto err;
	}

	uhsmp->uhsmp_ifver = vercmd.uc_args[0];
	uhsmp->uhsmp_maxfn = 0;
	if (uhsmp->uhsmp_ifver < ARRAY_SIZE(uhsmp_ifver_maxfn)) {
		uhsmp->uhsmp_maxfn =
		    uhsmp_ifver_maxfn[uhsmp->uhsmp_ifver];
	}
	if (uhsmp->uhsmp_maxfn == 0) {
		dev_err(dip, CE_WARN,
		    "Unsupported UHSMP interface version 0x%x",
		    uhsmp->uhsmp_ifver);
		goto err;
	}

	uhsmp->uhsmp_ndfs = amdzen_c_df_count();
	for (uint_t i = 0; i < uhsmp->uhsmp_ndfs; i++) {
		char buf[32];

		(void) snprintf(buf, sizeof (buf), "uhsmp.%u", i);
		if (ddi_create_minor_node(dip, buf, S_IFCHR, i, DDI_PSEUDO,
		    0) != DDI_SUCCESS) {
			dev_err(dip, CE_WARN, "!failed to create minor %s",
			    buf);
			goto err;
		}
	}

	uhsmp->uhsmp_dfs = kmem_zalloc(sizeof (uhsmp_df_t) * uhsmp->uhsmp_ndfs,
	    KM_SLEEP);
	for (uint_t i = 0; i < uhsmp->uhsmp_ndfs; i++) {
		uhsmp_df_t *df = &uhsmp->uhsmp_dfs[i];

		df->ud_dfno = i;
		uhsmp_map_metric_table(uhsmp, df);
		if (!uhsmp_create_kstats(uhsmp, df))
			goto err;
		if (!uhsmp_create_mt_kstat(uhsmp, df))
			goto err;
	}

	/*
	 * Populate the kstats once now so that they hold valid data before the
	 * first periodic sample fires, then arm the periodic worker.
	 */
	uhsmp_sample(uhsmp);
	if (uhsmp_kstat_period > 0) {
		uhsmp->uhsmp_periodic = ddi_periodic_add(uhsmp_sample, uhsmp,
		    (hrtime_t)uhsmp_kstat_period * NANOSEC, DDI_IPL_0);
	}

	return (DDI_SUCCESS);

err:
	uhsmp_cleanup(uhsmp);
	return (DDI_FAILURE);
}

static int
uhsmp_getinfo(dev_info_t *dip, ddi_info_cmd_t cmd, void *arg, void **resultp)
{
	uhsmp_t *uhsmp = &uhsmp_data;
	minor_t m;

	switch (cmd) {
	case DDI_INFO_DEVT2DEVINFO:
		m = getminor((dev_t)arg);
		if (m >= uhsmp->uhsmp_ndfs)
			return (DDI_FAILURE);
		*resultp = (void *)uhsmp->uhsmp_dip;
		break;
	case DDI_INFO_DEVT2INSTANCE:
		m = getminor((dev_t)arg);
		if (m >= uhsmp->uhsmp_ndfs)
			return (DDI_FAILURE);
		*resultp =
		    (void *)(uintptr_t)ddi_get_instance(uhsmp->uhsmp_dip);
		break;
	default:
		return (DDI_FAILURE);
	}
	return (DDI_SUCCESS);
}

static int
uhsmp_detach(dev_info_t *dip, ddi_detach_cmd_t cmd)
{
	uhsmp_t *uhsmp = &uhsmp_data;

	if (cmd == DDI_SUSPEND)
		return (DDI_SUCCESS);
	else if (cmd != DDI_DETACH)
		return (DDI_FAILURE);

	if (uhsmp->uhsmp_dip != dip) {
		dev_err(dip, CE_WARN,
		    "!asked to detach uhsmp, but dip doesn't match");
		return (DDI_FAILURE);
	}

	uhsmp_cleanup(uhsmp);
	return (DDI_SUCCESS);
}

static struct cb_ops uhsmp_cb_ops = {
	.cb_open = uhsmp_open,
	.cb_close = uhsmp_close,
	.cb_strategy = nodev,
	.cb_print = nodev,
	.cb_dump = nodev,
	.cb_read = nodev,
	.cb_write = nodev,
	.cb_ioctl = uhsmp_ioctl,
	.cb_devmap = nodev,
	.cb_mmap = nodev,
	.cb_segmap = nodev,
	.cb_chpoll = nochpoll,
	.cb_prop_op = ddi_prop_op,
	.cb_flag = D_MP,
	.cb_rev = CB_REV,
	.cb_aread = nodev,
	.cb_awrite = nodev
};

static struct dev_ops uhsmp_dev_ops = {
	.devo_rev = DEVO_REV,
	.devo_refcnt = 0,
	.devo_getinfo = uhsmp_getinfo,
	.devo_identify = nulldev,
	.devo_probe = nulldev,
	.devo_attach = uhsmp_attach,
	.devo_detach = uhsmp_detach,
	.devo_reset = nodev,
	.devo_quiesce = ddi_quiesce_not_needed,
	.devo_cb_ops = &uhsmp_cb_ops
};

static struct modldrv uhsmp_modldrv = {
	.drv_modops = &mod_driverops,
	.drv_linkinfo = "AMD User HSMP Access",
	.drv_dev_ops = &uhsmp_dev_ops
};

static struct modlinkage uhsmp_modlinkage = {
	.ml_rev = MODREV_1,
	.ml_linkage = { &uhsmp_modldrv, NULL }
};

int
_init(void)
{
	return (mod_install(&uhsmp_modlinkage));
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&uhsmp_modlinkage, modinfop));
}

int
_fini(void)
{
	return (mod_remove(&uhsmp_modlinkage));
}
