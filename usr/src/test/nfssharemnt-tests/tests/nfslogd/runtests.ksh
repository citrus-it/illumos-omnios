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
#   For each log tag in nfslog.conf, combine different nfs version.
#   Do some simple rw operation and verify the operation is logged.
#
# STRATEGY:
#   1. Call domount_check to mount test filesystem and verify
#   2. Call do_rw_test script to test in the mounted filesystem
#   3. Call unmount_check to umount and check the filesystem is umounted
#   4. Call test_nfslogd to verify logging. 
#
# TESTABILITY: explicit
#
# TEST_AUTOMATION_LEVEL: automated
#
# __stc_assertion_end
#
################################################################################

USAGE="Usage: runtests Test_name Log_tag Share_opt Mnt_opts"

if (( $# < 4 )); then
	echo "$USAGE"
	exit $STF_UNRESOLVED
fi

typeset Tname=$1
typeset Tag=$2
typeset Shropt=$3
typeset Mntopts=$4
typeset Lock_Dir=/var/tmp/sharemnt_lock

is_cipso "$Mntopts" "$SERVER"
run_result=$?
if (( $run_result == 3 )); then
        echo "$NAME: UNSUPPORTED"
        echo "\tCurrently nfslogd only support NFSv2 and NFSv3, "
        echo "\tNFSv4 testing with Trusted Extensions is not supported" 
        exit $STF_UNSUPPORTED
elif (( $run_result == 1 )); then
	echo "$NAME: UNSUPPORTED"
	echo "\tCurrently only NFSv3 and NFSv4 are supported under TX"
        exit $STF_UNSUPPORTED
fi

# make sure nfslogd is running on server before test start
SRVDEBUG=$SHAREMNT_DEBUG
[[ :$SHAREMNT_DEBUG: == *:RSH:* ]] && SRVDEBUG=all
RSH root $SERVER \
    "export SHAREMNT_DEBUG=$SRVDEBUG; \
    ksh $SRV_TMPDIR/sharemnt.nfslogd -C" \
    > $STF_TMPDIR/rsh.out.$$ 2>&1
if (( $? != 0 )); then
    echo "\n$Tname: run $SRV_TMPDIR/sharemnt.nfslogd in $SERVER failed:"
    cat $STF_TMPDIR/rsh.out.$$
    cleanup $STF_FAIL
else
	print_debug $STF_TMPDIR/rsh.out.$$
fi

# The function returns the content of lock file on server.
function read_lock_file {
	typeset lockfile=$1

	value=$(RSH root $SERVER "cat $Lock_Dir/$lockfile" 2>/dev/null)
	if (( $? != 0 )); then
		echo "\n$Tname: failed to read lock file<$Lock_Dir/$lockfile"
		echo $value
		cleanup $STF_UNINITIATED
	fi

	echo $value
}

# get the number of client
client_num=$(read_lock_file ".stf_unconfigure")

Timeout=$((1800*$client_num))
interval=$((5*$client_num))
do_exec=0
waited_time=0
while (( $waited_time <= $Timeout )); do
	ref_exec=$(read_lock_file ".stf_execute")
	if [[ $ref_exec == 0 ]]; then
		# write pid to lock file and occupy the lock.
		RSH root $SERVER \
			"echo ${CLIENT_S}_$$ > $Lock_Dir/.stf_execute; sync" \
			> $STF_TMPDIR/rsh.out.$$ 2>&1
		if (( $? != 0 )); then
			echo "\n$Tname: failed to write lock file"
			cat $STF_TMPDIR/rsh.out.$$
			cleanup $STF_UNINITIATED
		fi

		# make sure to release the lock
		# If there are mutiple clients, sleep for a while to let other
		# clients get the lock and have a chance to be scheduled.
		trap "RSH root $SERVER \"echo 0 > $Lock_Dir/.stf_execute\" && \
			sync && [[ $client_num != 1 ]] && \
			sleep $((interval+7))" \
			0 1 2 15

		# make sure that other clients do not overwrite the lock file
		if (( $client_num != 1 )); then
			sleep 3
			ref_exec=$(read_lock_file ".stf_execute")
			[[ $ref_exec != ${CLIENT_S}_$$ ]] && continue
		fi

		do_exec=1
		break
	else
		sleep $interval
		waited_time=$((waited_time + $interval))
	fi
done

if (( $do_exec == 0 )); then
	echo "\n$TNAME: The case can not get the lock after sleep $Timeout secs"
	cleanup $STF_UNINITIATED
fi

share_check "$Shropt" "$NFSLOGDDIR"
domount_check "$Mntopts" "$Shropt" "$NFSLOGDDIR"
do_rw_test "tfile.$$"
unmount_check

echo "Verify write operations logged correctly in logfile ... \c"
sleep 5
SRVDEBUG=$SHAREMNT_DEBUG
[[ :$SHAREMNT_DEBUG: == *:RSH:* ]] && SRVDEBUG=all
RSH root $SERVER \
    "export SHAREMNT_DEBUG=$SRVDEBUG; \
    ksh $SRV_TMPDIR/test_nfslogd $Tname tfile.$$ $Tag 1 1 1 ${SHAREMNT_DEBUG}" \
    > $STF_TMPDIR/rsh.out.$$ 2>&1
if (( $? != 0 )); then
    echo "\n$Tname: run $SRV_TMPDIR/test_nfslogd in $SERVER failed:"
    cat $STF_TMPDIR/rsh.out.$$
    cleanup $STF_FAIL
else
	print_debug $STF_TMPDIR/rsh.out.$$
    echo "OK"
fi

echo "$Tname: testing complete - Result PASS"
cleanup $STF_PASS
