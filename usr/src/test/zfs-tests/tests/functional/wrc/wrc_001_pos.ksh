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
#	Creating a pool with a wrc device succeeds.
#
# STRATEGY:
#	1. Create pool with separated wrc devices.
#	2. Display pool status
#	3. Scrub pool and check status
#	4. Destroy and loop to create pool with different configuration.
#

verify_runnable "global"
log_assert "Creating a pool with a wrc device succeeds."
log_onexit cleanup
for pool_type in "" "mirror" "raidz" "raidz2" "raidz3" ; do
	for wrc_type in "" "mirror" ; do
		for wrc_mode in "off" "active" "passive" ; do
			rs=$(random_get "4k" "8k" "16k" "32k" "64k" "128k")
			cs=$(random_get "off" "on" "lz4" "lzjb")
			log_must $ZPOOL create -f \
				-O compression=$cs -O recordsize=$rs \
				$TESTPOOL $pool_type $HDD_DISKS \
				special $wrc_type $SSD_DISKS
			log_must $ZPOOL set wrc_mode=$wrc_mode $TESTPOOL
			log_must display_status $TESTPOOL
			log_must $SYNC
			log_must $ZPOOL scrub $TESTPOOL
			while is_pool_scrubbing $TESTPOOL ; do
				$SLEEP 1
			done
			log_must check_pool_errors $TESTPOOL
			log_must destroy_pool $TESTPOOL
		done
	done
done
log_pass "Creating a pool with a wrc device succeeds."
