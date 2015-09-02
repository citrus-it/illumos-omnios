#! /usr/bin/ksh -p
#
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
# Copyright 2015 Nexenta Systems, Inc. All rights reserved.
#

. $STF_SUITE/tests/functional/wrc/wrc.cfg
. $STF_SUITE/tests/functional/wrc/wrc.kshlib

#
# DESCRIPTION:
#	wrc active/passive mode can not be deactivated.
#
# STRATEGY:
#	1. Create pool with separated wrc devices.
#	2. Enable wrc active or passive mode
#	3. Display pool status
#	4. Try to disable wrc mode
#

verify_runnable "global"
log_assert "wrc device can only active/passive mode."
log_onexit cleanup
for wrc_mode in "active" "passive" ; do
	log_must create_pool_wrc $TESTPOOL $wrc_mode
	log_must display_status $TESTPOOL
	log_mustnot $ZPOOL set wrc_mode=off $TESTPOOL
	log_must destroy_pool $TESTPOOL
done
log_pass "wrc device can only active/passive mode."
