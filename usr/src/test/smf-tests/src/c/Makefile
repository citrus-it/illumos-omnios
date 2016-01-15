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
# Copyright (c) 2012 by Delphix. All rights reserved.
# Copyright 2015 Nexenta Systems, Inc.  All rights reserved.
#
include $(SRC)/cmd/Makefile.cmd

ROOTOPTPKG = $(ROOT)/opt/smf-tests
TESTDIR = $(ROOTOPTPKG)/bin

PROGS = service_checkrun service_lastcall service_offsetcall \
	service_countcall service_getpids service_dumpstate \
	ps_start name_gen base32

MULTIPROG = service_app

SMF_INC = ../../include

LOGOBJS = logging.o state.o
SVCOBJS = service_functionality.o service_main.o

SRCS = $(PROGS:%=%.c) $(LOGOBJS:%.o=%.c) $(SVCOBJS:%.o=%.c)
OBJS = $(PROGS:%=%.o) $(LOGOBJS) $(SVCOBJS)

BINS = $(PROGS:%=$(TESTDIR)/%) $(MULTIPROG:%=$(TESTDIR)/%)
$(BINS) := FILEMODE = 0555

TCFLAGS = -erroff=E_NAME_USED_NOT_DEF2 \
	-erroff=E_NAME_DEF_NOT_USED2 \
	-erroff=E_CONST_TRUNCATED_BY_ASSIGN

CFLAGS = -I$(SMF_INC)
LDLIBS += -lproject -lscf -lpool -lumem -lsocket -lnsl
LDFLAGS += -zignore

$(MULTIPROG): $(LOGOBJS) $(SVCOBJS)
	$(LINK.c) -o $@ $(SVCOBJS) $(LOGOBJS) $(LDLIBS)
	$(POST_PROCESS)

$(PROGS): $(OBJS) $(LOGOBJS)
	$(LINK.c) -o $@ $@.o $(LOGOBJS) $(LDLIBS)
	$(POST_PROCESS)

%.o: %.c
	$(COMPILE.c) $(CFLAGS) $<

all install: $(BINS)

lint:
	for src in $(SRCS); \
	do \
	$(LINT.c) $(TCFLAGS) $(CFLAGS) $${src} $(LDLIBS); \
	done
	
clobber clean:
	-$(RM) $(OBJS) $(PROGS) $(MULTIPROG)

$(TESTDIR)/%: %
	$(INS.file)
