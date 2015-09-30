#! /usr/bin/ksh -p
#
# CDDL HEADER START
#
# The contents of this file are subject to the terms of the
# Common Development and Distribution License (the "License").
# You may not use this file except in compliance with the License.
#
# You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
# or http://www.opensolaris.org/os/licensing.
# See the License for the specific language governing permissions
# and limitations under the License.
#
# When distributing Covered Code, include this CDDL HEADER in each
# file and include the License file at usr/src/OPENSOLARIS.LICENSE.
# If applicable, add the following below this CDDL HEADER, with the
# fields enclosed by brackets "[]" replaced with your own identifying
# information: Portions Copyright [yyyy] [name of copyright owner]
#
# CDDL HEADER END
#

#
# Copyright 2009 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#

# Test directories from client and server

# Path for SERVER to test
export NFSSHRDIR=${NFSSHRDIR:-"/TESTDIR_shmnt"}
# Path for localhost to mount
export NFSMNTDIR=${NFSMNTDIR:-"/MNTDIR_shmnt"}

# Path for localhost to mount
export AUTOIND=${AUTOIND:-"/AUTO_shmnt"}

# NFS default mount options
export MNTOPT=${MNTOPT:-"rw"}

# RDMA variable in case user wants it
export TESTRDMA=${TESTRDMA:-"no"}

# DEBUG variable for the suite
export SHAREMNT_DEBUG=\$SHAREMNT_DEBUG

# Unique tag of testing user for the suite
export TUSER_UTAG="NFSTestSuiteUser@sharemnt"

# UTILS variable for generic libs/utils used by test
whence -p stc_genutils > /dev/null 2>&1
if (( $? != 0 )); then
	echo "config.vars: stc_genutils command not found!"
	exit 1
fi

export STC_GENUTILS=$(stc_genutils path)

# NFS services
export SRV_FMRI="svc:/network/nfs/server"
export LCK_FMRI="svc:/network/nfs/nlockmgr"
export QUOTA_FMRI="svc:/network/nfs/rquota"
export STAT_FMRI="svc:/network/nfs/status"
export CBD_FMRI="svc:/network/nfs/cbd"
export MAP_FMRI="svc:/network/nfs/mapid"
export AUTO_FMRI="svc:/system/filesystem/autofs:default"
export SERVICES="$SRV_FMRI $LCK_FMRI $QUOTA_FMRI $STAT_FMRI $CBD_FMRI $MAP_FMRI"

STF_VARIABLES=" \
		SERVER MNTDIR AUTOIND MNTOPT \
		SHAREMNT_DEBUG CLIENT CLIENT_S SERVER_S \
		SERVICES SRV_FMRI LCK_FMRI QUOTA_FMRI STAT_FMRI \
		CBD_FMRI MAP_FMRI AUTO_FMRI \
		NFSSHRDIR NFSMNTDIR ZONE_PATH NFSMAPID_DOMAIN \
		TESTRDMA TUSER_UTAG STC_GENUTILS"
