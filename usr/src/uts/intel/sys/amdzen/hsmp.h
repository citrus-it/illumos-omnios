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

#ifndef _SYS_AMDZEN_HSMP_H
#define	_SYS_AMDZEN_HSMP_H

#include <sys/bitext.h>
#include <sys/amdzen/smn.h>
#include <sys/x86_archext.h>

/*
 * This header covers the SMN Mailbox Registers and associated data for the
 * HSMP (Host System Management Port).
 */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * HSMP commands. The names mirror those used by AMD's published HSMP
 * documentation.
 */
#define	HSMP_CMD_TESTMESSAGE		0x1
#define	HSMP_CMD_GETIFVERSION		0x3
#define	HSMP_CMD_GETSOCKETPOWER		0x4
#define	HSMP_CMD_GETFCLKMCLK		0xf
#define	HSMP_CMD_GETCCLKTHROTTLELIMIT	0x10
#define	HSMP_CMD_GETC0PERCENT		0x11
#define	HSMP_CMD_GETSOCKETFREQLIMIT	0x19
#define	HSMP_CMD_GETMETRICTABLEVER	0x23
#define	HSMP_CMD_GETMETRICTABLE		0x24
#define	HSMP_CMD_GETMETRICTABLEADDR	0x25

/*
 * The result word of HSMP_CMD_GETSOCKETFREQLIMIT carries the current frequency
 * in MHz in its upper half and a bitfield describing what is currently limiting
 * the frequency in its lower half. The limit source bits are not decoded here.
 */
#define	HSMP_SOCKET_FREQ_GET_MHZ(x)	bitx32(x, 31, 16)
#define	HSMP_SOCKET_FREQ_GET_SOURCE(x)	bitx32(x, 15, 0)

/*
 * Documented HSMP response codes.
 */
#define	HSMP_RESPONSE_INCOMPLETE	0x0
#define	HSMP_RESPONSE_OK		0x1
#define	HSMP_RESPONSE_REJECTED_BUSY	0xfc
#define	HSMP_RESPONSE_REJECTED_PREREQ	0xfd
#define	HSMP_RESPONSE_INVALID_MSGID	0xfe
#define	HSMP_RESPONSE_INVALID_ARGS	0xff

/*
 * Supported number of functions for each interface version.
 */
#define	HSMP_IFVER1_FUNCS		0x11
#define	HSMP_IFVER2_FUNCS		0x12
#define	HSMP_IFVER3_FUNCS		0x14
#define	HSMP_IFVER4_FUNCS		0x15
#define	HSMP_IFVER5_FUNCS		0x2f
#define	HSMP_IFVER7_FUNCS		0x3f

/*
 * HSMP register block.
 */
#define	SMN_HSMP_APERTURE_MASK	0xffffffffffffff00
AMDZEN_MAKE_SMN_REG_FN(amdzen_smn_hsmp_reg, HSMP, 0x3b10900,
    SMN_HSMP_APERTURE_MASK, 1, 0);

/*
 * HSMP Message ID.
 * The address of the message ID register changed in Turin to something in the
 * same range as the others.
 */
/*CSTYLED*/
#define	D_SMN_HSMP_MSGID	(const smn_reg_def_t){	\
	.srd_unit = SMN_UNIT_HSMP,	\
	.srd_reg = 0x34,		\
}
#define	HSMP_LEGACY_ID_REG	SMN_MAKE_REG(0x3b10534, SMN_UNIT_HSMP)
static inline smn_reg_t
SMN_HSMP_MSGID(x86_processor_family_t fam)
{
	switch (fam) {
	case X86_PF_AMD_MILAN:
	case X86_PF_AMD_GENOA:
	case X86_PF_AMD_VERMEER:
	case X86_PF_AMD_REMBRANDT:
	case X86_PF_AMD_CEZANNE:
	case X86_PF_AMD_RAPHAEL:
	case X86_PF_AMD_PHOENIX:
	case X86_PF_AMD_BERGAMO:
		return (HSMP_LEGACY_ID_REG);
	default:
		break;
	}
	return (amdzen_smn_hsmp_reg(0, D_SMN_HSMP_MSGID, 0));
}

/*
 * HSMP Response Status.
 */
/*CSTYLED*/
#define	D_SMN_HSMP_RESP	(const smn_reg_def_t){	\
	.srd_unit = SMN_UNIT_HSMP,	\
	.srd_reg = 0x80,		\
}
#define	SMN_HSMP_RESP	\
    amdzen_smn_hsmp_reg(0, D_SMN_HSMP_RESP, 0)

/*
 * HSMP Arguments.
 */
/*CSTYLED*/
#define	D_SMN_HSMP_ARG	(const smn_reg_def_t){	\
	.srd_unit = SMN_UNIT_HSMP,	\
	.srd_reg = 0xe0,		\
	.srd_stride = 0x4,		\
	.srd_nents = 8			\
}
#define	SMN_HSMP_ARG(n)	\
    amdzen_smn_hsmp_reg(0, D_SMN_HSMP_ARG, n)

/*
 * Metric table.
 *
 * Newer parts maintain a large table of telemetry in DRAM. The physical
 * address of the table is obtained with HSMP_CMD_GETMETRICTABLEADDR and the SMU
 * refreshes the contents in response to HSMP_CMD_GETMETRICTABLE. The layout
 * depends on the processor family and the structures below mirror those in
 * AMD's HSMP documentation. All members are naturally aligned and the
 * structures must not be packed. The numeric meaning and units of each member
 * are described in the AMD PPR.
 *
 * This layout is used by AMD Family 19h (Genoa, Bergamo) parts.
 */
typedef struct hsmp_metric_table {
	uint32_t hmt_accumulation_counter;

	/* Temperature */
	uint32_t hmt_max_socket_temperature;
	uint32_t hmt_max_vr_temperature;
	uint32_t hmt_max_hbm_temperature;
	uint64_t hmt_max_socket_temperature_acc;
	uint64_t hmt_max_vr_temperature_acc;
	uint64_t hmt_max_hbm_temperature_acc;

	/* Power */
	uint32_t hmt_socket_power_limit;
	uint32_t hmt_max_socket_power_limit;
	uint32_t hmt_socket_power;

	/* Energy */
	uint64_t hmt_timestamp;
	uint64_t hmt_socket_energy_acc;
	uint64_t hmt_ccd_energy_acc;
	uint64_t hmt_xcd_energy_acc;
	uint64_t hmt_aid_energy_acc;
	uint64_t hmt_hbm_energy_acc;

	/* Frequency */
	uint32_t hmt_cclk_frequency_limit;
	uint32_t hmt_gfxclk_frequency_limit;
	uint32_t hmt_fclk_frequency;
	uint32_t hmt_uclk_frequency;
	uint32_t hmt_socclk_frequency[4];
	uint32_t hmt_vclk_frequency[4];
	uint32_t hmt_dclk_frequency[4];
	uint32_t hmt_lclk_frequency[4];
	uint64_t hmt_gfxclk_frequency_acc[8];
	uint64_t hmt_cclk_frequency_acc[96];

	/* Frequency range */
	uint32_t hmt_max_cclk_frequency;
	uint32_t hmt_min_cclk_frequency;
	uint32_t hmt_max_gfxclk_frequency;
	uint32_t hmt_min_gfxclk_frequency;
	uint32_t hmt_fclk_frequency_table[4];
	uint32_t hmt_uclk_frequency_table[4];
	uint32_t hmt_socclk_frequency_table[4];
	uint32_t hmt_vclk_frequency_table[4];
	uint32_t hmt_dclk_frequency_table[4];
	uint32_t hmt_lclk_frequency_table[4];
	uint32_t hmt_max_lclk_dpm_range;
	uint32_t hmt_min_lclk_dpm_range;

	/* xGMI */
	uint32_t hmt_xgmi_width;
	uint32_t hmt_xgmi_bitrate;
	uint64_t hmt_xgmi_read_bandwidth_acc[8];
	uint64_t hmt_xgmi_write_bandwidth_acc[8];

	/* Activity */
	uint32_t hmt_socket_c0_residency;
	uint32_t hmt_socket_gfx_busy;
	uint32_t hmt_dram_bandwidth_utilization;
	uint64_t hmt_socket_c0_residency_acc;
	uint64_t hmt_socket_gfx_busy_acc;
	uint64_t hmt_dram_bandwidth_acc;
	uint32_t hmt_max_dram_bandwidth;
	uint64_t hmt_dram_bandwidth_utilization_acc;
	uint64_t hmt_pcie_bandwidth_acc[4];

	/* Throttlers */
	uint32_t hmt_prochot_residency_acc;
	uint32_t hmt_ppt_residency_acc;
	uint32_t hmt_socket_thm_residency_acc;
	uint32_t hmt_vr_thm_residency_acc;
	uint32_t hmt_hbm_thm_residency_acc;
	uint32_t hmt_spare;

	uint32_t hmt_gfxclk_frequency[8];
} hsmp_metric_table_t;

/*
 * This layout is used by AMD Family 1Ah, Model 50h-5Fh (Turin) parts. It is
 * hierarchical: a socket-wide I/O die (IOD) section followed by a per-CCD
 * section for each of the (up to) eight CCDs.
 */
#define	HSMP_F1A_M50_MAX_CORES_PER_CCD	32
#define	HSMP_F1A_M50_MAX_FREQ_TABLE	4
#define	HSMP_F1A_M50_MAX_XGMI		8
#define	HSMP_F1A_M50_MAX_PCIE		8
#define	HSMP_F1A_M50_MAX_CCD		8

typedef struct hsmp_metric_table_f1a_m50_iod {
	uint32_t hmti_num_active_ccds;
	uint32_t hmti_accumulation_counter;

	/* Temperature */
	uint64_t hmti_max_socket_temperature_acc;

	/* Power */
	uint32_t hmti_socket_power_limit;
	uint32_t hmti_max_socket_power_limit;
	uint64_t hmti_socket_power_acc;
	uint64_t hmti_core_power_acc;
	uint64_t hmti_uncore_power_acc;

	/* Energy */
	uint64_t hmti_timestamp;
	uint64_t hmti_socket_energy_acc;
	uint64_t hmti_core_energy_acc;
	uint64_t hmti_uncore_energy_acc;

	/* Frequency */
	uint64_t hmti_fclk_frequency_acc;
	uint64_t hmti_uclk_frequency_acc;
	uint64_t hmti_ddr_rate_acc;
	uint64_t hmti_lclk_frequency_acc[HSMP_F1A_M50_MAX_FREQ_TABLE];

	/* Frequency range */
	uint32_t hmti_fclk_frequency_table[HSMP_F1A_M50_MAX_FREQ_TABLE];
	uint32_t hmti_uclk_frequency_table[HSMP_F1A_M50_MAX_FREQ_TABLE];
	uint32_t hmti_ddr_rate_table[HSMP_F1A_M50_MAX_FREQ_TABLE];
	uint32_t hmti_max_df_pstate_range;
	uint32_t hmti_min_df_pstate_range;
	uint32_t hmti_lclk_frequency_table[HSMP_F1A_M50_MAX_FREQ_TABLE];
	uint32_t hmti_max_lclk_dpm_range;
	uint32_t hmti_min_lclk_dpm_range;

	/* xGMI */
	uint64_t hmti_xgmi_bit_rate[HSMP_F1A_M50_MAX_XGMI];
	uint64_t hmti_xgmi_read_bandwidth[HSMP_F1A_M50_MAX_XGMI];
	uint64_t hmti_xgmi_write_bandwidth[HSMP_F1A_M50_MAX_XGMI];

	/* Activity */
	uint64_t hmti_socket_c0_residency_acc;
	uint64_t hmti_socket_df_cstate_residency_acc;
	uint64_t hmti_dram_read_bandwidth_acc;
	uint64_t hmti_dram_write_bandwidth_acc;
	uint32_t hmti_max_dram_bandwidth;
	uint64_t hmti_pcie_bandwidth_acc[HSMP_F1A_M50_MAX_PCIE];

	/* Throttlers */
	uint32_t hmti_prochot_residency_acc;
	uint32_t hmti_ppt_residency_acc;
	uint32_t hmti_thm_residency_acc;
	uint32_t hmti_vrhot_residency_acc;
	uint32_t hmti_cpu_tdc_residency_acc;
	uint32_t hmti_soc_tdc_residency_acc;
	uint32_t hmti_io_mem_tdc_residency_acc;
	uint32_t hmti_fit_residency_acc;
} hsmp_metric_table_f1a_m50_iod_t;

typedef struct hsmp_metric_table_f1a_m50_ccd {
	uint32_t hmtc_core_apicid_of_thread0[HSMP_F1A_M50_MAX_CORES_PER_CCD];
	uint64_t hmtc_core_c0[HSMP_F1A_M50_MAX_CORES_PER_CCD];
	uint64_t hmtc_core_cc1[HSMP_F1A_M50_MAX_CORES_PER_CCD];
	uint64_t hmtc_core_cc6[HSMP_F1A_M50_MAX_CORES_PER_CCD];
	uint64_t hmtc_core_frequency[HSMP_F1A_M50_MAX_CORES_PER_CCD];
	uint64_t hmtc_core_frequency_effective[HSMP_F1A_M50_MAX_CORES_PER_CCD];
	uint64_t hmtc_core_power[HSMP_F1A_M50_MAX_CORES_PER_CCD];
} hsmp_metric_table_f1a_m50_ccd_t;

typedef struct hsmp_metric_table_f1a_m50 {
	hsmp_metric_table_f1a_m50_iod_t hmt_iod;
	hsmp_metric_table_f1a_m50_ccd_t hmt_ccd[HSMP_F1A_M50_MAX_CCD];
} hsmp_metric_table_f1a_m50_t;

#ifdef __cplusplus
}
#endif

#endif /* _SYS_AMDZEN_HSMP_H */
