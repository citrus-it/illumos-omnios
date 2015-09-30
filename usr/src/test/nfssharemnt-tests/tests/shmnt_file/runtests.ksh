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
#   Share a file for each SHROPT, then go through the MNTOPT and mount
#   this file from SERVER's filesystems and do some simple rw/ro testing
#
# STRATEGY:
#   1. Share a file from server's filesystem
#   2. Call domount_check and automount_check to mount test file and verify
#   3. Call do_rw|ro_test script if needed for testing in the mounted file
#   4. Call unmount_check to umount and check the file is umounted
#
# TESTABILITY: explicit
#
# TEST_AUTOMATION_LEVEL: automated
#
# __stc_assertion_end
#
################################################################################

# Function to do rw testing in the NFS mount point that's mounted from a "file"
#   Usage: do_rw_file [check_dir]
#
function do_rw_file {
	typeset Fname=do_rw_file
	[[ :$SHAREMNT_DEBUG: == *:${NAME}:* \
		|| :${SHAREMNT_DEBUG}: == *:all:* ]] && set -x

	typeset ckDIR=$1
	[[ -z $ckDIR ]] && ckDIR=$MNTDIR

	echo "Doing READ/WRITE testing at <$ckDIR> file ..."
	typeset TData="READ/WRITE tests at $ckDIR file"

	file $ckDIR > $STF_TMPDIR/file.out.$$ 2>&1
	grep 'ksh script' $STF_TMPDIR/file.out.$$ > /dev/null 2>&1
	if (( $? != 0 )); then
		echo "$Fname: checking file-type to $ckDIR failed"
		echo "\texpected=<ksh script>, got:"
		cat $STF_TMPDIR/file.out.$$
		cleanup $STF_FAIL
	fi

	echo "$TData" >> $ckDIR 2> $STF_TMPDIR/wr.out.$$
	if (( $? != 0 )); then
		echo "$Fname: APPEND/WRITE to $ckDIR failed"
		cat $STF_TMPDIR/wr.out.$$
		cleanup $STF_FAIL
	fi
	ckData=$(tail -1 $ckDIR)
	if [[ "$ckData" != "$TData" ]]; then
		echo "$Fname: READ file $ckDIR failed"
		echo "\texpect=$<$TData>"
		echo "\tgot=$<$ckData>"
		cleanup $STF_FAIL
	fi
	echo "OK"
}

# Function to do ro testing in the mount point that's mounted from a "file"
#   Usage: do_rw_file [check_dir]
#
function do_ro_file {
	typeset Fname=do_ro_file
	[[ :$SHAREMNT_DEBUG: == *:${NAME}:* \
		|| :${SHAREMNT_DEBUG}: == *:all:* ]] && set -x

	typeset ckDIR=$1
	[[ -z $ckDIR ]] && ckDIR=$MNTDIR

	echo "Doing READ ONLY testing at <$ckDIR> file ..."

	ls -ld $ckDIR > $STF_TMPDIR/lsd.out.$$ 2>&1
	grep "^d" $STF_TMPDIR/lsd.out.$$ > /dev/null 2>&1
	if (( $? == 0 )); then
		echo "$Fname: checking listing of $ckDIR failed"
		echo "\tit listed as a directory?"
		cat $STF_TMPDIR/lsd.out.$$
		cleanup $STF_FAIL
	fi
	# check ACL if it's not 'rootfile'
	if [[ ,$Shropt, != *,anon=0,* ]]; then
		ls -v $ckDIR > $STF_TMPDIR/lsv.out.$$ 2>&1
		if [[ ,$Mntopts, == @(*,vers=2,*|*,vers=3,*) ]]; then
			# NFSv3,v2 use POSIX acl, check differently
			grep "user" $STF_TMPDIR/lsv.out.$$ | \
				grep ":r--" > /dev/null 2>&1
		else
			grep "owner" $STF_TMPDIR/lsv.out.$$ | grep \
				"write_data" | grep ":deny" > /dev/null 2>&1
		fi
		if (( $? != 0 )); then
			echo "$Fname: checking ACL of $ckDIR failed"
			echo "\texpected: <owner deny write_data>|<user:r-->"
			cat $STF_TMPDIR/lsv.out.$$
			cleanup $STF_FAIL
		fi
	fi
	# append data to file should fail
	echo "Trying to write into <$ckDIR> file" >> $ckDIR \
		2> $STF_TMPDIR/write.out.$$
	if (( $? == 0 )); then
		echo "$Fname: trying to write into <$ckDIR> file succeeded"
		echo "\tit should be READ ONLY"
		cat $STF_TMPDIR/lsd.out.$$
		cleanup $STF_FAIL
	fi
	echo "OK"
}

USAGE="Usage: runtests Test_name Share_opt Mnt_opts"

if (( $# < 3 )); then
	echo "$USAGE"
	exit $STF_UNINITIATED
fi

typeset Tname=$1
typeset Shropt=$2
typeset Mntopts=$3

# Check TX related info
check_for_cipso "$SHRDIR" "$MNTDIR" "$Mntopts" || return $STF_UNSUPPORTED

# set the ReadOnly flag based on share/mount options
typeset -i ckro=0 shr_ro=0 mnt_ro=0
shr_ro=$(echo "$Shropt" | \
	nawk -F\, '{for (i=1; i<=NF; i++) {if ($i ~ /^ro/) print i} }')
mnt_ro=$(echo "$Mntopts" | \
	nawk -F\, '{for (i=1; i<=NF; i++) {if ($i ~ /^ro/) print i} }')
ckro=$((shr_ro | mnt_ro))

# first unshare the file from last run, just in case
RSH root $SERVER \
	"export SHAREMNT_DEBUG=$SRVDEBUG; \
	$SRV_TMPDIR/srv_setup -u $SHRDIR" > $STF_TMPDIR/rsh.out.$$ 2>&1
if (( $? != 0 )); then
	echo "$NAME: failed to unshare <$SHRDIR>"
	cat $STF_TMPDIR/rsh.out.$$ > /dev/null 2>&1
	cleanup $STF_UNRESOLVED
fi

# Now set the test file for share in server, for share_check
if [[ ,$Shropt, == *,anon=0,* ]]; then
	NEW_SHRDIR=$SHRDIR/rootfile
else
	(( ckro > 0 )) && \
		NEW_SHRDIR=$SHRDIR/rofile || \
			NEW_SHRDIR=$SHRDIR/rwfile
fi
share_check "$Shropt" $NEW_SHRDIR

typeset isURL=""
if [[ ,$Shropt, == *,public,* ]]; then
	# NFSv3,v2 cannot mount public root as file; thus use full path
	# And skip NFSv4,public for now because of CR 6504516
	if [[ ,$Mntopts, == @(*,vers=2,*|*,vers=3,*) ]]; then
		NEW_SHRDIR="/$NEW_SHRDIR"
	else
		# NEW_SHRDIR="/"
		echo "\tCurrently it can't test mounting NFS/URL with NFSv4"
		echo "\tdue to CR 6504516.  Test should be re-enabled once"
		echo "\tthe bug is fixed"
		cleanup $STF_UNTESTED
	fi
	isURL=URL
fi

# Then do the rw/ro check for both manual mount and automount
domount_check $isURL "$Mntopts" "$Shropt" $NEW_SHRDIR
nfsstat -m $MNTDIR
(( ckro > 0 )) && do_ro_file || do_rw_file

automount_check $isURL "$Mntopts" "$Shropt" "BASIC_URL" "$NEW_SHRDIR"
mntPTR=$(grep "$Mntopts" $STF_TMPDIR/auto_indirect.shmnt | awk '{print $1}')
mntPTR=/AUTO_shmnt/$mntPTR
nfsstat -m $mntPTR
(( ckro > 0 )) && do_ro_file $mntPTR || do_rw_file $mntPTR

# Finally umount it
unmount_check

echo "$NAME: testing complete - Result PASS"
cleanup $STF_PASS
