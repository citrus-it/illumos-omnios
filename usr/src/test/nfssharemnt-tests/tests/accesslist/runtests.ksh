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
[[ :$SHAREMNT_DEBUG: == *:${NAME}:* \
	|| :${SHAREMNT_DEBUG}: == *:all:* ]] && set -x

################################################################################
#
# __stc_assertion_start
#
# ID: runtests
#
# DESCRIPTION:
#   For each SHROPT, go through the MNTOPT and mount the SERVER's filesystems
#   Do some simple rw/ro testing
#
# STRATEGY:
#   1. Call domount_check to mount test filesystem and verify
#   2. Call do_rw|ro_test script if needed for testing in the mounted filesystem
#   3. Call unmount_check to umount and check the filesystem is umounted
#
# TESTABILITY: explicit
#
# TEST_AUTOMATION_LEVEL: automated
#
# __stc_assertion_end
#
################################################################################

# Function to do ro|rw|remount testing in the NFS mount point
#   Usage: do_others [URL] [check_dir]
#
function do_others {
    typeset Fname=do_others
    [[ :$SHAREMNT_DEBUG: == *:${NAME}:* \
	|| :${SHAREMNT_DEBUG}: == *:all:* ]] && set -x

    typeset urlflag=""
    if [[ $1 == URL ]]; then
	urlflag="URL"
	shift
    fi
    typeset ckdir="$*"

    if (( $ckro > 0 )); then
	do_ro_test

	if (( $shr_ro == 0 )); then
	    set -A molist remount remount,rw rw,remount
	    typeset remnt_opt=${molist[$((RANDOM % 3 ))]}
	    typeset newmntopt=$remnt_opt
	    # if mount with URL format and "vers=2|3",
	    # remount must use "vers=2|3";
	    # if mount option contains "public" and "vers=2|3",
	    # remount must use "public" and "vers=2|3" too,
	    # otherwise, remount will fail.
	    [[ (-n $urlflag || ,$Mntopts, == *,public,*) && \
	    	,$Mntopts, == @(*,vers=2,*|*,vers=3,*) ]] && \
	    	newmntopt=$(echo $Mntopts | sed "s/ro/$remnt_opt/g")
	    domount_check $urlflag $newmntopt $Shropt $ckdir
	    do_rw_test
	fi
    else
	if [[ $Shropt == *anon=* ]]; then
	    do_rw_test "ANON" $Shropt
	else
	    do_rw_test "ANON" "nobody"
	fi

	set -A molist remount,ro ro,remount
	do_neg_mount_check ${molist[$((RANDOM % 2 ))]}
    fi
    unmount_check
}

USAGE="Usage: runtests Test_name Share_opt Mnt_opts"

if (( $# < 3 )); then
	echo "$USAGE"
	exit $STF_UNRESOLVED
fi

typeset Tname=$1
typeset Shropt=$2
typeset Mntopts=$3

# Check TX related info
check_for_cipso "$SHRDIR" "$MNTDIR" "$Mntopts" || return $STF_UNSUPPORTED

share_check "$Shropt"

typeset -i ckro=0 shr_ro=0 mnt_ro=0
shr_ro=$(echo "$Shropt" | \
	nawk -F\, '{for (i=1; i<=NF; i++) {if ($i ~ /^ro/) print i} }')
mnt_ro=$(echo "$Mntopts" | \
	nawk -F\, '{for (i=1; i<=NF; i++) {if ($i ~ /^ro/) print i} }')
ckro=$((shr_ro | mnt_ro))

if [[ ,$Shropt, == *,public,* && ,$Mntopts, == *,public,* && \
	,$Mntopts, != @(*,vers=2,*|*,vers=3,*) && \
	! (,$Mntopts, == *,proto=udp,* && $Mntopts != *"vers"*) ]]; then
	domount_check "$Mntopts" "$Shropt" "/"
	automount_check "$Mntopts" "$Shropt" "BASIC" "/"
	do_others "/"
else
	domount_check "$Mntopts" "$Shropt"
	automount_check "$Mntopts" "$Shropt" "BASIC"
	do_others
fi

# do url mount check
if [[ ,$Shropt, == *,public,* ]]; then
	domount_check "URL" "$Mntopts" "$Shropt" "/"
	automount_check "URL" "$Mntopts" "$Shropt" "BASIC_URL" "/"
	do_others "URL" "/"
else
	domount_check "URL" "$Mntopts" "$Shropt"
	automount_check "URL" "$Mntopts" "$Shropt" "BASIC_URL"
	do_others "URL"
fi

echo "$NAME: testing complete - Result PASS"
cleanup $STF_PASS
