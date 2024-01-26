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
 * Copyright 2021 Oxide Computer Company
 */

#include "ena_hw.h"
#include "ena.h"

uint32_t
ena_hw_bar_read32(const ena_t *ena, const uint16_t offset)
{
	caddr_t addr = ena->ena_reg_base + offset;
	return (ena_hw_abs_read32(ena, (uint32_t *)addr));
}

uint32_t
ena_hw_abs_read32(const ena_t *ena, uint32_t *addr)
{
	VERIFY3U(addr, >=, ena->ena_reg_base);
	VERIFY3U(addr, <, ena->ena_reg_base + (ena->ena_reg_size - 4));

	return (ddi_get32(ena->ena_reg_hdl, addr));
}

void
ena_hw_bar_write32(const ena_t *ena, const uint16_t offset, const uint32_t val)
{
	caddr_t addr = ena->ena_reg_base + offset;
	ena_hw_abs_write32(ena, (uint32_t *)addr, val);
}

void
ena_hw_abs_write32(const ena_t *ena, uint32_t *addr, const uint32_t val)
{
	VERIFY3P(ena, !=, NULL);
	VERIFY3P(addr, !=, NULL);
	VERIFY3U(addr, >=, ena->ena_reg_base);
	VERIFY3U(addr, <, ena->ena_reg_base + (ena->ena_reg_size - 4));

	ddi_put32(ena->ena_reg_hdl, addr, val);
}

int
enahw_resp_status_to_errno(ena_t *ena, enahw_resp_status_t status)
{
	int ret = 0;

	switch (status) {
	case ENAHW_RESP_SUCCESS:
		break;

	case ENAHW_RESP_RESOURCE_ALLOCATION_FAILURE:
		ret = ENOMEM;
		break;

	case ENAHW_RESP_UNSUPPORTED_OPCODE:
		ret = ENOTSUP;
		break;

	case ENAHW_RESP_BAD_OPCODE:
	case ENAHW_RESP_MALFORMED_REQUEST:
	case ENAHW_RESP_ILLEGAL_PARAMETER:
		ret = EINVAL;
		break;

	case ENAHW_RESP_RESOURCE_BUSY:
		ret = EAGAIN;
		break;

	case ENAHW_RESP_UNKNOWN_ERROR:
	default:
		/*
		 * If the device presents us with an "unknwon error"
		 * code, or the status code is undefined, then we log
		 * an error and convert it to EIO.
		 */
		ena_err(ena, "unexpected status code: %d", status);
		ret = EIO;
		break;
	}

	return (ret);
}

// XXX does not work with more than one ena device, just here for
// testing and experimentation.
enahw_reg_nv_t enahw_reg_cache[ENAHW_NUM_REGS] = {
	{ .ern_name = "Version", .ern_offset = ENAHW_REG_VERSION },
	{
		.ern_name = "Controller Version",
		.ern_offset = ENAHW_REG_CONTROLLER_VERSION
	},
	{ .ern_name = "Caps", .ern_offset = ENAHW_REG_CAPS },
	{ .ern_name = "Extended Caps", .ern_offset = ENAHW_REG_CAPS_EXT },
	{
		.ern_name = "Admin SQ Base Low",
		.ern_offset = ENAHW_REG_ASQ_BASE_LO
	},
	{
		.ern_name = "Admin SQ Base High",
		.ern_offset = ENAHW_REG_ASQ_BASE_HI
	},
	{ .ern_name = "Admin SQ Caps", .ern_offset = ENAHW_REG_ASQ_CAPS },
	{ .ern_name = "Gap 0x1C", .ern_offset = ENAHW_REG_GAP_1C },
	{
		.ern_name = "Admin CQ Base Low",
		.ern_offset = ENAHW_REG_ACQ_BASE_LO
	},
	{
		.ern_name = "Admin CQ Base High",
		.ern_offset = ENAHW_REG_ACQ_BASE_HI
	},
	{ .ern_name = "Admin CQ Caps", .ern_offset = ENAHW_REG_ACQ_CAPS },
	{ .ern_name = "Admin SQ Doorbell", .ern_offset = ENAHW_REG_ASQ_DB },
	{ .ern_name = "Admin CQ Tail", .ern_offset = ENAHW_REG_ACQ_TAIL },
	{
		.ern_name = "Admin Event Notification Queue Caps",
		.ern_offset = ENAHW_REG_AENQ_CAPS
	},
	{
		.ern_name = "Admin Event Notification Queue Base Low",
		.ern_offset = ENAHW_REG_AENQ_BASE_LO
	},
	{
		.ern_name = "Admin Event Notification Queue Base High",
		.ern_offset = ENAHW_REG_AENQ_BASE_HI
	},
	{
		.ern_name = "Admin Event Notification Queue Head Doorbell",
		.ern_offset = ENAHW_REG_AENQ_HEAD_DB
	},
	{
		.ern_name = "Admin Event Notification Queue Tail",
		.ern_offset = ENAHW_REG_AENQ_TAIL
	},
	{ .ern_name = "Gap 0x48", .ern_offset = ENAHW_REG_GAP_48 },
	{
		.ern_name = "Interrupt Mask (disable interrupts)",
		.ern_offset = ENAHW_REG_INTERRUPT_MASK
	},
	{ .ern_name = "Gap 0x50", .ern_offset = ENAHW_REG_GAP_50 },
	{ .ern_name = "Device Control", .ern_offset = ENAHW_REG_DEV_CTL },
	{ .ern_name = "Device Status", .ern_offset = ENAHW_REG_DEV_STS },
	{
		.ern_name = "MMIO Register Read",
		.ern_offset = ENAHW_REG_MMIO_REG_READ
	},
	{
		.ern_name = "MMIO Response Address Low",
		.ern_offset = ENAHW_REG_MMIO_RESP_LO
	},
	{
		.ern_name = "MMIO Response Address High",
		.ern_offset = ENAHW_REG_MMIO_RESP_HI
	},
	{
		.ern_name = "RSS Indirection Entry Update",
		.ern_offset = ENAHW_REG_RSS_IND_ENTRY_UPDATE
	}
};

boolean_t
enahw_check_status(const ena_t *ena)
{
	if (ena->ena_reg_base == 0)
		return (B_TRUE);
	uint32_t status = ena_hw_bar_read32(ena, ENAHW_REG_DEV_STS);
	if ((status & ENAHW_DEV_STS_FATAL_ERROR_MASK) != 0)
		return (B_FALSE);
	return (B_TRUE);
}

void
enahw_status(const ena_t *ena)
{
	if (!enahw_check_status(ena)) {
		enahw_update_reg_cache(ena);
		ena_dbg(ena, "Fatal device error");
		delay(drv_usectohz(5000 * 1000));
		panic("Fatal device error");
	}
}

void
enahw_update_reg_cache(const ena_t *ena)
{
	for (uint_t i = 0; i < ENAHW_NUM_REGS; i++) {
		enahw_reg_nv_t *nv = &enahw_reg_cache[i];
		nv->ern_value = ena_hw_bar_read32(ena, nv->ern_offset);
	}
}
