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
include $(SRC)/Makefile.master

TEST = svccfg/templates
DESC = stf_description config.env
TCS = templates_cleanup

include ../../Makefile.test

SUBDIRS = include pg_pattern_common_name pg_pattern_description \
	pg_pattern_name pg_pattern_required pg_pattern_target \
	pg_pattern_type prop_pattern_cardinality prop_pattern_choices \
	prop_pattern_common_name prop_pattern_constraints validation \
	prop_pattern_description prop_pattern_name prop_pattern_separator \
	prop_pattern_type prop_pattern_units prop_pattern_value \
	prop_pattern_value_common_name prop_pattern_value_description \
	prop_pattern_value_name prop_pattern_visibility

include $(SRC)/test/Makefile.com
