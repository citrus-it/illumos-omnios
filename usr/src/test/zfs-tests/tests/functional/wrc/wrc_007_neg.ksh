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
#	A raidz wrc can not be added to existing pool.
#
# STRATEGY:
#	1. Create pool without wrc.
#	2. Add a raidz wrc to this pool.
#	3. Verify failed to add.
#

verify_runnable "global"
log_assert "A raidz wrc can not be added to existing pool."
log_onexit cleanup
for pool_type in "" "mirror" ; do
	for wrc_type in "raidz" "raidz2" "raidz3" ; do
		log_must $ZPOOL create -f \
			$TESTPOOL $pool_type $SSD_DISKS
		log_mustnot $ZPOOL add $TESTPOOL special $wrc_type $HDD_DISKS
		log_must destroy_pool $TESTPOOL
	done
done
log_pass "A raidz wrc can not be added to existing pool."
