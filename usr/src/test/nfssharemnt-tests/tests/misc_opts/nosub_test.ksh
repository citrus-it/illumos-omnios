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

DIR=$(dirname $0)
NAME=$(basename $0)

. $STF_SUITE/include/sharemnt.kshlib
. $STC_GENUTILS/include/nfs-tx.kshlib

export STC_GENUTILS_DEBUG=$STC_GENUTILS_DEBUG:$SHAREMNT_DEBUG
[[ :${SHAREMNT_DEBUG}: = *:${NAME}:* \
	|| :${SHAREMNT_DEBUG}: = *:all:* ]] && set -x

################################################################################
#
# __stc_assertion_start
#
# ID: nosub_test
#
# DESCRIPTION:
#   For the mount and export the SERVER's filesystems with nosub
#
# STRATEGY:
#   verify the share and mount/automount behaviors
#
# TESTABILITY: explicit
#
# TEST_AUTOMATION_LEVEL: automated
#
# __stc_assertion_end
#
################################################################################

USAGE="Usage: $0 Test_name Share_opt Mnt_opts"

if (( $# < 2 )); then
	echo "$USAGE"
	exit $STF_UNRESOLVED
fi

typeset Tname=$1
typeset Shropt=$2
typeset Mntopts=$3
typeset res="PASS"

export SHRDIR_OFFSET="/dir0777"

# Check TX related info
check_for_cipso "$SHRDIR" "$MNTDIR" "$Mntopts" || return $STF_UNSUPPORTED

case $Mntopts in
*2|*3)
	share_check "$Shropt"
	do_neg_mount_check "$Mntopts" "$Shropt"
	do_neg_automount_check "$Mntopts" "NBASIC_NOSUB" "$Shropt"
	;;
*)
	# Using the runtests to avoid the duplicate code
	# Notice the output
	ret=$(runtests "$Tname" "$Shropt" "$Mntopts")
	[[ $? != 0 ]] && res="FAIL"
	echo "$ret" | grep -v "runtests: testing complete"
	;;
esac

export SHRDIR_OFFSET=""
echo "$NAME: testing complete - Result $res"
[[ $res == "PASS" ]] && cleanup $STF_PASS || cleanup $STF_FAIL
