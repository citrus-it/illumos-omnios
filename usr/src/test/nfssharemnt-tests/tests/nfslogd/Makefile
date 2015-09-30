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
# Copyright (c) 2012 by Delphix. All rights reserved.
# Copyright 2015 Nexenta Systems, Inc.  All rights reserved.
#
include $(SRC)/Makefile.master

ROOTOPTPKG = $(ROOT)/opt/nfssharemnt-tests
TESTDIR = $(ROOTOPTPKG)/tests/nfslogd
DESCDIR = $(TESTDIR)

DATAFILE = stf_description nfslog.conf nfslogd \
	nfslogd.vars
PROGS = configure mkassert runtests srv_setup \
	test_nfslogd unconfigure
	

DATA = $(DATAFILE:%=$(DESCDIR)/%)
CMDS = $(PROGS:%=$(TESTDIR)/%)
$(CMDS) := FILEMODE = 0555
$(DATA) := FILEMODE = 0444

all lint clean clobber:

install: $(CMDS) $(DATA)

$(DATA) : $(DESCDIR)
$(CMDS): $(TESTDIR)

$(TESTDIR):
	$(INS.dir)

$(DESCDIR)/% : %
	$(INS.file)

$(TESTDIR)/%: %.ksh
	$(INS.rename)
