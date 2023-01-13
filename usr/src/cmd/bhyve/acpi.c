/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2012 NetApp, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY NETAPP, INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL NETAPP, INC OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

/*
 * bhyve ACPI table generator.
 *
 * Create the minimal set of ACPI tables required to boot FreeBSD (and
 * hopefully other o/s's) by writing out ASL template files for each of
 * the tables and the compiling them to AML with the Intel iasl compiler.
 * The AML files are then read into guest memory.
 *
 *  The tables are placed in the guest's ROM area just below 1MB physical,
 * above the MPTable.
 *
 *  Layout (No longer correct at FADT and beyond due to properly
 *  calculating the size of the MADT to allow for changes to
 *  VM_MAXCPU above 21 which overflows this layout.)
 *  ------
 *   RSDP  ->   0xf2400    (36 bytes fixed)
 *     RSDT  ->   0xf2440    (36 bytes + 4*7 table addrs, 4 used)
 *     XSDT  ->   0xf2480    (36 bytes + 8*7 table addrs, 4 used)
 *       MADT  ->   0xf2500  (depends on #CPUs)
 *       FADT  ->   0xf2600  (268 bytes)
 *       HPET  ->   0xf2740  (56 bytes)
 *       MCFG  ->   0xf2780  (60 bytes)
 *         FACS  ->   0xf27C0 (64 bytes)
 *         DSDT  ->   0xf2800 (variable - can go up to 0x100000)
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/endian.h>
#include <sys/errno.h>
#include <sys/stat.h>

#include <err.h>
#include <paths.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <machine/vmm.h>
#include <vmmapi.h>

#include "bhyverun.h"
#include "acpi.h"
#include "basl.h"
#include "pci_emul.h"
#include "vmgenc.h"

/*
 * Define the base address of the ACPI tables, the sizes of some tables,
 * and the offsets to the individual tables,
 */
#define RSDT_OFFSET		0x040
#define XSDT_OFFSET		0x080
#define MADT_OFFSET		0x100
/*
 * The MADT consists of:
 *	44		Fixed Header
 *	8 * maxcpu	Processor Local APIC entries
 *	12		I/O APIC entry
 *	2 * 10		Interrupt Source Override entries
 *	6		Local APIC NMI entry
 */
#define	MADT_SIZE		roundup2((44 + basl_ncpu*8 + 12 + 2*10 + 6), 0x100)
#define	FADT_OFFSET		(MADT_OFFSET + MADT_SIZE)
#define	FADT_SIZE		0x140
#define	HPET_OFFSET		(FADT_OFFSET + FADT_SIZE)
#define	HPET_SIZE		0x40
#define	MCFG_OFFSET		(HPET_OFFSET + HPET_SIZE)
#define	MCFG_SIZE		0x40
#define	FACS_OFFSET		(MCFG_OFFSET + MCFG_SIZE)
#define	FACS_SIZE		0x40
#define	DSDT_OFFSET		(FACS_OFFSET + FACS_SIZE)

#define	BHYVE_ASL_TEMPLATE	"bhyve.XXXXXXX"
#define BHYVE_ASL_SUFFIX	".aml"
#define BHYVE_ASL_COMPILER	"/usr/sbin/iasl"

#define BHYVE_ADDRESS_IOAPIC 	0xFEC00000
#define BHYVE_ADDRESS_HPET 	0xFED00000
#define BHYVE_ADDRESS_LAPIC 	0xFEE00000

static int basl_keep_temps;
static int basl_verbose_iasl;
static int basl_ncpu;
static uint32_t basl_acpi_base = BHYVE_ACPI_BASE;
static uint32_t hpet_capabilities;

/*
 * Contains the full pathname of the template to be passed
 * to mkstemp/mktemps(3)
 */
static char basl_template[MAXPATHLEN];
static char basl_stemplate[MAXPATHLEN];

/*
 * State for dsdt_line(), dsdt_indent(), and dsdt_unindent().
 */
static FILE *dsdt_fp;
static int dsdt_indent_level;
static int dsdt_error;

struct basl_fio {
	int	fd;
	FILE	*fp;
	char	f_name[MAXPATHLEN];
};

#define EFPRINTF(...) \
	if (fprintf(__VA_ARGS__) < 0) goto err_exit;

#define EFFLUSH(x) \
	if (fflush(x) != 0) goto err_exit;

static int
basl_fwrite_rsdp(FILE *fp)
{
	EFPRINTF(fp, "/*\n");
	EFPRINTF(fp, " * bhyve RSDP template\n");
	EFPRINTF(fp, " */\n");
	EFPRINTF(fp, "[0008]\t\tSignature : \"RSD PTR \"\n");
	EFPRINTF(fp, "[0001]\t\tChecksum : 43\n");
	EFPRINTF(fp, "[0006]\t\tOem ID : \"BHYVE \"\n");
	EFPRINTF(fp, "[0001]\t\tRevision : 02\n");
	EFPRINTF(fp, "[0004]\t\tRSDT Address : %08X\n",
	    basl_acpi_base + RSDT_OFFSET);
	EFPRINTF(fp, "[0004]\t\tLength : 00000024\n");
	EFPRINTF(fp, "[0008]\t\tXSDT Address : 00000000%08X\n",
	    basl_acpi_base + XSDT_OFFSET);
	EFPRINTF(fp, "[0001]\t\tExtended Checksum : 00\n");
	EFPRINTF(fp, "[0003]\t\tReserved : 000000\n");

	EFFLUSH(fp);

	return (0);

err_exit:
	return (errno);
}

static int
basl_fwrite_rsdt(FILE *fp)
{
	EFPRINTF(fp, "/*\n");
	EFPRINTF(fp, " * bhyve RSDT template\n");
	EFPRINTF(fp, " */\n");
	EFPRINTF(fp, "[0004]\t\tSignature : \"RSDT\"\n");
	EFPRINTF(fp, "[0004]\t\tTable Length : 00000000\n");
	EFPRINTF(fp, "[0001]\t\tRevision : 01\n");
	EFPRINTF(fp, "[0001]\t\tChecksum : 00\n");
	EFPRINTF(fp, "[0006]\t\tOem ID : \"BHYVE \"\n");
	EFPRINTF(fp, "[0008]\t\tOem Table ID : \"BVRSDT  \"\n");
	EFPRINTF(fp, "[0004]\t\tOem Revision : 00000001\n");
	/* iasl will fill in the compiler ID/revision fields */
	EFPRINTF(fp, "[0004]\t\tAsl Compiler ID : \"xxxx\"\n");
	EFPRINTF(fp, "[0004]\t\tAsl Compiler Revision : 00000000\n");
	EFPRINTF(fp, "\n");

	/* Add in pointers to the MADT, FADT and HPET */
	EFPRINTF(fp, "[0004]\t\tACPI Table Address 0 : %08X\n",
	    basl_acpi_base + MADT_OFFSET);
	EFPRINTF(fp, "[0004]\t\tACPI Table Address 1 : %08X\n",
	    basl_acpi_base + FADT_OFFSET);
	EFPRINTF(fp, "[0004]\t\tACPI Table Address 2 : %08X\n",
	    basl_acpi_base + HPET_OFFSET);
	EFPRINTF(fp, "[0004]\t\tACPI Table Address 3 : %08X\n",
	    basl_acpi_base + MCFG_OFFSET);

	EFFLUSH(fp);

	return (0);

err_exit:
	return (errno);
}

static int
basl_fwrite_xsdt(FILE *fp)
{
	EFPRINTF(fp, "/*\n");
	EFPRINTF(fp, " * bhyve XSDT template\n");
	EFPRINTF(fp, " */\n");
	EFPRINTF(fp, "[0004]\t\tSignature : \"XSDT\"\n");
	EFPRINTF(fp, "[0004]\t\tTable Length : 00000000\n");
	EFPRINTF(fp, "[0001]\t\tRevision : 01\n");
	EFPRINTF(fp, "[0001]\t\tChecksum : 00\n");
	EFPRINTF(fp, "[0006]\t\tOem ID : \"BHYVE \"\n");
	EFPRINTF(fp, "[0008]\t\tOem Table ID : \"BVXSDT  \"\n");
	EFPRINTF(fp, "[0004]\t\tOem Revision : 00000001\n");
	/* iasl will fill in the compiler ID/revision fields */
	EFPRINTF(fp, "[0004]\t\tAsl Compiler ID : \"xxxx\"\n");
	EFPRINTF(fp, "[0004]\t\tAsl Compiler Revision : 00000000\n");
	EFPRINTF(fp, "\n");

	/* Add in pointers to the MADT, FADT and HPET */
	EFPRINTF(fp, "[0004]\t\tACPI Table Address 0 : 00000000%08X\n",
	    basl_acpi_base + MADT_OFFSET);
	EFPRINTF(fp, "[0004]\t\tACPI Table Address 1 : 00000000%08X\n",
	    basl_acpi_base + FADT_OFFSET);
	EFPRINTF(fp, "[0004]\t\tACPI Table Address 2 : 00000000%08X\n",
	    basl_acpi_base + HPET_OFFSET);
	EFPRINTF(fp, "[0004]\t\tACPI Table Address 3 : 00000000%08X\n",
	    basl_acpi_base + MCFG_OFFSET);

	EFFLUSH(fp);

	return (0);

err_exit:
	return (errno);
}

static int
basl_fwrite_fadt(FILE *fp)
{
	EFPRINTF(fp, "/*\n");
	EFPRINTF(fp, " * bhyve FADT template\n");
	EFPRINTF(fp, " */\n");
	EFPRINTF(fp, "[0004]\t\tSignature : \"FACP\"\n");
	EFPRINTF(fp, "[0004]\t\tTable Length : 0000010C\n");
	EFPRINTF(fp, "[0001]\t\tRevision : 05\n");
	EFPRINTF(fp, "[0001]\t\tChecksum : 00\n");
	EFPRINTF(fp, "[0006]\t\tOem ID : \"BHYVE \"\n");
	EFPRINTF(fp, "[0008]\t\tOem Table ID : \"BVFACP  \"\n");
	EFPRINTF(fp, "[0004]\t\tOem Revision : 00000001\n");
	/* iasl will fill in the compiler ID/revision fields */
	EFPRINTF(fp, "[0004]\t\tAsl Compiler ID : \"xxxx\"\n");
	EFPRINTF(fp, "[0004]\t\tAsl Compiler Revision : 00000000\n");
	EFPRINTF(fp, "\n");

	EFPRINTF(fp, "[0004]\t\tFACS Address : %08X\n",
	    basl_acpi_base + FACS_OFFSET);
	EFPRINTF(fp, "[0004]\t\tDSDT Address : %08X\n",
	    basl_acpi_base + DSDT_OFFSET);
	EFPRINTF(fp, "[0001]\t\tModel : 01\n");
	EFPRINTF(fp, "[0001]\t\tPM Profile : 00 [Unspecified]\n");
	EFPRINTF(fp, "[0002]\t\tSCI Interrupt : %04X\n",
	    SCI_INT);
	EFPRINTF(fp, "[0004]\t\tSMI Command Port : %08X\n",
	    SMI_CMD);
	EFPRINTF(fp, "[0001]\t\tACPI Enable Value : %02X\n",
	    BHYVE_ACPI_ENABLE);
	EFPRINTF(fp, "[0001]\t\tACPI Disable Value : %02X\n",
	    BHYVE_ACPI_DISABLE);
	EFPRINTF(fp, "[0001]\t\tS4BIOS Command : 00\n");
	EFPRINTF(fp, "[0001]\t\tP-State Control : 00\n");
	EFPRINTF(fp, "[0004]\t\tPM1A Event Block Address : %08X\n",
	    PM1A_EVT_ADDR);
	EFPRINTF(fp, "[0004]\t\tPM1B Event Block Address : 00000000\n");
	EFPRINTF(fp, "[0004]\t\tPM1A Control Block Address : %08X\n",
	    PM1A_CNT_ADDR);
	EFPRINTF(fp, "[0004]\t\tPM1B Control Block Address : 00000000\n");
	EFPRINTF(fp, "[0004]\t\tPM2 Control Block Address : 00000000\n");
	EFPRINTF(fp, "[0004]\t\tPM Timer Block Address : %08X\n",
	    IO_PMTMR);
	EFPRINTF(fp, "[0004]\t\tGPE0 Block Address : %08X\n", IO_GPE0_BLK);
	EFPRINTF(fp, "[0004]\t\tGPE1 Block Address : 00000000\n");
	EFPRINTF(fp, "[0001]\t\tPM1 Event Block Length : 04\n");
	EFPRINTF(fp, "[0001]\t\tPM1 Control Block Length : 02\n");
	EFPRINTF(fp, "[0001]\t\tPM2 Control Block Length : 00\n");
	EFPRINTF(fp, "[0001]\t\tPM Timer Block Length : 04\n");
	EFPRINTF(fp, "[0001]\t\tGPE0 Block Length : %02x\n", IO_GPE0_LEN);
	EFPRINTF(fp, "[0001]\t\tGPE1 Block Length : 00\n");
	EFPRINTF(fp, "[0001]\t\tGPE1 Base Offset : 00\n");
	EFPRINTF(fp, "[0001]\t\t_CST Support : 00\n");
	EFPRINTF(fp, "[0002]\t\tC2 Latency : 0000\n");
	EFPRINTF(fp, "[0002]\t\tC3 Latency : 0000\n");
	EFPRINTF(fp, "[0002]\t\tCPU Cache Size : 0000\n");
	EFPRINTF(fp, "[0002]\t\tCache Flush Stride : 0000\n");
	EFPRINTF(fp, "[0001]\t\tDuty Cycle Offset : 00\n");
	EFPRINTF(fp, "[0001]\t\tDuty Cycle Width : 00\n");
	EFPRINTF(fp, "[0001]\t\tRTC Day Alarm Index : 00\n");
	EFPRINTF(fp, "[0001]\t\tRTC Month Alarm Index : 00\n");
	EFPRINTF(fp, "[0001]\t\tRTC Century Index : 32\n");
	EFPRINTF(fp, "[0002]\t\tBoot Flags (decoded below) : 0000\n");
	EFPRINTF(fp, "\t\t\tLegacy Devices Supported (V2) : 0\n");
	EFPRINTF(fp, "\t\t\t8042 Present on ports 60/64 (V2) : 0\n");
	EFPRINTF(fp, "\t\t\tVGA Not Present (V4) : 1\n");
	EFPRINTF(fp, "\t\t\tMSI Not Supported (V4) : 0\n");
	EFPRINTF(fp, "\t\t\tPCIe ASPM Not Supported (V4) : 1\n");
	EFPRINTF(fp, "\t\t\tCMOS RTC Not Present (V5) : 0\n");
	EFPRINTF(fp, "[0001]\t\tReserved : 00\n");
	EFPRINTF(fp, "[0004]\t\tFlags (decoded below) : 00000000\n");
	EFPRINTF(fp, "\t\t\tWBINVD instruction is operational (V1) : 1\n");
	EFPRINTF(fp, "\t\t\tWBINVD flushes all caches (V1) : 0\n");
	EFPRINTF(fp, "\t\t\tAll CPUs support C1 (V1) : 1\n");
	EFPRINTF(fp, "\t\t\tC2 works on MP system (V1) : 0\n");
	EFPRINTF(fp, "\t\t\tControl Method Power Button (V1) : 0\n");
	EFPRINTF(fp, "\t\t\tControl Method Sleep Button (V1) : 1\n");
	EFPRINTF(fp, "\t\t\tRTC wake not in fixed reg space (V1) : 0\n");
	EFPRINTF(fp, "\t\t\tRTC can wake system from S4 (V1) : 0\n");
	EFPRINTF(fp, "\t\t\t32-bit PM Timer (V1) : 1\n");
	EFPRINTF(fp, "\t\t\tDocking Supported (V1) : 0\n");
	EFPRINTF(fp, "\t\t\tReset Register Supported (V2) : 1\n");
	EFPRINTF(fp, "\t\t\tSealed Case (V3) : 0\n");
	EFPRINTF(fp, "\t\t\tHeadless - No Video (V3) : 1\n");
	EFPRINTF(fp, "\t\t\tUse native instr after SLP_TYPx (V3) : 0\n");
	EFPRINTF(fp, "\t\t\tPCIEXP_WAK Bits Supported (V4) : 0\n");
	EFPRINTF(fp, "\t\t\tUse Platform Timer (V4) : 0\n");
	EFPRINTF(fp, "\t\t\tRTC_STS valid on S4 wake (V4) : 0\n");
	EFPRINTF(fp, "\t\t\tRemote Power-on capable (V4) : 0\n");
	EFPRINTF(fp, "\t\t\tUse APIC Cluster Model (V4) : 0\n");
	EFPRINTF(fp, "\t\t\tUse APIC Physical Destination Mode (V4) : 1\n");
	EFPRINTF(fp, "\t\t\tHardware Reduced (V5) : 0\n");
	EFPRINTF(fp, "\t\t\tLow Power S0 Idle (V5) : 0\n");
	EFPRINTF(fp, "\n");

	EFPRINTF(fp,
	    "[0012]\t\tReset Register : [Generic Address Structure]\n");
	EFPRINTF(fp, "[0001]\t\tSpace ID : 01 [SystemIO]\n");
	EFPRINTF(fp, "[0001]\t\tBit Width : 08\n");
	EFPRINTF(fp, "[0001]\t\tBit Offset : 00\n");
	EFPRINTF(fp, "[0001]\t\tEncoded Access Width : 01 [Byte Access:8]\n");
	EFPRINTF(fp, "[0008]\t\tAddress : 0000000000000CF9\n");
	EFPRINTF(fp, "\n");

	EFPRINTF(fp, "[0001]\t\tValue to cause reset : 06\n");
	EFPRINTF(fp, "[0002]\t\tARM Flags (decoded below): 0000\n");
	EFPRINTF(fp, "\t\t\tPSCI Compliant : 0\n");
	EFPRINTF(fp, "\t\t\tMust use HVC for PSCI : 0\n");
	EFPRINTF(fp, "[0001]\t\tFADT Minor Revision : 01\n");
	EFPRINTF(fp, "[0008]\t\tFACS Address : 00000000%08X\n",
	    basl_acpi_base + FACS_OFFSET);
	EFPRINTF(fp, "[0008]\t\tDSDT Address : 00000000%08X\n",
	    basl_acpi_base + DSDT_OFFSET);
	EFPRINTF(fp,
	    "[0012]\t\tPM1A Event Block : [Generic Address Structure]\n");
	EFPRINTF(fp, "[0001]\t\tSpace ID : 01 [SystemIO]\n");
	EFPRINTF(fp, "[0001]\t\tBit Width : 20\n");
	EFPRINTF(fp, "[0001]\t\tBit Offset : 00\n");
	EFPRINTF(fp, "[0001]\t\tEncoded Access Width : 02 [Word Access:16]\n");
	EFPRINTF(fp, "[0008]\t\tAddress : 00000000%08X\n",
	    PM1A_EVT_ADDR);
	EFPRINTF(fp, "\n");

	EFPRINTF(fp,
	    "[0012]\t\tPM1B Event Block : [Generic Address Structure]\n");
	EFPRINTF(fp, "[0001]\t\tSpace ID : 01 [SystemIO]\n");
	EFPRINTF(fp, "[0001]\t\tBit Width : 00\n");
	EFPRINTF(fp, "[0001]\t\tBit Offset : 00\n");
	EFPRINTF(fp,
	    "[0001]\t\tEncoded Access Width : 00 [Undefined/Legacy]\n");
	EFPRINTF(fp, "[0008]\t\tAddress : 0000000000000000\n");
	EFPRINTF(fp, "\n");

	EFPRINTF(fp,
	    "[0012]\t\tPM1A Control Block : [Generic Address Structure]\n");
	EFPRINTF(fp, "[0001]\t\tSpace ID : 01 [SystemIO]\n");
	EFPRINTF(fp, "[0001]\t\tBit Width : 10\n");
	EFPRINTF(fp, "[0001]\t\tBit Offset : 00\n");
	EFPRINTF(fp, "[0001]\t\tEncoded Access Width : 02 [Word Access:16]\n");
	EFPRINTF(fp, "[0008]\t\tAddress : 00000000%08X\n",
	    PM1A_CNT_ADDR);
	EFPRINTF(fp, "\n");

	EFPRINTF(fp,
	    "[0012]\t\tPM1B Control Block : [Generic Address Structure]\n");
	EFPRINTF(fp, "[0001]\t\tSpace ID : 01 [SystemIO]\n");
	EFPRINTF(fp, "[0001]\t\tBit Width : 00\n");
	EFPRINTF(fp, "[0001]\t\tBit Offset : 00\n");
	EFPRINTF(fp,
	    "[0001]\t\tEncoded Access Width : 00 [Undefined/Legacy]\n");
	EFPRINTF(fp, "[0008]\t\tAddress : 0000000000000000\n");
	EFPRINTF(fp, "\n");

	EFPRINTF(fp,
	    "[0012]\t\tPM2 Control Block : [Generic Address Structure]\n");
	EFPRINTF(fp, "[0001]\t\tSpace ID : 01 [SystemIO]\n");
	EFPRINTF(fp, "[0001]\t\tBit Width : 08\n");
	EFPRINTF(fp, "[0001]\t\tBit Offset : 00\n");
	EFPRINTF(fp,
	    "[0001]\t\tEncoded Access Width : 00 [Undefined/Legacy]\n");
	EFPRINTF(fp, "[0008]\t\tAddress : 0000000000000000\n");
	EFPRINTF(fp, "\n");

	/* Valid for bhyve */
	EFPRINTF(fp,
	    "[0012]\t\tPM Timer Block : [Generic Address Structure]\n");
	EFPRINTF(fp, "[0001]\t\tSpace ID : 01 [SystemIO]\n");
	EFPRINTF(fp, "[0001]\t\tBit Width : 20\n");
	EFPRINTF(fp, "[0001]\t\tBit Offset : 00\n");
	EFPRINTF(fp,
	    "[0001]\t\tEncoded Access Width : 03 [DWord Access:32]\n");
	EFPRINTF(fp, "[0008]\t\tAddress : 00000000%08X\n",
	    IO_PMTMR);
	EFPRINTF(fp, "\n");

	EFPRINTF(fp, "[0012]\t\tGPE0 Block : [Generic Address Structure]\n");
	EFPRINTF(fp, "[0001]\t\tSpace ID : 01 [SystemIO]\n");
	EFPRINTF(fp, "[0001]\t\tBit Width : %02x\n", IO_GPE0_LEN * 8);
	EFPRINTF(fp, "[0001]\t\tBit Offset : 00\n");
	EFPRINTF(fp, "[0001]\t\tEncoded Access Width : 01 [Byte Access:8]\n");
	EFPRINTF(fp, "[0008]\t\tAddress : %016X\n", IO_GPE0_BLK);
	EFPRINTF(fp, "\n");

	EFPRINTF(fp, "[0012]\t\tGPE1 Block : [Generic Address Structure]\n");
	EFPRINTF(fp, "[0001]\t\tSpace ID : 01 [SystemIO]\n");
	EFPRINTF(fp, "[0001]\t\tBit Width : 00\n");
	EFPRINTF(fp, "[0001]\t\tBit Offset : 00\n");
	EFPRINTF(fp,
	    "[0001]\t\tEncoded Access Width : 00 [Undefined/Legacy]\n");
	EFPRINTF(fp, "[0008]\t\tAddress : 0000000000000000\n");
	EFPRINTF(fp, "\n");

	EFPRINTF(fp,
	   "[0012]\t\tSleep Control Register : [Generic Address Structure]\n");
	EFPRINTF(fp, "[0001]\t\tSpace ID : 01 [SystemIO]\n");
	EFPRINTF(fp, "[0001]\t\tBit Width : 08\n");
	EFPRINTF(fp, "[0001]\t\tBit Offset : 00\n");
	EFPRINTF(fp, "[0001]\t\tEncoded Access Width : 01 [Byte Access:8]\n");
	EFPRINTF(fp, "[0008]\t\tAddress : 0000000000000000\n");
	EFPRINTF(fp, "\n");

	EFPRINTF(fp,
	    "[0012]\t\tSleep Status Register : [Generic Address Structure]\n");
	EFPRINTF(fp, "[0001]\t\tSpace ID : 01 [SystemIO]\n");
	EFPRINTF(fp, "[0001]\t\tBit Width : 08\n");
	EFPRINTF(fp, "[0001]\t\tBit Offset : 00\n");
	EFPRINTF(fp, "[0001]\t\tEncoded Access Width : 01 [Byte Access:8]\n");
	EFPRINTF(fp, "[0008]\t\tAddress : 0000000000000000\n");

	EFFLUSH(fp);

	return (0);

err_exit:
	return (errno);
}

/*
 * Helper routines for writing to the DSDT from other modules.
 */
void
dsdt_line(const char *fmt, ...)
{
	va_list ap;

	if (dsdt_error != 0)
		return;

	if (strcmp(fmt, "") != 0) {
		if (dsdt_indent_level != 0)
			EFPRINTF(dsdt_fp, "%*c", dsdt_indent_level * 2, ' ');
		va_start(ap, fmt);
		if (vfprintf(dsdt_fp, fmt, ap) < 0) {
			va_end(ap);
			goto err_exit;
		}
		va_end(ap);
	}
	EFPRINTF(dsdt_fp, "\n");
	return;

err_exit:
	dsdt_error = errno;
}

void
dsdt_indent(int levels)
{

	dsdt_indent_level += levels;
	assert(dsdt_indent_level >= 0);
}

void
dsdt_unindent(int levels)
{

	assert(dsdt_indent_level >= levels);
	dsdt_indent_level -= levels;
}

void
dsdt_fixed_ioport(uint16_t iobase, uint16_t length)
{

	dsdt_line("IO (Decode16,");
	dsdt_line("  0x%04X,             // Range Minimum", iobase);
	dsdt_line("  0x%04X,             // Range Maximum", iobase);
	dsdt_line("  0x01,               // Alignment");
	dsdt_line("  0x%02X,               // Length", length);
	dsdt_line("  )");
}

void
dsdt_fixed_irq(uint8_t irq)
{

	dsdt_line("IRQNoFlags ()");
	dsdt_line("  {%d}", irq);
}

void
dsdt_fixed_mem32(uint32_t base, uint32_t length)
{

	dsdt_line("Memory32Fixed (ReadWrite,");
	dsdt_line("  0x%08X,         // Address Base", base);
	dsdt_line("  0x%08X,         // Address Length", length);
	dsdt_line("  )");
}

static int
basl_fwrite_dsdt(FILE *fp)
{
	dsdt_fp = fp;
	dsdt_error = 0;
	dsdt_indent_level = 0;

	dsdt_line("/*");
	dsdt_line(" * bhyve DSDT template");
	dsdt_line(" */");
	dsdt_line("DefinitionBlock (\"bhyve_dsdt.aml\", \"DSDT\", 2,"
		 "\"BHYVE \", \"BVDSDT  \", 0x00000001)");
	dsdt_line("{");
	dsdt_line("  Name (_S5, Package ()");
	dsdt_line("  {");
	dsdt_line("      0x05,");
	dsdt_line("      Zero,");
	dsdt_line("  })");

	pci_write_dsdt();

	dsdt_line("");
	dsdt_line("  Scope (_SB.PC00)");
	dsdt_line("  {");
	dsdt_line("    Device (HPET)");
	dsdt_line("    {");
	dsdt_line("      Name (_HID, EISAID(\"PNP0103\"))");
	dsdt_line("      Name (_UID, 0)");
	dsdt_line("      Name (_CRS, ResourceTemplate ()");
	dsdt_line("      {");
	dsdt_indent(4);
	dsdt_fixed_mem32(0xFED00000, 0x400);
	dsdt_unindent(4);
	dsdt_line("      })");
	dsdt_line("    }");
	dsdt_line("  }");

	vmgenc_write_dsdt();

	dsdt_line("}");

	if (dsdt_error != 0)
		return (dsdt_error);

	EFFLUSH(fp);

	return (0);

err_exit:
	return (errno);
}

static int
basl_open(struct basl_fio *bf, int suffix)
{
	int err;

	err = 0;

	if (suffix) {
		strlcpy(bf->f_name, basl_stemplate, MAXPATHLEN);
		bf->fd = mkstemps(bf->f_name, strlen(BHYVE_ASL_SUFFIX));
	} else {
		strlcpy(bf->f_name, basl_template, MAXPATHLEN);
		bf->fd = mkstemp(bf->f_name);
	}

	if (bf->fd > 0) {
		bf->fp = fdopen(bf->fd, "w+");
		if (bf->fp == NULL) {
			unlink(bf->f_name);
			close(bf->fd);
		}
	} else {
		err = 1;
	}

	return (err);
}

static void
basl_close(struct basl_fio *bf)
{

	if (!basl_keep_temps)
		unlink(bf->f_name);
	fclose(bf->fp);
}

static int
basl_start(struct basl_fio *in, struct basl_fio *out)
{
	int err;

	err = basl_open(in, 0);
	if (!err) {
		err = basl_open(out, 1);
		if (err) {
			basl_close(in);
		}
	}

	return (err);
}

static void
basl_end(struct basl_fio *in, struct basl_fio *out)
{

	basl_close(in);
	basl_close(out);
}

static int
basl_load(struct vmctx *ctx, int fd, uint64_t off)
{
	struct stat sb;
	void *addr;

	if (fstat(fd, &sb) < 0)
		return (errno);

	addr = calloc(1, sb.st_size);
	if (addr == NULL)
		return (EFAULT);

	if (read(fd, addr, sb.st_size) < 0)
		return (errno);

	struct basl_table *table;

	uint8_t name[ACPI_NAMESEG_SIZE + 1] = { 0 };
	memcpy(name, addr, sizeof(name) - 1 /* last char is '\0' */);
	BASL_EXEC(
	    basl_table_create(&table, ctx, name, BASL_TABLE_ALIGNMENT, off));
	BASL_EXEC(basl_table_append_bytes(table, addr, sb.st_size));

	return (0);
}

static int
basl_compile(struct vmctx *ctx, int (*fwrite_section)(FILE *), uint64_t offset)
{
	struct basl_fio io[2];
	static char iaslbuf[3*MAXPATHLEN + 10];
	const char *fmt;
	int err;

	err = basl_start(&io[0], &io[1]);
	if (!err) {
		err = (*fwrite_section)(io[0].fp);

		if (!err) {
			/*
			 * iasl sends the results of the compilation to
			 * stdout. Shut this down by using the shell to
			 * redirect stdout to /dev/null, unless the user
			 * has requested verbose output for debugging
			 * purposes
			 */
			fmt = basl_verbose_iasl ?
				"%s -p %s %s" :
				"/bin/sh -c \"%s -p %s %s\" 1> /dev/null";

			snprintf(iaslbuf, sizeof(iaslbuf),
				 fmt,
				 BHYVE_ASL_COMPILER,
				 io[1].f_name, io[0].f_name);
			err = system(iaslbuf);

			if (!err) {
				/*
				 * Copy the aml output file into guest
				 * memory at the specified location
				 */
				err = basl_load(ctx, io[1].fd, offset);
			}
		}
		basl_end(&io[0], &io[1]);
	}

	return (err);
}

static int
basl_make_templates(void)
{
	const char *tmpdir;
	int err;
	int len;

	err = 0;

	/*
	 *
	 */
	if ((tmpdir = getenv("BHYVE_TMPDIR")) == NULL || *tmpdir == '\0' ||
	    (tmpdir = getenv("TMPDIR")) == NULL || *tmpdir == '\0') {
		tmpdir = _PATH_TMP;
	}

	len = strlen(tmpdir);

	if ((len + sizeof(BHYVE_ASL_TEMPLATE) + 1) < MAXPATHLEN) {
		strcpy(basl_template, tmpdir);
		while (len > 0 && basl_template[len - 1] == '/')
			len--;
		basl_template[len] = '/';
		strcpy(&basl_template[len + 1], BHYVE_ASL_TEMPLATE);
	} else
		err = E2BIG;

	if (!err) {
		/*
		 * len has been intialized (and maybe adjusted) above
		 */
		if ((len + sizeof(BHYVE_ASL_TEMPLATE) + 1 +
		     sizeof(BHYVE_ASL_SUFFIX)) < MAXPATHLEN) {
			strcpy(basl_stemplate, tmpdir);
			basl_stemplate[len] = '/';
			strcpy(&basl_stemplate[len + 1], BHYVE_ASL_TEMPLATE);
			len = strlen(basl_stemplate);
			strcpy(&basl_stemplate[len], BHYVE_ASL_SUFFIX);
		} else
			err = E2BIG;
	}

	return (err);
}

static int
build_dsdt(struct vmctx *const ctx)
{
	BASL_EXEC(basl_compile(ctx, basl_fwrite_dsdt, DSDT_OFFSET));

	return (0);
}

static int
build_facs(struct vmctx *const ctx)
{
	ACPI_TABLE_FACS facs;
	struct basl_table *table;
	BASL_EXEC(basl_table_create(&table, ctx, ACPI_SIG_FACS,
	    BASL_TABLE_ALIGNMENT_FACS, FACS_OFFSET));

	memset(&facs, 0, sizeof(facs));
	memcpy(facs.Signature, ACPI_SIG_FACS, ACPI_NAMESEG_SIZE);
	facs.Length = sizeof(facs);
	facs.Version = htole32(2);
	BASL_EXEC(basl_table_append_bytes(table, &facs, sizeof(facs)));

	return (0);
}

static int
build_hpet(struct vmctx *const ctx)
{
	ACPI_TABLE_HPET hpet;
	struct basl_table *table;

	BASL_EXEC(basl_table_create(&table, ctx, ACPI_SIG_HPET,
	    BASL_TABLE_ALIGNMENT, HPET_OFFSET));

	memset(&hpet, 0, sizeof(hpet));
	BASL_EXEC(basl_table_append_header(table, ACPI_SIG_HPET, 1, 1));
	hpet.Id = htole32(hpet_capabilities);
	basl_fill_gas(&hpet.Address, ACPI_ADR_SPACE_SYSTEM_MEMORY, 0, 0,
	    ACPI_GAS_ACCESS_WIDTH_LEGACY, BHYVE_ADDRESS_HPET);
	hpet.Flags = ACPI_HPET_PAGE_PROTECT4;
	BASL_EXEC(basl_table_append_content(table, &hpet, sizeof(hpet)));

	return (0);
}

static int
build_madt(struct vmctx *const ctx)
{
	ACPI_TABLE_MADT madt;
	ACPI_MADT_LOCAL_APIC madt_lapic;
	ACPI_MADT_IO_APIC madt_ioapic;
	ACPI_MADT_INTERRUPT_OVERRIDE madt_irq_override;
	ACPI_MADT_LOCAL_APIC_NMI madt_lapic_nmi;
	struct basl_table *table;

	BASL_EXEC(basl_table_create(&table, ctx, ACPI_SIG_MADT,
	    BASL_TABLE_ALIGNMENT, MADT_OFFSET));

	memset(&madt, 0, sizeof(madt));
	BASL_EXEC(basl_table_append_header(table, ACPI_SIG_MADT, 1, 1));
	madt.Address = htole32(BHYVE_ADDRESS_LAPIC);
	madt.Flags = htole32(ACPI_MADT_PCAT_COMPAT);
	BASL_EXEC(basl_table_append_content(table, &madt, sizeof(madt)));

	/* Local APIC for each CPU */
	for (int i = 0; i < basl_ncpu; ++i) {
		memset(&madt_lapic, 0, sizeof(madt_lapic));
		madt_lapic.Header.Type = ACPI_MADT_TYPE_LOCAL_APIC;
		madt_lapic.Header.Length = sizeof(madt_lapic);
		madt_lapic.ProcessorId = i;
		madt_lapic.Id = i;
		madt_lapic.LapicFlags = htole32(ACPI_MADT_ENABLED);
		BASL_EXEC(basl_table_append_bytes(table, &madt_lapic,
		    sizeof(madt_lapic)));
	}

	/* I/O APIC */
	memset(&madt_ioapic, 0, sizeof(madt_ioapic));
	madt_ioapic.Header.Type = ACPI_MADT_TYPE_IO_APIC;
	madt_ioapic.Header.Length = sizeof(madt_ioapic);
	madt_ioapic.Address = htole32(BHYVE_ADDRESS_IOAPIC);
	BASL_EXEC(
	    basl_table_append_bytes(table, &madt_ioapic, sizeof(madt_ioapic)));

	/* Legacy IRQ0 is connected to pin 2 of the I/O APIC */
	memset(&madt_irq_override, 0, sizeof(madt_irq_override));
	madt_irq_override.Header.Type = ACPI_MADT_TYPE_INTERRUPT_OVERRIDE;
	madt_irq_override.Header.Length = sizeof(madt_irq_override);
	madt_irq_override.GlobalIrq = htole32(2);
	madt_irq_override.IntiFlags = htole16(
	    ACPI_MADT_POLARITY_ACTIVE_HIGH | ACPI_MADT_TRIGGER_EDGE);
	BASL_EXEC(basl_table_append_bytes(table, &madt_irq_override,
	    sizeof(madt_irq_override)));

	memset(&madt_irq_override, 0, sizeof(madt_irq_override));
	madt_irq_override.Header.Type = ACPI_MADT_TYPE_INTERRUPT_OVERRIDE;
	madt_irq_override.Header.Length = sizeof(madt_irq_override);
	madt_irq_override.SourceIrq = SCI_INT;
	madt_irq_override.GlobalIrq = htole32(SCI_INT);
	madt_irq_override.IntiFlags = htole16(
	    ACPI_MADT_POLARITY_ACTIVE_LOW | ACPI_MADT_TRIGGER_LEVEL);
	BASL_EXEC(basl_table_append_bytes(table, &madt_irq_override,
	    sizeof(madt_irq_override)));

	/* Local APIC NMI is conntected to LINT 1 on all CPUs */
	memset(&madt_lapic_nmi, 0, sizeof(madt_lapic_nmi));
	madt_lapic_nmi.Header.Type = ACPI_MADT_TYPE_LOCAL_APIC_NMI;
	madt_lapic_nmi.Header.Length = sizeof(madt_lapic_nmi);
	madt_lapic_nmi.ProcessorId = 0xFF;
	madt_lapic_nmi.IntiFlags = htole16(
	    ACPI_MADT_POLARITY_ACTIVE_HIGH | ACPI_MADT_TRIGGER_EDGE);
	madt_lapic_nmi.Lint = 1;
	BASL_EXEC(basl_table_append_bytes(table, &madt_lapic_nmi,
	    sizeof(madt_lapic_nmi)));

	return (0);
}

static int
build_mcfg(struct vmctx *const ctx)
{
	ACPI_TABLE_MCFG mcfg;
	ACPI_MCFG_ALLOCATION mcfg_allocation;
	struct basl_table *table;

	BASL_EXEC(basl_table_create(&table, ctx, ACPI_SIG_MCFG,
	    BASL_TABLE_ALIGNMENT, MCFG_OFFSET));

	memset(&mcfg, 0, sizeof(mcfg));
	BASL_EXEC(basl_table_append_header(table, ACPI_SIG_MCFG, 1, 1));
	BASL_EXEC(basl_table_append_content(table, &mcfg, sizeof(mcfg)));

	memset(&mcfg_allocation, 0, sizeof(mcfg_allocation));
	mcfg_allocation.Address = htole64(pci_ecfg_base());
	mcfg_allocation.EndBusNumber = 0xFF;
	BASL_EXEC(basl_table_append_bytes(table, &mcfg_allocation,
	    sizeof(mcfg_allocation)));

	return (0);
}

int
acpi_build(struct vmctx *ctx, int ncpu)
{
	int err;

	basl_ncpu = ncpu;

	err = vm_get_hpet_capabilities(ctx, &hpet_capabilities);
	if (err != 0)
		return (err);

	/*
	 * For debug, allow the user to have iasl compiler output sent
	 * to stdout rather than /dev/null
	 */
	if (getenv("BHYVE_ACPI_VERBOSE_IASL"))
		basl_verbose_iasl = 1;

	/*
	 * Allow the user to keep the generated ASL files for debugging
	 * instead of deleting them following use
	 */
	if (getenv("BHYVE_ACPI_KEEPTMPS"))
		basl_keep_temps = 1;

	BASL_EXEC(basl_init());

	BASL_EXEC(basl_make_templates());

	/*
	 * Run through all the ASL files, compiling them and
	 * copying them into guest memory
	 */
	BASL_EXEC(basl_compile(ctx, basl_fwrite_rsdp, 0));
	BASL_EXEC(basl_compile(ctx, basl_fwrite_rsdt, RSDT_OFFSET));
	BASL_EXEC(basl_compile(ctx, basl_fwrite_xsdt, XSDT_OFFSET));
	BASL_EXEC(basl_compile(ctx, basl_fwrite_fadt, FADT_OFFSET));
	BASL_EXEC(build_madt(ctx));
	BASL_EXEC(build_hpet(ctx));
	BASL_EXEC(build_mcfg(ctx));
	BASL_EXEC(build_facs(ctx));
	BASL_EXEC(build_dsdt(ctx));

	BASL_EXEC(basl_finish());

	return (0);
}
