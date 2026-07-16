#
# This file and its contents are supplied under the terms of the
# Common Development and Distribution License ("CDDL"), version 1.0.
# You may only use this file in accordance with the terms of version
# 1.0 of the CDDL.
#
# A full copy of the text of the CDDL should have accompanied this
# source.  A copy of the CDDL is also available via the Internet at
# http://www.illumos.org/license/CDDL.
#

#
# Copyright 2026 Oxide Computer Company
#

PROG = devlink-canary
OBJS = devlink_canary.o
SRCS = $(OBJS:.o=.c)

MANIFEST = devlink-monitor.xml
SVCMETHOD = svc-devlink-monitor

include ../Makefile.cmd
include ../Makefile.cmd.64
include ../Makefile.ctf

ROOTMANIFESTDIR = $(ROOTSVCSYSTEM)
ROOTLIBMONDIR = $(ROOTLIB)/devlink-monitor
ROOTLIBMONPROG = $(ROOTLIBMONDIR)/$(PROG)

CSTD = $(CSTD_GNU99)

CLEANFILES += $(OBJS)

.KEEP_STATE:

all: $(PROG)

$(PROG): $(OBJS)
	$(LINK.c) -o $@ $(OBJS) $(LDLIBS)
	$(POST_PROCESS)

$(ROOTLIBMONDIR):
	$(INS.dir)

$(ROOTLIBMONPROG): $(ROOTLIBMONDIR) $(PROG)
	$(INS.file) $(PROG)

install: all $(ROOTMANIFEST) $(ROOTSVCMETHOD) $(ROOTLIBMONPROG)

check: $(CHKMANIFEST)

clean:
	$(RM) $(CLEANFILES)

include ../Makefile.targ
