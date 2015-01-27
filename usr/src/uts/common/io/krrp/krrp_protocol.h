/*
 * Copyright 2015 Nexenta Systems, Inc.  All rights reserved.
 */

#ifndef	_KRRP_PROTOCOL_H
#define	_KRRP_PROTOCOL_H

#ifdef __cplusplus
extern "C" {
#endif

#define	KRRP_CTRL_OPCODE_MASK	0x1000

/* Data PDU opcodes */
#define	KRRP_OPCODES_DATA_MAP(X)	\
	X(DATA_WRITE)					\

/* Ctrl PDU opcodes */
#define	KRRP_OPCODES_CTRL_MAP(X)	\
	X(ERROR)						\
	X(ATTACH_SESS)					\
	X(PING)							\
	X(PONG)							\
	X(FL_CTRL_UPDATE)				\
	X(TXG_ACK)						\
	X(TXG_ACK2)						\
	X(SEND_DONE)					\
	X(SHUTDOWN)						\

#define	KRRP_OPCODE_EXPAND(enum_name) KRRP_OPCODE_##enum_name,
typedef enum {
	KRRP_OPCODE_DATA_FIRST = 0,
	KRRP_OPCODES_DATA_MAP(KRRP_OPCODE_EXPAND)

	KRRP_OPCODE_CTRL_FIRST = KRRP_CTRL_OPCODE_MASK,
	KRRP_OPCODES_CTRL_MAP(KRRP_OPCODE_EXPAND)
	KRRP_OPCODE_DATA_LAST
} krrp_opcode_t;
#undef KRRP_OPCODE_EXPAND

const char * krrp_protocol_opcode_str(krrp_opcode_t);

#ifdef __cplusplus
}
#endif

#endif /* _KRRP_PROTOCOL_H */
