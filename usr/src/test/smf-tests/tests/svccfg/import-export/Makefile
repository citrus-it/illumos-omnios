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

TEST = svccfg/import-export
DESC = config.env stf_description standard.xml
TCS = impexp_cleanup  impexp_lib impexp_setup

include ../../Makefile.test

SUBDIRS = dependency dependent instance invalid lists property_group \
	propval stability template

include $(SRC)/test/Makefile.com
