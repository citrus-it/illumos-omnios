# This file and its contents are supplied under the terms of the
# Common Development and Distribution License ("CDDL"), version 1.0.
# You may only use this file in accordance with the terms of version
# 1.0 of the CDDL.
#
# A full copy of the text of the CDDL should have accompanied this
# source. A copy of the CDDL is also available via the Internet at
# http://www.illumos.org/license/CDDL.

# Copyright 2022 OmniOS Community Edition (OmniOSce) Association.

MODULE = vmm.so
MDBTGT = kvm

MODSRCS = vmm.c

include ../../../../../Makefile.cmd
include ../../../../../Makefile.cmd.64
include ../../../../intel/Makefile.amd64
include ../../../Makefile.i86pc
include ../../../../Makefile.module

CPPFLAGS += -DMP -D_MACHDEP
CPPFLAGS += -I../../../../common
CPPFLAGS += -I$(SRC)/uts/i86pc/io/vmm
CPPFLAGS += -I$(SRC)/uts/i86pc

