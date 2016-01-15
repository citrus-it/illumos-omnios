/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */

/*
 * Copyright 2008 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * NAME
 *	base32 - Get a base32 encoded value or decode a base32 value
 *
 * SYNOPSIS
 *	base32 [e|d] <value>
 *
 * DESCRIPTION
 *	This program simply encodes a given value, or decodes a
 * 	given value.
 *
 * OPTIONS
 *	The following options are supported:
 *
 *	-e	encode the given value
 *
 *	-d	decode the given value
 *
 *	-p padchr	Character to use for padding.  Default is to leave
 *			the selection to the encoding functions.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/types.h>
#include <unistd.h>
#include <libscf.h>

/* Default values */
#define	DECODE32_GS		(8)	/* scf_decode32 group size */
#define	DEFAULT_PAD		(45)

/*
 * base32[] index32[] are used in base32 encoding and decoding.
 */
static char base32[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
static char index32[128] = {
	-1, -1, -1, -1, -1, -1, -1, -1, /* 0-7 */
	-1, -1, -1, -1, -1, -1, -1, -1, /* 8-15 */
	-1, -1, -1, -1, -1, -1, -1, -1, /* 16-23 */
	-1, -1, -1, -1, -1, -1, -1, -1, /* 24-31 */
	-1, -1, -1, -1, -1, -1, -1, -1, /* 32-39 */
	-1, -1, -1, -1, -1, -1, -1, -1, /* 40-47 */
	-1, -1, 26, 27, 28, 29, 30, 31, /* 48-55 */
	-1, -1, -1, -1, -1, -1, -1, -1, /* 56-63 */
	-1, 0, 1, 2, 3, 4, 5, 6,	/* 64-71 */
	7, 8, 9, 10, 11, 12, 13, 14,	/* 72-79 */
	15, 16, 17, 18, 19, 20, 21, 22, /* 80-87 */
	23, 24, 25, -1, -1, -1, -1, -1, /* 88-95 */
	-1, -1, -1, -1, -1, -1, -1, -1, /* 96-103 */
	-1, -1, -1, -1, -1, -1, -1, -1, /* 104-111 */
	-1, -1, -1, -1, -1, -1, -1, -1, /* 112-119 */
	-1, -1, -1, -1, -1, -1, -1, -1  /* 120-127 */
};

static void
usage()
{
	(void) fprintf(stderr, "Usage\n");
	(void) fprintf(stderr, "\t base32 [-e|-d] [-p char] <value>\n");
	exit(1);
}

/*
 * Code taken directly from the scf library to implement the
 * base32 encoding.  This is done with the consideration that
 * the integration implementation is correct and that any changes
 * to the implementation should be reflected in the code to
 * keep it the same so that this implementations results do not
 * change, or the tests need to be updated to deal with the
 * implementations as such.
 */
int
encode32(const char *input, size_t inlen, char *output, size_t outmax,
    size_t *outlen, char pad)
{
	uint_t group_size = 5;
	uint_t i;
	const unsigned char *in = (const unsigned char *)input;
	size_t olen;
	uchar_t *out = (uchar_t *)output;
	uint_t oval;
	uint_t pad_count;

	/* Verify that there is enough room for the output. */
	olen = ((inlen + (group_size - 1)) / group_size) * 8;
	if (outlen)
		*outlen = olen;
	if (olen > outmax)
		return (-1);

	/* If caller did not provide pad character, use the default. */
	if (pad == 0) {
		pad = '=';
	} else {
		/*
		 * Make sure that caller's pad is not one of the encoding
		 * characters.
		 */
		for (i = 0; i < sizeof (base32) - 1; i++) {
			if (pad == base32[i])
				return (-1);
		}
	}

	/* Process full groups capturing 5 bits per output character. */
	for (; inlen >= group_size; in += group_size, inlen -= group_size) {
		/*
		 * For the purposes of the comments in this section of the
		 * bits in an 8 bit byte have number 0 to 7.  The high
		 * order bit is bit 7 and the low order bit is bit 0.
		 */

		/* top 5 bits (7-3) from in[0] */
		*out++ = base32[in[0] >> 3];
		/* bits 2-0 from in[0] and top 2 (7-6) from in[1] */
		*out++ = base32[((in[0] << 2) & 0x1c) | (in[1] >> 6)];
		/* 5 bits (5-1) from in[1] */
		*out++ = base32[(in[1] >> 1) & 0x1f];
		/* low bit (0) from in[1] and top 4 (7-4) from in[2] */
		*out++ = base32[((in[1] << 4) & 0x10) | ((in[2] >> 4) & 0xf)];
		/* low 4 (3-0) from in[2] and top bit (7) from in[3] */
		*out++ = base32[((in[2] << 1) & 0x1e) | (in[3] >> 7)];
		/* 5 bits (6-2) from in[3] */
		*out++ = base32[(in[3] >> 2) & 0x1f];
		/* low 2 (1-0) from in[3] and top 3 (7-5) from in[4] */
		*out++ = base32[((in[3] << 3) & 0x18) | (in[4] >> 5)];
		/* low 5 (4-0) from in[4] */
		*out++ = base32[in[4] & 0x1f];
	}

	/* Take care of final input bytes. */
	pad_count = 0;
	if (inlen) {
		/* top 5 bits (7-3) from in[0] */
		*out++ = base32[in[0] >> 3];
		/*
		 * low 3 (2-0) from in[0] and top 2 (7-6) from in[1] if
		 * available.
		 */
		oval = (in[0] << 2) & 0x1c;
		if (inlen == 1) {
			*out++ = base32[oval];
			pad_count = 6;
			goto padout;
		}
		oval |= in[1] >> 6;
		*out++ = base32[oval];
		/* 5 bits (5-1) from in[1] */
		*out++ = base32[(in[1] >> 1) & 0x1f];
		/*
		 * low bit (0) from in[1] and top 4 (7-4) from in[2] if
		 * available.
		 */
		oval = (in[1] << 4) & 0x10;
		if (inlen == 2) {
			*out++ = base32[oval];
			pad_count = 4;
			goto padout;
		}
		oval |= in[2] >> 4;
		*out++ = base32[oval];
		/*
		 * low 4 (3-0) from in[2] and top 1 (7) from in[3] if
		 * available.
		 */
		oval = (in[2] << 1) & 0x1e;
		if (inlen == 3) {
			*out++ = base32[oval];
			pad_count = 3;
			goto padout;
		}
		oval |= in[3] >> 7;
		*out++ = base32[oval];
		/* 5 bits (6-2) from in[3] */
		*out++ = base32[(in[3] >> 2) & 0x1f];
		/* low 2 bits (1-0) from in[3] */
		*out++ = base32[(in[3] << 3) & 0x18];
		pad_count = 1;
	}
padout:
	/*
	 * Pad the output so that it is a multiple of 8 bytes.
	 */
	for (; pad_count > 0; pad_count--) {
		*out++ = pad;
	}

	/*
	 * Null terminate the output if there is enough room.
	 */
	if (olen < outmax)
		*out = 0;

	return (0);
}


/*
 * Code taken directly from the scf library to implement the
 * base32 decoding.  This is done with the consideration that
 * the integration implementation is correct and that any changes
 * to the implementation should be reflected in the code to
 * keep it the same so that this implementations results do not
 * change, or the tests need to be updated to deal with the
 * implementations as such.
 */
int
decode32(const char *in, size_t inlen, char *outbuf, size_t outmax,
    size_t *outlen, char pad)
{
	char *bufend = outbuf + outmax;
	char c;
	uint_t count;
	uint32_t g[DECODE32_GS];
	size_t i;
	uint_t j;
	char *out = outbuf;
	boolean_t pad_seen = B_FALSE;

	/* If caller did not provide pad character, use the default. */
	if (pad == 0) {
		pad = '=';
	} else {
		/*
		 * Make sure that caller's pad is not one of the encoding
		 * characters.
		 */
		for (i = 0; i < sizeof (base32) - 1; i++) {
		if (pad == base32[i])
			return (-1);
		}
	}

	i = 0;
	while ((i < inlen) && (out < bufend)) {
		/* Get a group of input characters. */
		for (j = 0, count = 0;
		    (j < DECODE32_GS) && (i < inlen); i++) {
				c = in[i];
				/*
				 * RFC 4648 allows for the encoded data to be
				 * split into multiple lines, so skip carriage
				 * returns and new lines.
				 */
				if ((c == '\r') || (c == '\n'))
					continue;
				if ((pad_seen == B_TRUE) && (c != pad)) {
					/* Group not completed by pads */
					return (-1);
				}
				if ((c < 0) || (c >= sizeof (index32))) {
					/* Illegal character. */
					return (-1);
				}
				if (c == pad) {
					pad_seen = B_TRUE;
					continue;
				}
				if ((g[j++] = index32[(int)c]) == 0xff) {
					/* Illegal character */
					return (-1);
				}
				count++;
		}

		/* Pack the group into five 8 bit bytes. */
		if ((count >= 2) && (out < bufend)) {
			/*
			 * Output byte 0:
			 *	5 bits (7-3) from g[0]
			 *	3 bits (2-0) from g[1] (4-2)
			 */
			*out++ = (g[0] << 3) | ((g[1] >> 2) & 0x7);
		}
		if ((count >= 4) && (out < bufend)) {
			/*
			 * Output byte 1:
			 *	2 bits (7-6) from g[1] (1-0)
			 *	5 bits (5-1) from g[2] (4-0)
			 *	1 bit (0) from g[3] (4)
			 */
			*out++ = (g[1] << 6) | (g[2] << 1) | \
			    ((g[3] >> 4) & 0x1);
		}
		if ((count >= 5) && (out < bufend)) {
			/*
			 * Output byte 2:
			 *	4 bits (7-4) from g[3] (3-0)
			 *	4 bits (3-0) from g[4] (4-1)
			 */
			*out++ = (g[3] << 4) | ((g[4] >> 1) & 0xf);
		}
		if ((count >= 7) && (out < bufend)) {
			/*
			 * Output byte 3:
			 *	1 bit (7) from g[4] (0)
			 *	5 bits (6-2) from g[5] (4-0)
			 *	2 bits (0-1) from g[6] (4-3)
			 */
			*out++ = (g[4] << 7) | (g[5] << 2) |
			    ((g[6] >> 3) & 0x3);
		}
		if ((count == 8) && (out < bufend)) {
			/*
			 * Output byte 4;
			 *	3 bits (7-5) from g[6] (2-0)
			 *	5 bits (4-0) from g[7] (4-0)
			 */
			*out++ = (g[6] << 5) | g[7];
		}
	}

	if (i < inlen) {
		/* Did not process all input characters. */
		return (-1);
	}

	if (outlen)
		*outlen = out - outbuf;

	/* Null terminate the output if there is room. */
	if (out < bufend)
		*out = 0;
	return (0);
}

void
do_code(char *src, char *coded, size_t src_len, size_t coded_len, int pad,
    int flag) {
	size_t	olen;
	int	r;

	if (flag == 1) {
		r = encode32(src, src_len, coded, coded_len, &olen, pad);
		if (r == 0)
			(void) printf("%s\n", coded);
		return;
	}

	if (flag == 2) {
		r = decode32(coded, coded_len, src, src_len, &olen, pad);
		if (r == 0)
			(void) printf("%s\n", src);
		return;
	}

	exit(1);
}

int
main(int argc, char * const argv[])
{
	size_t		src_len = NULL, coded_len = NULL;
	char		*coded = NULL;
	char		*src = NULL;
	int		pad = DEFAULT_PAD;
	int		flag;
	int		c;

	flag = 0;

	while ((c = getopt(argc, argv, "e:d:p:h")) != -1) {
		switch (c) {
		case 'e':
			flag |= 1;
			src = optarg;
			src_len = strlen(optarg);
			break;
		case 'd':
			flag |= 2;
			coded = optarg;
			coded_len = strlen(optarg);
			break;
		case 'p':
			pad = atoi(optarg);
			break;
		case 'h':
			usage();
			exit(0);
		}
	}

	if (flag == 0 || flag == 3) {
		(void) fprintf(stderr, "Invalid option set\n");
		usage();
	}

	if (src == NULL && coded == NULL) {
		exit(0);
	}

	/*
	 * Allocate buffers.
	 */
	if (flag == 1) {
		coded_len = (((src_len + 5) / 5) * 8) + 1;
		coded = malloc(coded_len);
	} else {
		src_len = coded_len;
		src = malloc(src_len);
	}
	if ((src == NULL) || (coded == NULL)) {
		(void) fprintf(stderr, "Out of memory.\n");
		exit(1);
	}

	do_code(src, coded, src_len, coded_len, pad, flag);

	return (0);
}
