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
#	Enabling active wrc mode succeeds.
#
# STRATEGY:
#	1. Create pool with separated wrc devices.
#	2. Enable wrc passive mode
#	3. Display pool status
#	3. Enable wrc active mode
#	5. Display pool status
#	6. Scrub pool and check status
#

verify_runnable "global"
log_assert "Enabling active wrc mode succeeds."
log_onexit cleanup
log_must create_pool_wrc $TESTPOOL passive
log_must display_status $TESTPOOL
log_must $ZPOOL set wrc_mode=active $TESTPOOL
log_must display_status $TESTPOOL
log_must $SYNC
log_must $ZPOOL scrub $TESTPOOL
while is_pool_scrubbing $TESTPOOL ; do
	$SLEEP 1
done
log_must check_pool_errors $TESTPOOL
log_must destroy_pool $TESTPOOL
log_pass "Enabling active wrc mode succeeds."
