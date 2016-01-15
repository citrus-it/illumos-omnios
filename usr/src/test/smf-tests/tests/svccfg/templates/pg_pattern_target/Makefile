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
TEST = $(TPLDIR)/pg_pattern_target
DESC = config.env pg_pattern_target_local.xml pg_pattern_target_service.xml \
	pg_pattern_target_restarter.xml pg_pattern_target_user.xml \
	stf_description
TCS = pg_pattern_target_001 pg_pattern_target_002 pg_pattern_target_003 \
	pg_pattern_target_004 pg_pattern_target_005 pg_pattern_target_006 \
	pg_pattern_target_007 pg_pattern_target_008 pg_pattern_target_009 \
	pg_pattern_target_010 pg_pattern_target_011 pg_pattern_target_012

include ../../../Makefile.test
