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

NAME=$(basename $0)

. $STF_SUITE/include/sharemnt.kshlib
. $STC_GENUTILS/include/nfs-tx.kshlib

export STC_GENUTILS_DEBUG=$STC_GENUTILS_DEBUG:$SHAREMNT_DEBUG
[[ :${SHAREMNT_DEBUG}: == *:${NAME}:* \
		|| :${SHAREMNT_DEBUG}: == *:all:* ]] && set -x

################################################################################
#
# __stc_assertion_start
#
# ID: neg_test
#
# DESCRIPTION:
#   For the invalid SHROPT/MNTOPT and export/mount the SERVER's filesystems
#
# STRATEGY:
#   verify the share and mount/automount fail gracefully with invalid options
#
# TESTABILITY: explicit
#
# TEST_AUTOMATION_LEVEL: automated
#
# __stc_assertion_end
#
################################################################################

USAGE="Usage: $0 Test_name opts..."

if (( $# < 2 )); then
	echo "$USAGE"
	exit $STF_UNRESOLVED
fi

typeset Tname=$1
typeset Opts=$2
typeset Other_opts=$3

# Check TX related info
check_for_cipso "$SHRDIR" "$MNTDIR" "$Opts" || return $STF_UNSUPPORTED

case $Tname in
NEG_MNT*)
	share_check "rw"
	do_neg_mount_check "$Opts"
	do_neg_automount_check "$Opts" "NBASIC"
	;;
*)
	echo "$Tname: error - it is unreachable - Result FAIL"
	exit $STF_UNRESOLVED
	;;
esac

echo "$NAME: testing complete - Result PASS"
cleanup $STF_PASS
