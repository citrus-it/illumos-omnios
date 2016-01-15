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

TPLDIR = svccfg/templates
TEST = $(TPLDIR)/validation
DESC = stf_description validation_combo_sets validation_sets
TCS = runtest

include ../../../Makefile.test

#
# perl scripts
#
PERL = gen_validate_test gen_vi_test
PERLDIR = $(ROOTOPTPKG)/tests/$(TEST)

PLS = $(PERL:%=$(PERLDIR)/%)
$(PLS) := FILEMODE = 0555

install: $(PLS)

$(PERLDIR)/%: %.pl
	$(INS.rename)
