/*
 * Copyright 2015 Nexenta Systems, Inc.  All rights reserved.
 */

#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/cmn_err.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>

#include "krrp_protocol.h"

static struct {
	const char		*str;
	krrp_opcode_t	opcode;
} opcodes_str[] = {
#define	KRRP_OPCODE_EXPAND(enum_name) \
	    {"KRRP_OPCODE_"#enum_name, KRRP_OPCODE_##enum_name},
	KRRP_OPCODES_DATA_MAP(KRRP_OPCODE_EXPAND)
	KRRP_OPCODES_CTRL_MAP(KRRP_OPCODE_EXPAND)
#undef KRRP_OPCODE_EXPAND
};

static size_t opcodes_str_sz = sizeof (opcodes_str) / sizeof (opcodes_str[0]);

const char *
krrp_protocol_opcode_str(krrp_opcode_t opcode)
{
	size_t i;

	for (i = 0; i < opcodes_str_sz; i++) {
		if (opcodes_str[i].opcode == opcode)
			return (opcodes_str[i].str);
	}

	return ("KRRP_OPCODE_UNKNOWN");
}
