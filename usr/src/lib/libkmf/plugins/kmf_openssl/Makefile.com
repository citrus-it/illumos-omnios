#
# CDDL HEADER START
#
# The contents of this file are subject to the terms of the
# Common Development and Distribution License (the "License").
# You may not use this file except in compliance with the License.
#
# You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
# or http://www.opensolaris.org/os/licensing.
# See the License for the specific language governing permissions
# and limitations under the License.
#
# When distributing Covered Code, include this CDDL HEADER in each
# file and include the License file at usr/src/OPENSOLARIS.LICENSE.
# If applicable, add the following below this CDDL HEADER, with the
# fields enclosed by brackets "[]" replaced with your own identifying
# information: Portions Copyright [yyyy] [name of copyright owner]
#
# CDDL HEADER END
#
# Copyright 2009 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
# Copyright (c) 2018, Joyent, Inc.

LIBRARY=	kmf_openssl.a
VERS=		.1

OBJECTS=	openssl_spi.o

include	$(SRC)/lib/Makefile.lib

LIBLINKS=	$(DYNLIB:.so.1=.so)
KMFINC=		-I../../../include -I../../../ber_der/inc

BERLIB=		-lkmf -lkmfberder
BERLIB64=	$(BERLIB)

OPENSSLLIBS=	$(BERLIB) -lcrypto -lcryptoutil -lc
OPENSSLLIBS64=	$(BERLIB64) -lcrypto -lcryptoutil -lc

SRCDIR=		../common
INCDIR=		../../include

CPPFLAGS	+=	$(KMFINC) \
			-I$(INCDIR) -I$(ADJUNCT_PROTO)/usr/include/libxml2

CERRWARN	+=	-Wno-unused-label
CERRWARN	+=	-Wno-unused-value
CERRWARN	+=	-Wno-uninitialized

# not linted
SMATCH=off

PICS=	$(OBJECTS:%=pics/%)

LDLIBS32 	+=	$(OPENSSLLIBS)

ROOTLIBDIR=	$(ROOT)/lib/crypto/$(MACH32)
ROOTLIBDIR64=	$(ROOT)/lib/crypto

.KEEP_STATE:

LIBS	=	$(DYNLIB)
all:	$(DYNLIB)

FRC:

include $(SRC)/lib/Makefile.targ
