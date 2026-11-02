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
 * Copyright 2020 Joyent, Inc.
 * Copyright 2026 Oxide Computer Company
 */

#include <sys/tsc.h>
#include <sys/prom_debug.h>
#include <sys/clock.h>
#include <sys/archsystm.h>
#include <sys/sysmacros.h>
#include <sys/cmn_err.h>
#include <sys/io/fch/hpet.h>
#include <sys/io/fch/pmio.h>

/*
 * TSC calibration against the FCH's HPET. There is no ACPI on this
 * architecture so we find the HPET at its fixed address and, if nothing has
 * done so already, enable its decoding and its main counter ourselves. The
 * counter is left running afterwards. None of the HPET's timers or interrupts
 * are touched.
 *
 * The measurement interval, in microseconds, is tunable. The HPET runs at
 * 14.318MHz on all supported FCHs, so its quantisation alone contributes
 * about 70ns of error over the interval, or 1.4ppm at the default of 50ms.
 * Each HPET read is a slow uncached access to the FCH, so each end of the
 * interval is sampled several times between a pair of TSC reads. The sample
 * with the narrowest pair is used, and its TSC timestamp is the midpoint of
 * the pair.
 */
uint32_t tscc_hpet_interval_us = 50000;

#define	TSCC_HPET_INTERVAL_MIN_US	1000U
#define	TSCC_HPET_INTERVAL_MAX_US	1000000U
#define	TSCC_HPET_SAMPLE_TRIES		8

/*
 * Results of the most recent HPET calibration, for diagnostic purposes.
 */
uint64_t hpet_tsc_hz;
uint32_t hpet_period_fs;
uint64_t hpet_tsc_ticks;
uint32_t hpet_ticks;

static void
tscc_hpet_sample(const mmio_reg_t ctr, uint32_t *hpetp, uint64_t *tscp)
{
	uint64_t best = UINT64_MAX;

	for (uint_t i = 0; i < TSCC_HPET_SAMPLE_TRIES; i++) {
		const uint64_t before = tsc_read();
		const uint32_t hpet = (uint32_t)mmio_reg_read(ctr);
		const uint64_t after = tsc_read();

		if (after - before < best) {
			best = after - before;
			*hpetp = hpet;
			*tscp = before + best / 2;
		}
	}
}

static boolean_t
tsc_calibrate_hpet(uint64_t *freqp)
{
	mmio_reg_block_t block;
	mmio_reg_t reg;
	uint32_t val;

	PRM_POINT("Attempting to use HPET for TSC calibration...");

	block = fch_pmio_mmio_block();
	reg = FCH_PMIO_DECODEEN_MMIO(block);
	val = mmio_reg_read(reg);
	if (FCH_PMIO_DECODEEN_GET_HPETEN(val) == 0) {
		PRM_POINT("Enabling HPET decoding");
		val = FCH_PMIO_DECODEEN_SET_HPETEN(val, 1);
		mmio_reg_write(reg, val);
	}
	mmio_reg_block_unmap(&block);

	block = fch_hpet_mmio_block();

	val = mmio_reg_read(FCH_HPET_GCAP_ID_MMIO(block));
	const uint32_t period = mmio_reg_read(FCH_HPET_CLK_PERIOD_MMIO(block));
	PRM_DEBUG(val);
	PRM_DEBUG(period);
	if (val == UINT32_MAX || period == 0 ||
	    period > FCH_HPET_CLK_PERIOD_MAX_FS) {
		PRM_POINT("HPET is absent or reports an invalid period");
		mmio_reg_block_unmap(&block);
		return (B_FALSE);
	}

	reg = FCH_HPET_GEN_CONF_MMIO(block);
	val = mmio_reg_read(reg);
	if (FCH_HPET_GEN_CONF_GET_ENABLE_CNF(val) == 0) {
		PRM_POINT("Enabling HPET main counter");
		val = FCH_HPET_GEN_CONF_SET_ENABLE_CNF(val, 1);
		mmio_reg_write(reg, val);
	}

	const uint64_t interval_us = MAX(MIN(tscc_hpet_interval_us,
	    TSCC_HPET_INTERVAL_MAX_US), TSCC_HPET_INTERVAL_MIN_US);
	const uint32_t target = (uint32_t)(interval_us * 1000000000ULL /
	    period);

	/*
	 * If the counter does not advance we give up once the TSC has counted
	 * for as long as the interval would take at 10GHz, which is at least
	 * as long as the interval itself on any machine we will see.
	 */
	const uint64_t tsc_limit = interval_us * 10000;

	const mmio_reg_t ctr = FCH_HPET_MAIN_COUNTER_MMIO(block);
	uint32_t hpet_start, hpet_end;
	uint64_t tsc_start, tsc_end;
	boolean_t ok = B_TRUE;

	const ulong_t flags = clear_int_flag();

	/* Warm up the caches and the TLB. */
	tscc_hpet_sample(ctr, &hpet_start, &tsc_start);

	tscc_hpet_sample(ctr, &hpet_start, &tsc_start);
	do {
		if (tsc_read() - tsc_start > tsc_limit) {
			ok = B_FALSE;
			break;
		}
	} while ((uint32_t)mmio_reg_read(ctr) - hpet_start < target);
	tscc_hpet_sample(ctr, &hpet_end, &tsc_end);

	restore_int_flag(flags);
	mmio_reg_block_unmap(&block);

	if (!ok) {
		PRM_POINT("HPET main counter is not advancing");
		return (B_FALSE);
	}

	const uint64_t tsc_ticks = tsc_end - tsc_start;
	const uint32_t ticks = hpet_end - hpet_start;
	const uint64_t hpet_ns = (uint64_t)ticks * period / 1000000ULL;

	if (tsc_ticks == 0 || hpet_ns == 0 ||
	    tsc_ticks > UINT64_MAX / NANOSEC) {
		return (B_FALSE);
	}

	hpet_period_fs = period;
	hpet_tsc_ticks = tsc_ticks;
	hpet_ticks = ticks;
	*freqp = hpet_tsc_hz = tsc_ticks * NANOSEC / hpet_ns;

	PRM_POINT("HPET calibration complete");
	PRM_DEBUG(*freqp);

	cmn_err(CE_CONT, "?TSC calibration: %lu tsc ticks, %u hpet ticks, "
	    "%lu hpet ns\n", tsc_ticks, ticks, hpet_ns);

	return (B_TRUE);
}

/*
 * For now this is tried only if the PIT is unusable.
 */
static tsc_calibrate_t tsc_calibration_hpet = {
	.tscc_source = "HPET",
	.tscc_preference = 1,
	.tscc_calibrate = tsc_calibrate_hpet,
};
TSC_CALIBRATION_SOURCE(tsc_calibration_hpet);
