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
 * Copyright 2025 Oxide Computer Company
 */

#ifndef _UHSMP_H
#define	_UHSMP_H

/*
 * Private ioctls for interfacing with the uhsmp driver.
 */

#ifdef __cplusplus
extern "C" {
#endif

#define	UHSMP_IOCTL	(('h' << 24) | ('s' << 16) | ('m' << 8))

#define	UHSMP_GENERIC_COMMAND	(UHSMP_IOCTL | 0x01)

typedef struct uhsmp_cmd {
	uint32_t uc_id;
	uint32_t uc_response;
	uint32_t uc_args[8];
} uhsmp_cmd_t;

#define	UHSMP_RESPONSE_INCOMPLETE	0x0
#define	UHSMP_RESPONSE_OK		0x1
#define	UHSMP_RESPONSE_INVALID_ARGS	0xff
#define	UHSMP_RESPONSE_INVALID_MSGID	0xfe
#define	UHSMP_RESPONSE_REJECTED_PREREQ	0xfd
#define	UHSMP_RESPONSE_REJECTED_BUSY	0xfc

#define	UHSMP_TESTMESSAGE	0x1
#define	UHSMP_GETIFVERSION	0x3

#ifdef __cplusplus
}
#endif

#endif /* _UHSMP_H */
