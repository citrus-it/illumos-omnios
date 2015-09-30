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
# ID: access_test
#
# DESCRIPTION:
#   For the mount and export the SERVER's filesystems with
#   ro|rw|root=access-list
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

# Function to do root permission testing in the NFS mount point
# and auto mount point
#   Usage: do_root_test exp_root exp_rw auto_mntdir
#
function do_root_test {
	Fname=do_root_test
	[[ :$SHAREMNT_DEBUG: == *:${NAME}:* \
		|| :${SHAREMNT_DEBUG}: == *:all:* ]] && set -x
	typeset exp_root=${1}
	typeset exp_rw=${2}
	typeset auto_mntdir=${3}

	for mntdir in $MNTDIR $auto_mntdir; do
	    echo "Doing ROOT access testing at $mntdir ... \c"
	    ls -lv $mntdir/nopermfile 2> $STF_TMPDIR/ls.out.$$ | \
		grep "nopermfile" > /dev/null 2>&1
	    if [[ $? != 0 ]]; then
		echo "\n$Fname: READDIR of <nopermfile> in $mntdir failed"
		cat $STF_TMPDIR/ls.out.$$
		cleanup $STF_FAIL
	    fi
	    typeset -i TLine=22
	    typeset -i fline
	    fline=$(wc -l 2> $STF_TMPDIR/wc.out.$$ < $mntdir/nopermfile)
	    [[ $? == 0 ]] && rc=0 || rc=1
	    if [[ $rc != $exp_root ]]; then
		echo "\n$Fname: not the expected result: "
		if [[ $rc == 0 ]]; then
			echo "\texpected READ of <nopermfile> failed"
		else
			echo "\texpected READ of <nopermfile> successful"
			cat $STF_TMPDIR/wc.out.$$
		fi
		cleanup $STF_FAIL
	    fi

	    if (( $rc == 0 && TLine != fline )); then
		echo "\n$Fname: number of line of nopermfile is wrong"
		echo "\t it is $fline instead of $TLine"
		cleanup $STF_FAIL
	    fi

	    if [[ $exp_rw == "rw" ]]; then
		echo "Checking root can create new file at $mntdir if the \c"
		echo "client is granted with rw access;"
		echo "and checking the created file is owned by root if it \c"
		echo "is granted with root access ..."
		typeset TData="ROOT WRITE tests at $mntdir"
		echo "$TData" > $mntdir/tfile.$$ 2> $STF_TMPDIR/wr.out.$$
		if [[ $? != 0 ]]; then
			echo "\n$Fname: WRITE to $mntdir failed"
			cat $STF_TMPDIR/wr.out.$$
			cleanup $STF_FAIL
		fi
		owner=$(ls -l $mntdir/tfile.$$ 2> $STF_TMPDIR/ls.out.$$ | \
			awk '{print $3}')
		if [[ $? != 0 ]]; then
			echo "\n$Fname: READDIR of <tfile.$$> in $mntdir failed"
			cat $STF_TMPDIR/ls.out.$$
			cleanup $STF_FAIL
		fi
		[[ $owner == "root" ]] && rc=0 || rc=1
		if [[ $rc != $exp_root ]]; then
			echo "\n$Fname: not the expected result: "
			if [[ $rc == 0 ]]; then
				echo "\texpected the file owner is not root"
			else
				echo "\texpected the file owner is root"
			fi
			ls -l $mntdir/tfile.$$
			cleanup $STF_FAIL
		fi
	    fi
	done
	echo "OK"
}

USAGE="Usage: $0 Test_name Share_opt Mnt_opts Shrexp_opt Root_opt"

if (( $# < 5 )); then
	echo "$USAGE"
	exit $STF_UNRESOLVED
fi

typeset Tname=$1
typeset Shropt=$2
typeset Mntopts=$3
typeset Shrexpopt=$4
typeset Rootopt=$5
# Rootopt=root --> have root permission
# Rootopt=noroot --> don't have root permission

# Check TX related info
check_for_cipso "$SHRDIR" "$MNTDIR" "$Mntopts" || return $STF_UNSUPPORTED

[[ ${Mntopts%,*} == "ro" ]] && clnt_opt=ro || clnt_opt=rw

# get what type operation is valid on client
[[ $Shrexpopt == "rw" && $clnt_opt == "rw" ]] && exp_opt=rw || exp_opt=ro

# whether grant root access on client
[[ $Rootopt == "root" ]] && root_opt=0 || root_opt=1

share_check "$Shropt"
domount_check "$Mntopts" "$Shropt"
automount_check "$Mntopts" "$Shropt" "ACCESS"

typeset an=$(echo "$Mntopts" | sed -e 's/sec=//g' -e 's/://g')
do_root_test $root_opt $exp_opt "$AUTOIND/SM_ACCESS_$$_$an"

if [[ $exp_opt == "ro" ]]; then
	do_ro_test

	set -A molist remount remount,rw rw,remount
	domount_check ${molist[$((RANDOM % 3 ))]}
	echo $Shropt | grep -w ro > /dev/null 2>&1
	(( $? != 0 )) && do_rw_test
else
	do_rw_test

	set -A molist remount,ro ro,remount
	do_neg_mount_check ${molist[$((RANDOM % 2 ))]}
fi

unmount_check

echo "$NAME: testing complete - Result PASS"
cleanup $STF_PASS
