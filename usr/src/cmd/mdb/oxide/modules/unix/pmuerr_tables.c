/*
 * This file and its contents are supplied under the terms of the
 * Common Development and Distribution License ("CDDL"), version 1.0.
 * You may only use this file in accordance with the terms of version
 * 1.0 of the CDDL.
 *
 * A full copy of the text of the CDDL should have accompanied this
 * source. A copy of the CDDL is also available via the Internet at
 * http://www.illumos.org/license/CDDL.
 */

/*
 * Copyright 2026 Oxide Computer Company
 */

/*
 * PMU error code lookup tables. Each entry maps a full 32-bit PMU error code
 * (MSG_ID in bits 31:16, ARG_COUNT in bits 15:0) to a human-readable message
 * string. Entries are tagged with the training phase(s) they apply to.
 *
 * Tables are organised per (platform, SMU firmware version). The SMU version
 * serves as a proxy for the PMU firmware version. At init time, the best
 * matching table is selected based on the detected processor family and SMU
 * firmware version. If no exact match is found, the latest available table
 * for the platform is used and a warning is printed.
 */

#include <mdb/mdb_modapi.h>
#include <sys/types.h>
#include <sys/sysmacros.h>

#include "pmuerr_tables.h"

#define	PH_T	PMUERR_PH_TRAIN
#define	PH_P	PMUERR_PH_POST
#define	PH_B	PMUERR_PH_BOTH

/*
 * Turin Rev Cx (RDIMM)
 *
 * This table covers both the training and post-training error code namespaces.
 * The full 32-bit error code (MSG_ID in bits 31:16, ARG_COUNT in bits 15:0)
 * is used as the lookup key; training and post-training codes occupy distinct
 * ranges of this space. Entries that appear in both namespaces with the same
 * code and message are tagged with PMUERR_PH_BOTH.
 */
static const pmuerr_entry_t pmuerr_turin_cx_94_91[] = {
	/* Post-Training */
	{ PH_P, 0x00020000,
	    "start address of ACSM MPR read sequence must be aligned on even "
	    "acsm addr position" },
	/* Training */
	{ PH_T, 0x000F0000,
	    "start address of ACSM MPR read sequence must be aligned on even "
	    "acsm addr position" },
	{ PH_P, 0x00150001,
	    "CS%#r failed to find a DFIMRL setting that worked for all bytes "
	    "during MaxRdLat training" },
	{ PH_P, 0x00170001,
	    "CS%#r failed to find a DFIMRL setting that worked for channel A "
	    "bytes during MaxRdLat training" },
	{ PH_P, 0x00190001,
	    "CS%#r failed to find a DFIMRL setting that worked for channel B "
	    "bytes during MaxRdLat training" },
	{ PH_P, 0x001B0000,
	    "No passing DFIMRL value found for any chip select during "
	    "MaxRdLat training" },
	{ PH_P, 0x001C0000,
	    "No passing DFIMRL value found for any chip select for channel A "
	    "during MaxRdLat training" },
	{ PH_P, 0x001D0000,
	    "No passing DFIMRL value found for any chip select for channel B "
	    "during MaxRdLat training" },
	{ PH_T, 0x00220001,
	    "CS%#r failed to find a DFIMRL setting that worked for all bytes "
	    "during MaxRdLat training" },
	{ PH_T, 0x00240001,
	    "CS%#r failed to find a DFIMRL setting that worked for channel A "
	    "bytes during MaxRdLat training" },
	{ PH_P, 0x00250003,
	    "Dbyte %#r lane %r txDqDly passing region is too small "
	    "(width = %#r)" },
	{ PH_T, 0x00260001,
	    "CS%#r failed to find a DFIMRL setting that worked for channel B "
	    "bytes during MaxRdLat training" },
	{ PH_T, 0x00280000,
	    "No passing DFIMRL value found for any chip select during "
	    "MaxRdLat training" },
	{ PH_T, 0x00290000,
	    "No passing DFIMRL value found for any chip select for channel A "
	    "during MaxRdLat training" },
	{ PH_T, 0x002A0000,
	    "No passing DFIMRL value found for any chip select for channel B "
	    "during MaxRdLat training" },
	{ PH_P, 0x002A0001,
	    "Dbyte %#r txDqDly DM training did not start inside the eye" },
	{ PH_P, 0x002F0003,
	    "Dbyte %#r lane %r txDqDly DM passing region is too small "
	    "(width = %#r)" },
	{ PH_P, 0x00320002,
	    "Dbyte %#r nibble %r found multiple working coarse delay setting "
	    "for MRD/MWD" },
	{ PH_T, 0x00320003,
	    "Dbyte %#r lane %r txDqDly passing region is too small "
	    "(width = %#r)" },
	{ PH_P, 0x00360003,
	    "Dbyte %#r nibble %r MRD/MWD passing region is too small "
	    "(width = %#r)" },
	{ PH_T, 0x00370001,
	    "Dbyte %#r txDqDly DM training did not start inside the eye" },
	{ PH_P, 0x003B0001,
	    "MRD/MWD training is not converging on rank %#r after trying all "
	    "possible RCD CmdDly" },
	{ PH_T, 0x003C0003,
	    "Dbyte %#r lane %r txDqDly DM passing region is too small "
	    "(width = %#r)" },
	{ PH_T, 0x003F0002,
	    "Dbyte %#r nibble %r found multiple working coarse delay setting "
	    "for MRD/MWD" },
	{ PH_T, 0x00430003,
	    "Dbyte %#r nibble %r MRD/MWD passing region is too small "
	    "(width = %#r)" },
	{ PH_P, 0x00440003,
	    "Dbyte %#r nibble %r rxClkDly passing region is too small "
	    "(width = %#r)" },
	{ PH_T, 0x00480001,
	    "MRD/MWD training is not converging on rank %#r after trying all "
	    "possible RCD CmdDly" },
	{ PH_P, 0x004D0003,
	    "D5 rd2D no passing region for rank %#r, db %r, lane %r" },
	{ PH_P, 0x004E0002,
	    "Wrong PBDly seed %#r results in too small RxClkDly %r" },
	{ PH_T, 0x00510003,
	    "Dbyte %#r nibble %r rxClkDly passing region is too small "
	    "(width = %#r)" },
	{ PH_P, 0x00580002,
	    "tg %#r nib %r RxClkDly had no passing region" },
	{ PH_T, 0x005A0003,
	    "D5 rd2D no passing region for rank %#r, db %r, lane %r" },
	{ PH_T, 0x005B0002,
	    "Wrong PBDly seed %#r results in too small RxClkDly %r" },
	{ PH_P, 0x005F0002,
	    "db %#r lane %r vrefDAC had no passing region" },
	{ PH_T, 0x00650002,
	    "tg %#r nib %r RxClkDly had no passing region" },
	{ PH_T, 0x006C0002,
	    "db %#r lane %r vrefDAC had no passing region" },
	{ PH_P, 0x00730002,
	    "dbyte %#r lane %r TxDqDly had no passing region" },
	{ PH_P, 0x007B0001,
	    "nib %#r vrefDQ had no passing region" },
	{ PH_T, 0x00800002,
	    "dbyte %#r lane %r TxDqDly had no passing region" },
	{ PH_T, 0x00880001,
	    "nib %#r vrefDQ had no passing region" },
	{ PH_P, 0x00990003,
	    "Dbyte %#r nibble %r MRD passing region is too small "
	    "(width = %#r)" },
	{ PH_P, 0x009A0003,
	    "Dbyte %#r nibble %r MWD passing region is too small "
	    "(width = %#r)" },
	{ PH_P, 0x00A50002,
	    "dbyte %#r lane %r's per-lane vrefDAC's had no passing region" },
	{ PH_T, 0x00A60003,
	    "Dbyte %#r nibble %r MRD passing region is too small "
	    "(width = %#r)" },
	{ PH_T, 0x00A70003,
	    "Dbyte %#r nibble %r MWD passing region is too small "
	    "(width = %#r)" },
	{ PH_P, 0x00AF0002,
	    "dbyte %#r lane %r failed read deskew" },
	{ PH_T, 0x00B20002,
	    "dbyte %#r lane %r's per-lane vrefDAC's had no passing region" },
	{ PH_T, 0x00BC0002,
	    "dbyte %#r lane %r failed read deskew" },
	{ PH_P, 0x00C10000,
	    "EnabledDQsChA must be > 0" },
	{ PH_P, 0x00C20000,
	    "EnabledDQsChB must be > 0" },
	{ PH_T, 0x00C80001,
	    "Wrong PMU image loaded. message Block DramType = %#r, but "
	    "image built for D4U Type" },
	{ PH_T, 0x00C90001,
	    "Wrong PMU image loaded. message Block DramType = %#r, but "
	    "image built for D4R Type" },
	{ PH_T, 0x00CA0001,
	    "Wrong PMU image loaded. message Block DramType = %#r, but "
	    "image built for D4LR Type" },
	{ PH_T, 0x00CB0000,
	    "Both 2t timing mode and ddr4 geardown mode specified in the "
	    "messageblock's PhyCfg and MR3 fields. Only one can be enabled" },
	{ PH_P, 0x00D00000,
	    "No dbiDisable without d4" },
	{ PH_T, 0x00D50000,
	    "start address of ACSM RxEn sequence must be aligned on even "
	    "acsm addr position" },
	{ PH_T, 0x00D90001,
	    "Dbyte %#r couldn't find the rising edge of DQS during RxEn "
	    "Training" },
	{ PH_T, 0x00E00001,
	    "Failed MRE for nib %#r" },
	{ PH_P, 0x00E00000,
	    "getMaxRxen() failed to find largest rxen nibble delay" },
	{ PH_T, 0x00EA0002,
	    "Failed MREP for nib %#r with %r one" },
	{ PH_T, 0x00FB0003,
	    "CSn %#r Channel %r CS train passing region is too small "
	    "(width = %#r)" },
	{ PH_T, 0x01040004,
	    "CSn %#r Channel %r Signal A%r CA train passing region is too "
	    "small (width = %#r)" },
	{ PH_T, 0x010B0003,
	    "CSn %#r Channel %r VrefCS train passing region is too small "
	    "(width = %#r)" },
	{ PH_T, 0x01110004,
	    "CSn %#r Channel %r Signal CA%r VrefCA train passing region is "
	    "too small (width = %#r)" },
	{ PH_T, 0x01140003,
	    "RCD CA DFE training failed for CS %#r Channel %r CA%r (no "
	    "open eye was found)" },
	{ PH_T, 0x01160003,
	    "RCD CA DFE training could not calculate trained VrefCA center "
	    "for CS %#r Channel %r CA%r" },
	{ PH_T, 0x01270003,
	    "CSn %#r Channel %r RCD CS train passing region is too small "
	    "(width = %#r)" },
	{ PH_T, 0x01280000,
	    "neither CS0 nor CS2 present in RCD CS training." },
	{ PH_T, 0x012C0003,
	    "eye_center[%#r][%r]=%r is not within rcw_0x12 +/-24" },
	{ PH_T, 0x012F0000,
	    "eye center average should never be less than -12" },
	{ PH_T, 0x01300000,
	    "eye center average should never be greater than 63" },
	{ PH_T, 0x01320000,
	    "start address of ACSM RCD_CA sequence must be aligned on even "
	    "acsm addr position" },
	{ PH_P, 0x01350000,
	    "start address of ACSM WR/RD activate sequence must be aligned "
	    "on even acsm addr position" },
	{ PH_P, 0x01370000,
	    "start address of ACSM WR/RD program sequence must be aligned "
	    "on even acsm addr position" },
	{ PH_P, 0x01390000,
	    "start address of ACSM DM sequence must be aligned on even acsm "
	    "addr position" },
	{ PH_P, 0x013A0003,
	    "Firmware was not able to detect swizzle setting for TG%#r "
	    "Dbyte%#r DQ%r" },
	{ PH_P, 0x013B0005,
	    "Wrong DqLnSelTg setting for TG%#r Dbyte%r DQ%r: expected %r "
	    "found %#r" },
	{ PH_P, 0x013C0005,
	    "Wrong DqLnSelTg setting for TG%#r Dbyte%r: DQ%r and DQ%r have "
	    "the same value (%#r)" },
	{ PH_P, 0x013D0003,
	    "Firmware was not able to detect swizzle setting for TG%#r "
	    "Dbyte%#r DQ%r" },
	{ PH_T, 0x013F0002,
	    "specified bank (BG:%#r BA:%r) is not available for PPR" },
	{ PH_T, 0x01430000,
	    "Mismatched internal revision between DCCM and ICCM images" },
	{ PH_P, 0x01470000,
	    "internal error in d5_detect_dq_swizzle() cannot find unused "
	    "mapping" },
	{ PH_P, 0x014F0006,
	    "Mismatch found csn %#r, db %r, dbLane %r, errorCount %r, "
	    "mr10Offset %#r, wrBubble(memclk) %r." },
	{ PH_P, 0x01500004,
	    "Mismatch found at end of training after MRL csn %#r, db %r, "
	    "dbLane %#r, iter %r." },
	{ PH_P, 0x01540000,
	    "start address of ACSM WL sequence must be aligned on even acsm "
	    "addr position" },
	{ PH_P, 0x015F0001,
	    "Failed DWL for nib %#r" },
	{ PH_P, 0x01620002,
	    "nib %#r external WL %r underflow" },
	{ PH_P, 0x01640000,
	    "internal DWL error ACSM sequences overlap" },
	{ PH_P, 0x01680000,
	    "Some nibble didn't converge during internal WL" },
	{ PH_P, 0x016A0002,
	    "nib %#r internal WL %r overflow" },
	{ PH_P, 0x016E0002,
	    "nib %#r external WL %r underflow" },
	{ PH_T, 0x01720000,
	    "EnabledDQsChA must be > 0" },
	{ PH_T, 0x01730000,
	    "EnabledDQsChB must be > 0" },
	{ PH_P, 0x01740000,
	    "Some nibble didn't converge during internal WL" },
	{ PH_P, 0x01770002,
	    "nib %#r internal WL %r overflow" },
	{ PH_P, 0x017C0002,
	    "nib %#r external WL %r overflow" },
	{ PH_P, 0x017D0002,
	    "nib %#r external WL %r underflow" },
	{ PH_P, 0x017F0002,
	    "nib %#r external WL %r overflow" },
	{ PH_P, 0x01800002,
	    "nib %#r external WL %r underflow" },
	{ PH_T, 0x01810000,
	    "No dbiDisable without d4" },
	{ PH_T, 0x01910000,
	    "getMaxRxen() failed to find largest rxen nibble delay" },
	{ PH_P, 0x01960000,
	    "Failed write leveling coarse" },
	{ PH_P, 0x019B0001,
	    "All margin after write leveling coarse are smaller than "
	    "minMargin %#r" },
	{ PH_P, 0x01A30002,
	    "Failed DWL for nib %#r with %r one" },
	{ PH_P, 0x01C10002,
	    "db %#r lane %r vrefDAC had no passing region" },
	{ PH_T, 0x01E60000,
	    "start address of ACSM WR/RD activate sequence must be aligned "
	    "on even acsm addr position" },
	{ PH_T, 0x01E80000,
	    "start address of ACSM WR/RD program sequence must be aligned "
	    "on even acsm addr position" },
	{ PH_T, 0x01EA0000,
	    "start address of ACSM DM sequence must be aligned on even acsm "
	    "addr position" },
	{ PH_T, 0x01EB0003,
	    "Firmware was not able to detect swizzle setting for TG%#r "
	    "Dbyte%#r DQ%r" },
	{ PH_T, 0x01EC0005,
	    "Wrong DqLnSelTg setting for TG%#r Dbyte%r DQ%r: expected %r "
	    "found %#r" },
	{ PH_T, 0x01ED0005,
	    "Wrong DqLnSelTg setting for TG%#r Dbyte%r: DQ%r and DQ%r have "
	    "the same value (%#r)" },
	{ PH_T, 0x01EE0003,
	    "Firmware was not able to detect swizzle setting for TG%#r "
	    "Dbyte%#r DQ%r" },
	{ PH_T, 0x01F80000,
	    "internal error in d5_detect_dq_swizzle() cannot find unused "
	    "mapping" },
	{ PH_T, 0x02000006,
	    "Mismatch found csn %#r, db %r, dbLane %r, errorCount %r, "
	    "mr10Offset %#r, wrBubble(memclk) %r." },
	{ PH_T, 0x02010004,
	    "Mismatch found at end of training after MRL csn %#r, db %r, "
	    "dbLane %#r, iter %r." },
	{ PH_T, 0x02050000,
	    "start address of ACSM WL sequence must be aligned on even acsm "
	    "addr position" },
	{ PH_T, 0x02100001,
	    "Failed DWL for nib %#r" },
	{ PH_T, 0x02130002,
	    "nib %#r external WL %r underflow" },
	{ PH_T, 0x02150000,
	    "internal DWL error ACSM sequences overlap" },
	{ PH_T, 0x02190000,
	    "Some nibble didn't converge during internal WL" },
	{ PH_T, 0x021B0002,
	    "nib %#r internal WL %r overflow" },
	{ PH_T, 0x021F0002,
	    "nib %#r external WL %r underflow" },
	{ PH_T, 0x02250000,
	    "Some nibble didn't converge during internal WL" },
	{ PH_T, 0x02280002,
	    "nib %#r internal WL %r overflow" },
	{ PH_T, 0x022D0002,
	    "nib %#r external WL %r overflow" },
	{ PH_T, 0x022E0002,
	    "nib %#r external WL %r underflow" },
	{ PH_T, 0x02300002,
	    "nib %#r external WL %r overflow" },
	{ PH_T, 0x02310002,
	    "nib %#r external WL %r underflow" },
	{ PH_T, 0x02470000,
	    "Failed write leveling coarse" },
	{ PH_T, 0x024C0001,
	    "All margin after write leveling coarse are smaller than "
	    "minMargin %#r" },
	{ PH_T, 0x02540002,
	    "Failed DWL for nib %#r with %r one" },
	/*
	 * These entries appear in both the training and post-training tables
	 * with the same error code and message.
	 */
	{ PH_B, 0x04060001,
	    "acsm_set_cmd to non existent instruction address %#r" },
	{ PH_B, 0x04070001,
	    "acsm_set_cmd with unknown ddr cmd %#r" },
	{ PH_B, 0x040B0000,
	    "Polling on ACSM done failed to complete in acsm_poll_done()..." },
	{ PH_B, 0x04100004,
	    "setAcsmCLCWL: cl and cwl must be each >= %#r, and %r, resp. "
	    "CL=%#r CWL=%r" },
	{ PH_B, 0x04110002,
	    "setAcsmCLCWL: cl and cwl must be each >= 5. CL=%#r CWL=%r" },
	{ PH_B, 0x04130001,
	    "Reserved value of register F0RC0F found in message block: %#r" },
};

/*
 * Milan (RDIMM)
 *
 * This table covers both the 1D and 2D training error code namespaces.
 * The full 32-bit error code (MSG_ID in bits 31:16, ARG_COUNT in bits 15:0)
 * is used as the lookup key; 1D and 2D training codes occupy mostly distinct
 * ranges of this space but a small number of codes carry different messages
 * depending on the training dimension. Entries that appear in both namespaces
 * with the same code and message are tagged with PMUERR_PH_BOTH.
 */
static const pmuerr_entry_t pmuerr_milan_45_104[] = {
	/* Training (1D) */
	{ PH_T, 0x00040000,
	    "User requested MPR read pattern for read DQS training in "
	    "DDR3 Mode" },
	/* Post-Training (2D) */
	{ PH_P, 0x00080001,
	    "Illegal timing group number, %#r, in getPtrVrefDq" },
	{ PH_T, 0x00110001,
	    "CS%#r failed to find a DFIMRL setting that worked for all "
	    "bytes during MaxRdLat training" },
	{ PH_T, 0x00130000,
	    "No passing DFIMRL value found for any chip select during "
	    "MaxRdLat training" },
	{ PH_T, 0x00140003,
	    "Dbyte %#r lane %#r txDqDly passing region is too small "
	    "(width = %#r)" },
	{ PH_P, 0x001b0002,
	    "LP4 rank %#r cannot be mapped on tg %#r" },
	{ PH_T, 0x001c0003,
	    "Dbyte %#r lane %#r txDqDly passing region is too small "
	    "(width = %#r)" },
	{ PH_T, 0x00240001,
	    "Dbyte %#r txDqDly DM training did not start inside the "
	    "eye" },
	{ PH_T, 0x00280003,
	    "Dbyte %#r lane %#r txDqDly DM passing region is too small "
	    "(width = %#r)" },
	{ PH_T, 0x002b0002,
	    "Dbyte %#r nibble %#r found multiple working coarse delay "
	    "setting for MRD/MWD" },
	{ PH_T, 0x00300003,
	    "Dbyte %#r nibble %#r MRD/MWD passing region is too small "
	    "(width = %#r)" },
	{ PH_T, 0x00350001,
	    "MRD/MWD training is not converging on rank %#r after "
	    "trying all possible RCD CmdDly" },
	{ PH_T, 0x003d0003,
	    "Dbyte %#r nibble %#r rxClkDly passing region is too small "
	    "(width = %#r)" },
	{ PH_T, 0x00420002,
	    "dbyte %#r lane %#r's per-lane vrefDAC's had no passing "
	    "region" },
	{ PH_P, 0x00430000,
	    "No passing region found for 1 or more lanes. Set "
	    "hdtCtrl=4 to see passing regions" },
	{ PH_P, 0x00470000,
	    "No passing region found for 1 or more lanes. Set "
	    "hdtCtrl=4 to see passing regions" },
	{ PH_T, 0x004b0002,
	    "dbyte %#r lane %#r failed read deskew" },
	{ PH_T, 0x004e0000,
	    "Read deskew training has been requested, but "
	    "csrMajorModeDbyte[2] is set" },
	{ PH_T, 0x00510001,
	    "Wrong PMU image loaded. message Block DramType = %#r, but "
	    "image built for D3U Type" },
	{ PH_P, 0x00520000,
	    "No passing region found for 1 or more lanes. Set "
	    "hdtCtrl=4 to see passing regions" },
	{ PH_T, 0x00520001,
	    "Wrong PMU image loaded. message Block DramType = %#r, but "
	    "image built for D3R Type" },
	{ PH_T, 0x00530001,
	    "Wrong PMU image loaded. message Block DramType = %#r, but "
	    "image built for D4U Type" },
	{ PH_T, 0x00540001,
	    "Wrong PMU image loaded. message Block DramType = %#r, but "
	    "image built for D4R Type" },
	{ PH_T, 0x00550001,
	    "Wrong PMU image loaded. message Block DramType = %#r, but "
	    "image built for D4NV Type" },
	{ PH_T, 0x00560001,
	    "Wrong PMU image loaded. message Block DramType = %#r, but "
	    "image built for D4LR Type" },
	{ PH_T, 0x00570000,
	    "Both 2t timing mode and ddr4 geardown mode specified in "
	    "the messageblock's PhyCfg and MR3 fields. Only one can be "
	    "enabled" },
	{ PH_P, 0x00570001,
	    "getCompoundEye Called on lane%#r eye with non-compatible "
	    "centers" },
	{ PH_T, 0x00730000,
	    "RxEn training preamble not found" },
	{ PH_T, 0x00750001,
	    "Dbyte %#r couldn't find the rising edge of DQS during "
	    "RxEn Training" },
	{ PH_P, 0x00750003,
	    "Dbyte %#r nibble %#r's optimal rxClkDly of %#r is out of "
	    "bounds" },
	{ PH_P, 0x007b0003,
	    "Dbyte %#r lane %#r's optimal txDqDly of %#r is out of "
	    "bounds" },
	{ PH_T, 0x007e0002,
	    "Failed MREP for nib %#r with %#r one" },
	{ PH_P, 0x008e0002,
	    "LP4 rank %#r cannot be mapped on tg %#r" },
	{ PH_P, 0x009c0000,
	    "User requested MPR read pattern for read DQS training in "
	    "DDR3 Mode" },
	{ PH_P, 0x00a90001,
	    "CS%#r failed to find a DFIMRL setting that worked for all "
	    "bytes during MaxRdLat training" },
	{ PH_P, 0x00ab0000,
	    "No passing DFIMRL value found for any chip select during "
	    "MaxRdLat training" },
	{ PH_P, 0x00ac0003,
	    "Dbyte %#r lane %#r txDqDly passing region is too small "
	    "(width = %#r)" },
	{ PH_T, 0x00ae0000,
	    "CA Training Failed." },
	{ PH_P, 0x00b40003,
	    "Dbyte %#r lane %#r txDqDly passing region is too small "
	    "(width = %#r)" },
	{ PH_P, 0x00bc0001,
	    "Dbyte %#r txDqDly DM training did not start inside the "
	    "eye" },
	{ PH_T, 0x00be0000,
	    "Mismatched internal revision between DCCM and ICCM "
	    "images" },
	{ PH_P, 0x00c00003,
	    "Dbyte %#r lane %#r txDqDly DM passing region is too small "
	    "(width = %#r)" },
	{ PH_P, 0x00c30002,
	    "Dbyte %#r nibble %#r found multiple working coarse delay "
	    "setting for MRD/MWD" },
	{ PH_P, 0x00c80003,
	    "Dbyte %#r nibble %#r MRD/MWD passing region is too small "
	    "(width = %#r)" },
	{ PH_P, 0x00cd0001,
	    "MRD/MWD training is not converging on rank %#r after "
	    "trying all possible RCD CmdDly" },
	{ PH_P, 0x00d50003,
	    "Dbyte %#r nibble %#r rxClkDly passing region is too small "
	    "(width = %#r)" },
	{ PH_P, 0x00da0002,
	    "dbyte %#r lane %#r's per-lane vrefDAC's had no passing "
	    "region" },
	{ PH_T, 0x00dd0001,
	    "Invalid PhyDrvImpedance of %#r specified in message "
	    "block." },
	{ PH_T, 0x00de0001,
	    "Invalid PhyOdtImpedance of %#r specified in message "
	    "block." },
	{ PH_T, 0x00df0001,
	    "Invalid BPZNResVal of %#r specified in message block." },
	{ PH_P, 0x00e30002,
	    "dbyte %#r lane %#r failed read deskew" },
	{ PH_P, 0x00e60000,
	    "Read deskew training has been requested, but "
	    "csrMajorModeDbyte[2] is set" },
	{ PH_T, 0x00e70001,
	    "Dbyte %#r read 0 from the DQS oscillator it is connected "
	    "to" },
	{ PH_P, 0x00e90001,
	    "Wrong PMU image loaded. message Block DramType = %#r, but "
	    "image built for D3U Type" },
	{ PH_P, 0x00ea0001,
	    "Wrong PMU image loaded. message Block DramType = %#r, but "
	    "image built for D3R Type" },
	{ PH_P, 0x00eb0001,
	    "Wrong PMU image loaded. message Block DramType = %#r, but "
	    "image built for D4U Type" },
	{ PH_P, 0x00ec0001,
	    "Wrong PMU image loaded. message Block DramType = %#r, but "
	    "image built for D4R Type" },
	{ PH_P, 0x00ed0001,
	    "Wrong PMU image loaded. message Block DramType = %#r, but "
	    "image built for D4NV Type" },
	{ PH_P, 0x00ee0001,
	    "Wrong PMU image loaded. message Block DramType = %#r, but "
	    "image built for D4LR Type" },
	{ PH_T, 0x00ef0000,
	    "No dbiEnable with lp4" },
	{ PH_P, 0x00ef0000,
	    "Both 2t timing mode and ddr4 geardown mode specified in "
	    "the messageblock's PhyCfg and MR3 fields. Only one can be "
	    "enabled" },
	{ PH_T, 0x00f00000,
	    "No dbiDisable with lp4" },
	{ PH_T, 0x01000000,
	    "getMaxRxen() failed to find largest rxen nibble delay" },
	{ PH_P, 0x010b0000,
	    "RxEn training preamble not found" },
	{ PH_P, 0x010d0001,
	    "Dbyte %#r couldn't find the rising edge of DQS during "
	    "RxEn Training" },
	{ PH_P, 0x01160002,
	    "Failed MREP for nib %#r with %#r one" },
	{ PH_P, 0x01460000,
	    "CA Training Failed." },
	{ PH_T, 0x01500001,
	    "Messageblock phyVref=%#r is above the limit for TSMC28's "
	    "attenuated LPDDR4 receivers. Please see the pub "
	    "databook" },
	{ PH_T, 0x01510001,
	    "Messageblock phyVref=%#r is above the limit for TSMC28's "
	    "attenuated DDR4 receivers. Please see the pub databook" },
	{ PH_P, 0x01560000,
	    "Mismatched internal revision between DCCM and ICCM "
	    "images" },
	{ PH_T, 0x01660000,
	    "Failed write leveling coarse" },
	{ PH_T, 0x01700000,
	    "Failed write leveling coarse" },
	{ PH_T, 0x01750001,
	    "All margin after write leveling coarse are smaller than "
	    "minMargin %#r" },
	{ PH_P, 0x01750001,
	    "Invalid PhyDrvImpedance of %#r specified in message "
	    "block." },
	{ PH_P, 0x01760001,
	    "Invalid PhyOdtImpedance of %#r specified in message "
	    "block." },
	{ PH_P, 0x01770001,
	    "Invalid BPZNResVal of %#r specified in message block." },
	{ PH_T, 0x017b0000,
	    "Failed write leveling coarse" },
	{ PH_P, 0x017f0001,
	    "Dbyte %#r read 0 from the DQS oscillator it is connected "
	    "to" },
	{ PH_T, 0x01820002,
	    "Failed DWL for nib %#r with %#r one" },
	{ PH_P, 0x01870000,
	    "No dbiEnable with lp4" },
	{ PH_P, 0x01880000,
	    "No dbiDisable with lp4" },
	{ PH_P, 0x01980000,
	    "getMaxRxen() failed to find largest rxen nibble delay" },
	{ PH_P, 0x01e80001,
	    "Messageblock phyVref=%#r is above the limit for TSMC28's "
	    "attenuated LPDDR4 receivers. Please see the pub "
	    "databook" },
	{ PH_P, 0x01e90001,
	    "Messageblock phyVref=%#r is above the limit for TSMC28's "
	    "attenuated DDR4 receivers. Please see the pub databook" },
	{ PH_P, 0x01fe0000,
	    "Failed write leveling coarse" },
	{ PH_P, 0x02080000,
	    "Failed write leveling coarse" },
	{ PH_P, 0x020d0001,
	    "All margin after write leveling coarse are smaller than "
	    "minMargin %#r" },
	{ PH_P, 0x02130000,
	    "Failed write leveling coarse" },
	{ PH_P, 0x021a0002,
	    "Failed DWL for nib %#r with %#r one" },
	/*
	 * These entries appear in both the 1D and 2D training tables
	 * with the same error code and message.
	 */
	{ PH_B, 0x04000000,
	    "Mailbox Buffer Overflowed." },
	{ PH_B, 0x04010000,
	    "Mailbox Buffer Overflowed." },
	{ PH_B, 0x04070001,
	    "acsm_set_cmd to non existent instruction address %#r" },
	{ PH_B, 0x04080001,
	    "acsm_set_cmd with unknown ddr cmd %#r" },
	{ PH_B, 0x040a0000,
	    "Polling on ACSM done failed to complete in "
	    "acsm_poll_done()..." },
	{ PH_B, 0x040e0002,
	    "setAcsmCLCWL: cl and cwl must be each >= 2 and 5, resp. "
	    "CL=%#r CWL=%#r" },
	{ PH_B, 0x040f0002,
	    "setAcsmCLCWL: cl and cwl must be each >= 5. CL=%#r "
	    "CWL=%#r" },
	{ PH_B, 0x04110001,
	    "Reserved value of register F0RC0F found in message block: "
	    "%#r" },
	{ PH_B, 0x04150001,
	    "Boot clock divider setting of %#r is too small" },
	{ PH_B, 0x04180000,
	    "Delay too large in slomo" },
};

/*
 * Master table of all known PMU error tables, ordered by platform and then
 * by SMU version (ascending). pmuerr_init() searches this to find the best
 * matching table.
 */
static const pmuerr_table_t pmuerr_tables[] = {
	{
		.pmt_family = X86_PF_AMD_TURIN,
		.pmt_smu_maj = 94,
		.pmt_smu_min = 131,
		.pmt_desc = "Turin",
		.pmt_entries = pmuerr_turin_cx_94_91,
		.pmt_nentries = ARRAY_SIZE(pmuerr_turin_cx_94_91),
	},
	{
		.pmt_family = X86_PF_AMD_DENSE_TURIN,
		.pmt_smu_maj = 99,
		.pmt_smu_min = 131,
		.pmt_desc = "Dense Turin",
		.pmt_entries = pmuerr_turin_cx_94_91,
		.pmt_nentries = ARRAY_SIZE(pmuerr_turin_cx_94_91),
	},
	{
		.pmt_family = X86_PF_AMD_MILAN,
		.pmt_smu_maj = 45,
		.pmt_smu_min = 104,
		.pmt_desc = "Milan",
		.pmt_entries = pmuerr_milan_45_104,
		.pmt_nentries = ARRAY_SIZE(pmuerr_milan_45_104),
	},
};

static const pmuerr_table_t *pmuerr_active_table = NULL;

/*
 * Select the best matching PMU error table for the given processor family
 * and SMU firmware version. Within a family, the table with the highest
 * version not exceeding the system's version is preferred. If the system's
 * version is older than all tables, or no table exists for the family, the
 * latest table for that family (if any) is used with a warning.
 */
void
pmuerr_init(x86_processor_family_t family, const uint32_t smu_fw[3])
{
	const pmuerr_table_t *best = NULL;
	const pmuerr_table_t *latest = NULL;

	for (size_t i = 0; i < ARRAY_SIZE(pmuerr_tables); i++) {
		const pmuerr_table_t *t = &pmuerr_tables[i];

		if (t->pmt_family != family)
			continue;

		/*
		 * Track the latest table for this family regardless of version
		 * match.
		 */
		if (latest == NULL ||
		    t->pmt_smu_maj > latest->pmt_smu_maj ||
		    (t->pmt_smu_maj == latest->pmt_smu_maj &&
		    t->pmt_smu_min > latest->pmt_smu_min)) {
			latest = t;
		}

		/*
		 * Keep track of the best table that is not too new for this
		 * system.
		 */
		if (t->pmt_smu_maj <= smu_fw[0] ||
		    (t->pmt_smu_maj == smu_fw[0] &&
		    t->pmt_smu_min <= smu_fw[1])) {
			if (best == NULL ||
			    t->pmt_smu_maj > best->pmt_smu_maj ||
			    (t->pmt_smu_maj == best->pmt_smu_maj &&
			    t->pmt_smu_min > best->pmt_smu_min)) {
				best = t;
			}
		}
	}

	if (best != NULL) {
		pmuerr_active_table = best;
		if (best->pmt_smu_maj != smu_fw[0] ||
		    best->pmt_smu_min != smu_fw[1]) {
			mdb_warn("pmuerr: SMU version %u.%u does not "
			    "exactly match table (%u.%u); error "
			    "messages may be inaccurate\n",
			    smu_fw[0], smu_fw[1],
			    best->pmt_smu_maj, best->pmt_smu_min);
		}
	} else if (latest != NULL) {
		pmuerr_active_table = latest;
		mdb_warn("pmuerr: SMU version %u.%u is older than all "
		    "known tables; using %u.%u; error messages "
		    "may be inaccurate\n",
		    smu_fw[0], smu_fw[1],
		    latest->pmt_desc, latest->pmt_smu_maj, latest->pmt_smu_min);
	}
}

/*
 * Select the latest available table for the given processor family without
 * version matching. This is used when the SMU version is not available,
 * such as when analysing a raw APOB file.
 */
void
pmuerr_init_family(x86_processor_family_t family)
{
	const pmuerr_table_t *latest = NULL;

	for (size_t i = 0; i < ARRAY_SIZE(pmuerr_tables); i++) {
		const pmuerr_table_t *t = &pmuerr_tables[i];

		if (t->pmt_family != family)
			continue;

		if (latest == NULL ||
		    t->pmt_smu_maj > latest->pmt_smu_maj ||
		    (t->pmt_smu_maj == latest->pmt_smu_maj &&
		    t->pmt_smu_min > latest->pmt_smu_min)) {
			latest = t;
		}
	}

	pmuerr_active_table = latest;
}

const pmuerr_entry_t *
pmuerr_lookup(uint32_t code, uint32_t phases)
{
	if (pmuerr_active_table == NULL)
		return (NULL);

	const pmuerr_entry_t *fallback = NULL;

	for (size_t i = 0; i < pmuerr_active_table->pmt_nentries; i++) {
		const pmuerr_entry_t *e = &pmuerr_active_table->pmt_entries[i];

		if (e->pme_code != code)
			continue;

		if ((e->pme_phases & phases) != 0)
			return (e);

		if (fallback == NULL)
			fallback = e;
	}

	return (fallback);
}
