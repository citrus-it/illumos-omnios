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
#	A raidz wrc is not supported.
#
# STRATEGY:
#	1. Try to create pool with unsupported type
#	2. Creating pool must fail.
#

verify_runnable "global"
log_assert "A raidz wrc is not supported."
log_onexit cleanup
for pool_type in "" "mirror" ; do
	for wrc_type in "raidz" "raidz2" "raidz3" ; do
		log_mustnot $ZPOOL create -f \
			$TESTPOOL $pool_type $SSD_DISKS \
			special $wrc_type $HDD_DISKS
	done
done
log_pass "A raidz wrc is not supported."
