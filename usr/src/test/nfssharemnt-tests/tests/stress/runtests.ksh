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
[[ :$SHAREMNT_DEBUG: == *:$NAME:* \
	|| :$SHAREMNT_DEBUG: == *:all:* ]] && set -x

################################################################################
#
# __stc_assertion_start
#
# ID: runtests
#
# DESCRIPTION:
#   Print the time of sharing $NUM_SHARES entries on server. For ufs, we write
#   these entries to /etc/dfs/dfstab and perform "shareall" command; For zfs, we
#   just share these entries through setting zfs property "sharenfs" to "on".
#
# STRATEGY:
#   1. share $NUM_SHARES entries on the server and print the time
#   2. On the client, do mount/umount on each exported dirs
#   3. unshare all exported dirs on the server.
#
# TESTABILITY: explicit
#
# TEST_AUTOMATION_LEVEL: automated
#
# __stc_assertion_end
#
################################################################################

function stress_cleanup {
	[[ :$SHAREMNT_DEBUG: = *:$NAME:* || :$SHAREMNT_DEBUG: = *:all:* ]] \
		&& set -x

	if [[ $tag == 001 || $tag == 002 ]]; then
		# do umount on client
		umountall -h $SERVER > $STF_TMPDIR/umountall.out.$$ 2>&1
		mount | grep $STRESSMNT/mntdir_ >> $STF_TMPDIR/umountall.out.$$
		if (( $? == 0 )); then
			echo "$NAME: umountall failed, \c"
			echo "please do cleanup manually"
			cat $STF_TMPDIR/umountall.out.$$
		fi

		# do unshare on server
		typeset SRVDEBUG=$STC_GENUTILS_DEBUG:$SHAREMNT_DEBUG
		[[ :$SRVDEBUG: == *:RSH:* ]] && SRVDEBUG=all
		typeset CMD="export SHAREMNT_DEBUG=$SRVDEBUG; "
		CMD=$CMD"ksh $SRV_TMPDIR/sharemnt.stress -t cleanup_$tag"
		RSH root $SERVER "$CMD" > $STF_TMPDIR/rsh.out.$$ 2>&1
		if (( $? != 0 )); then
			echo "\n$Tname: running <$CMD> failed, please do \c"
			echo "cleanup manually."
			cat $STF_TMPDIR/rsh.out.$$
		fi
		[[ :$SRVDEBUG: == *:all:* ]] && cat $STF_TMPDIR/rsh.out.$$
		sleep 10
	fi

	cleanup $1
}

USAGE="Usage: runtests Test_name tag"

if (( $# < 2 )); then
	echo "$USAGE"
	exit $STF_UNRESOLVED
fi

typeset Tname=$1
typeset tag=$2
ZONENAME=$(zonename)

# NOTICE: When the variable "tag" is 002, we run this script to do
# stress_002 test, which is only for zfs testing. So if we want to change
# the script in future development, we SHOULD make sure it can run
# in both zfs and ufs tests.
if [[ -z $ZFSPOOL && $tag == 002 ]]; then
	echo "\n$Tname: UNTESTED, This is a zfs test case, \c"
	echo "but currently test runs over non-zfs"
	exit $STF_UNTESTED
fi

# NOTICE: When the variable "tag" is 003, we run this script to do
# stress_003 test, which is only for ufs testing at present. With
# zfs, it incurs
# 1. Slow execution. It takes about 1 hour to run with 50 entries
#    in a Sun-Fire-280R server which has 8G mem and 2x750M sparcv9
#    processors, and 2000 zfs are created.
# 2. System error in sharemgr. With 2000 zfs exists, stress_003
#    plays with 100 entries in one round, sharemgr 'set -p' and
#    'remove-share' sometimes complain 'System error'.
# So it will be skipped in zfs. After these issues are resolved,
# we must enable the test again in the future. This test is
# necessary both for zfs and ufs.
if [[ -n $ZFSPOOL && $tag == 003 ]]; then
	echo "\n$Tname: UNTESTED, This is a ufs test case, \c"
	echo "but currently test runs over non-ufs"
	exit $STF_UNTESTED
fi

# NOTICE: When the variable "tag" is 004, we run this script to do
# stress_004 test, which is only testable if sharemgr is available
# on the server.
if [[ $tag == 004 ]]; then
	ck_sharemgr=$(RSH root $SERVER "ls -l /usr/sbin/sharemgr 2>&1")
	if (( $? != 0 )); then
		echo "\n$Tname: RSH failed, $ck_sharemgr"
		exit $STF_UNRESOLVED
	elif [[ $ck_sharemgr == *"No such file or directory"* ]]; then
		echo "\n$Tname: UNTESTED, This is testable only \c"
		echo "if sharemgr is available on the server!\n"
		exit $STF_UNTESTED
	fi
fi

client_num=$(get_clients_num)
if (( $? != 0 )); then
	echo "\n$Tname: RSH failed, $client_num"
	exit $STF_UNRESOLVED
elif (( $client_num != 1 )); then
	echo "\n$Tname: multiple srv_shmnt files were found on the server."
	echo "\tthe stress tests don't support multiple clients\n"
	exit $STF_UNTESTED
fi

mount | grep $STRESSMNT/mntdir_ > $STF_TMPDIR/mount.out.$$ 2>&1
if (( $? == 0 )); then
	echo "\n$Tname: Some test dirs were mounted on client \c"
	echo "please umount them before running the test"
	cat $STF_TMPDIR/mount.out.$$
	cleanup $STF_UNINITIATED
fi

SRVDEBUG=$STC_GENUTILS_DEBUG:$SHAREMNT_DEBUG
[[ :$SRVDEBUG: == *:RSH:* ]] && SRVDEBUG=all
RSH root $SERVER \
	"if [[ -f $SRV_TMPDIR/sharemnt.stress ]]; then \
	export SHAREMNT_DEBUG=$SRVDEBUG; \
	$SRV_TMPDIR/sharemnt.stress -t stress_$tag; else \
	echo UNTESTED; fi" \
	> $STF_TMPDIR/rsh.out.$$ 2>&1 &
pid=$!
condition="cat $STF_TMPDIR/rsh.out.$$ | grep -v ^+ \
	| egrep \"ERROR|PASS|UNTESTED\" > /dev/null"
wait_now $STRESS_TIMEOUT "$condition"
if (( $? == 0 )); then
	cat $STF_TMPDIR/rsh.out.$$ | grep -v ^+ | grep PASS > /dev/null
	if (( $? == 0 )); then
		cat $STF_TMPDIR/rsh.out.$$
		[[ $tag == 003 || $tag == 004 ]] && stress_cleanup $STF_PASS
	else
		cat $STF_TMPDIR/rsh.out.$$ | grep -v ^+ | \
		    grep UNTESTED > /dev/null
		if (( $? == 0 )); then
		    echo "\n$Tname: no $STF_TMPDIR/sharemnt.stress in \c"
		    echo "$SERVER. Maybe multiple clients are running."
		    cleanup $STF_UNTESTED
		else
		    echo "\n$Tname: run $STF_TMPDIR/sharemnt.stress in \c"
		    echo "$SERVER failed:"
		    cat $STF_TMPDIR/rsh.out.$$
		    stress_cleanup $STF_FAIL
		fi
	fi
elif [[ $tag == 003 || $tag == 004 ]]; then
	echo "$Tname: run test on server timeout<$STRESS_TIMEOUT secs>"
	cat $STF_TMPDIR/rsh.out.$$
	stress_cleanup $STF_FAIL
else
	echo "$Tname: run test on server timeout<$STRESS_TIMEOUT secs>"
	cat $STF_TMPDIR/rsh.out.$$
	# We still have a chance to check exported entries.
	share_num=$(RSH root $SERVER "cat /etc/dfs/sharetab \
		| grep $STRESSDIR/sharemnt_ | wc -l" | nawk '{print $1}' \
		2>/dev/null)
	expected=$((NUM_SHARES+1))
	if [[ $expected != $share_num ]]; then
		echo "\texepected $expected directories were exported, \c"
		echo "but got $share_num"
		kill -KILL $pid
		stress_cleanup $STF_FAIL
	fi
fi

# define mount options with an array
set -A OPTS sec=sys ro hard proto=tcp proto=udp
set -A VERS vers=4 vers=3 vers=2

i=0
while (( $i <= $NUM_SHARES )); do
	m=$((RANDOM % 5))
	n=$((RANDOM % 3))
	is_cipso "$Mntopts" "$SERVER"
	if (( $? != 0 )); then
		[[ ${OPTS[$m]} == "proto=udp" ]] && opt="" || opt=${OPTS[$m]}
		ver="vers=4"
	elif [[ ${OPTS[$m]} == "proto=udp" && ${VERS[$n]} == "vers=4" ]]; then
		opt=""
		ver="vers=4"
	else
		opt=${OPTS[$m]}
		ver=${VERS[$n]}
	fi

	[[ -n $opt ]] && opt=",$opt"
	mount -o $ver$opt $SERVER:$STRESSDIR/sharemnt_${i}_stress \
		$STRESSMNT/mntdir_${i}_stress >> $STF_TMPDIR/mount.out.$$ 2>&1 &
	i=$((i+1))
done

# wait for all mount commands in background to finish
condition="! pgrep -z $ZONENAME -P $$ -x mount > /dev/null"
wait_now $STRESS_TIMEOUT "$condition"
if (( $? != 0 )); then
	echo "$Tname: timeout<$STRESS_TIMEOUT secs> for mounting..."
	ps -efz $ZONENAME | grep "mount"
	cat $STF_TMPDIR/mount.out.$$
	pkill -z $ZONENAME -P $$ -x mount
	stress_cleanup $STF_FAIL
fi

num=$(mount | grep $STRESSMNT/mntdir_ | wc -l)
if (( $num != $i )); then
	echo "$Tname: mount was unsuccessful"
	echo "\tExpected to see $i entries, but got $num"
	cat $STF_TMPDIR/mount.out.$$
	mount | grep $STRESSMNT/mntdir_
	stress_cleanup $STF_FAIL
fi

stress_cleanup $STF_PASS
