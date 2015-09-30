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

#
# Setup the SERVER for stress test
#

NAME=$(basename $0)
PROG=$0

Usage="Usage: $NAME -s | -c | -t OPTARG | -h path\n
		-s: to setup this host for stress test\n
		-c: to cleanup the server for stress test\n
		-t: to run stress_001/2/3/4 or cleanup_001/2\n
		-h: to share/unshare path\n
"
if (( $# < 1 )); then
	echo $Usage
	exit 99
fi

# variables gotten from client system:
STF_TMPDIR=STF_TMPDIR_from_client
SHAREMNT_DEBUG=${SHAREMNT_DEBUG:-"SHAREMNT_DEBUG_from_client"}
STRESS_TIMEOUT=STRESS_TIMEOUT_from_client
NUM_SHARES=NUM_SHARES_from_client
NUM_GROUPS=NUM_GROUPS_from_client
NUM_ENTRYS=NUM_ENTRYS_from_client

ZONENAME=$(zonename)
NUM=$((NUM_GROUPS * NUM_ENTRYS))

. $STF_TMPDIR/srv_config.vars

# Include common STC utility functions
if [[ -s $STC_GENUTILS/include/nfs-util.kshlib ]]; then
	. $STC_GENUTILS/include/nfs-util.kshlib
else
	. $STF_TMPDIR/nfs-util.kshlib
fi

# cleanup function on all exit
function cleanup {
	[[ :$SHAREMNT_DEBUG: == *:$NAME:* \
		|| :$SHAREMNT_DEBUG: == *:all:* ]] && set -x

	# Restore original /etc/dfs/dfstab files
	[[ $2 == "t" && -f /etc/dfs/dfstab.sharemnt.stress ]] && \
		mv /etc/dfs/dfstab.sharemnt.stress /etc/dfs/dfstab

	rm -fr $STF_TMPDIR/*.$$
	exit $1
}

# do clean up of share groups forcibly
function cleanup_sharemgr {
	[[ :$SHAREMNT_DEBUG: == *:$NAME:* \
		|| :$SHAREMNT_DEBUG: == *:all:* ]] && set -x

	sharemgr list -P nfs | grep "^shmgr_grp_" > $STF_TMPDIR/grp.out.$$
	typeset -i num=$(wc -l $STF_TMPDIR/grp.out.$$ | nawk '{print $1}')
	if (( $num > 0 )); then
		while read agroup; do
			sharemgr delete -f $agroup \
				>> $STF_TMPDIR/grp.del.$$ 2>&1 &
		done < $STF_TMPDIR/grp.out.$$
	fi
	# wait for all delete commands in background to finish
	condition="! pgrep -z $ZONENAME -P $$ -x sharemgr > /dev/null"
	wait_now $STRESS_TIMEOUT "$condition" 3
	if (( $? != 0 )); then
		echo "ERROR: timeout<$STRESS_TIMEOUT secs> for \c"
		echo "deleting share groups..."
		ps -efz $ZONENAME | grep "sharemgr"
		cat $STF_TMPDIR/grp.del.$$
		pkill -z $ZONENAME -P $$ -x sharemgr
	fi
	# check there is no any group more
	sharemgr list -P nfs | grep "^shmgr_grp_" > $STF_TMPDIR/grp.out.$$
	num=$(wc -l $STF_TMPDIR/grp.out.$$ | nawk '{print $1}')
	if (( $num > 0 )); then
		echo "ERROR: Still find $num groups remained"
		echo "\tyou need to cleanup them manually!"
		cat $STF_TMPDIR/grp.out.$$
	fi

	cleanup 1
}

# Turn on debug info, if requested
export STC_GENUTILS_DEBUG=$SHAREMNT_DEBUG
[[ :$SHAREMNT_DEBUG: == *:$NAME:* \
	|| :$SHAREMNT_DEBUG: == *:all:* ]] && set -x

getopts sct:h: opt
case $opt in
s)
	if [[ ! -d $STRESSDIR ]]; then
		mkdir -pm 0777 $STRESSDIR
		(( $? != 0 )) && \
			echo "could not create $STRESSDIR" && exit 1
	fi

	typeset -i i=0
	typeset -i j=$((NUM_SHARES + 1))
	(( $j < $NUM )) && j=$NUM
	while (( $i < $j )); do
		mkdir -p $STRESSDIR/sharemnt_${i}_stress

		# set up ZFS
		if [[ -n $ZFSPOOL ]] && (( $i <= $NUM_SHARES )); then
			create_zfs_fs $ZFSBASE $STRESSDIR/sharemnt_${i}_stress \
				> $STF_TMPDIR/zfs.out.$$ 2>&1
			if (( $? != 0 )); then
				echo "$NAME: failed to create_zfs_fs \c"
				echo "$STRESSDIR/$sharemnt_${i}_stress"
				cat $STF_TMPDIR/zfs.out.$$
				cleanup 2
			fi
		fi
		let i+=1
	done
	echo "Done - Setup OKAY"
	;;
t)
	# define share options with an array
	set -A SHOPT rw ro anon=0 nosuid ro=$CLIENT_S sec=sys

	# make sure there is no test dirs exported on server
	if [[ $OPTARG != cleanup* ]]; then
		cat $SHARETAB | grep $STRESSDIR/sharemnt_ \
			> $STF_TMPDIR/share.out.$$ 2>&1
		if (( $? == 0 )); then
			echo "ERROR: Some test dirs<$STRESSDIR/sharemnt_> \c"
			echo " were exported before the testing, \c"
			echo " please unshare them."
			cat $STF_TMPDIR/share.out.$$
			cleanup 1
		fi
	fi

	case $OPTARG in
	stress_001)
		mv /etc/dfs/dfstab /etc/dfs/dfstab.sharemnt.stress \
			> $STF_TMPDIR/dfstab.backup.$$ 2>&1
		if (( $? != 0 )); then
			echo "ERROR: failed to backup /etc/dfs/dfstab"
			cat $STF_TMPDIR/dfstab.backup.$$
			cleanup 1 t
		fi

		typeset -i i=0
		touch /etc/dfs/dfstab
		while (( $i < $NUM_SHARES )); do
			let k=i%6
			echo "share -o ${SHOPT[$k]} $STRESSDIR/sharemnt_${i}_stress" \
				>> /etc/dfs/dfstab
			let i+=1
		done
		entries=$(wc -l /etc/dfs/dfstab | nawk '{print $1}')
		if (( $entries != $NUM_SHARES )); then
			echo "ERROR: failed to add $NUM_SHARES entries in \c"
			echo "/etc/dfs/dfstab, got $entries entries"
			echo "======== /etc/dfs/dfstab ========="
			cat /etc/dfs/dfstab
			echo "===================================="
			cleanup 1 t
		fi

		ksh "time shareall" > $STF_TMPDIR/time.out.$$ 2>&1
		if (( $? != 0 )); then
			echo "ERROR: shareall failed."
			cat $STF_TMPDIR/time.out.$$
			cat $SHARETAB
			cleanup 1 t
		fi

		# check the filesystems are shared successfully
		condition="(( \$(cat $SHARETAB \
			| grep $STRESSDIR/sharemnt_ | wc -l \
			| nawk '{print \$1}') == $NUM_SHARES ))"
		wait_now 600 "$condition"
		if (( $? != 0 )); then
			num=$(cat $SHARETAB \
				| grep $STRESSDIR/sharemnt_ \
				| wc -l | nawk '{print $1}')
			echo "ERROR: expected $NUM_SHARES entries in \c"
			echo "$SHARETAB, but got $num entries"
			echo "======== $SHARETAB ========="
			cat $SHARETAB
			echo "======== /etc/dfs/dfstab ========="
			cat /etc/dfs/dfstab
			echo "===================================="
			cleanup 1 t
		fi

		# also check all filesystems are shared with correct options
		i=0
		while (( $i < $NUM_SHARES )); do
			k=$((i % 6))
			grep $STRESSDIR/sharemnt_${i}_stress $SHARETAB \
				> $STF_TMPDIR/share.out.$$ 2>&1
			if (( $? != 0 )); then
				echo "ERROR: $STRESSDIR/sharemnt_${i}_stress \c"
				echo "wasn't shared"
				cleanup 1 t
			fi
			grep ${SHOPT[$k]} $STF_TMPDIR/share.out.$$ \
				> /dev/null 2>&1
			if (( $? != 0 )); then
				echo "ERROR: $STRESSDIR/sharemnt_${i}_stress \c"
				echo "shared with incorrect option, expected \c"
				echo "${SHOPT[$k]}, but got :"
				cat $STF_TMPDIR/share.out.$$
				cleanup 1 t
			fi

			i=$((i+1))
		done

		echo "\nThe time of sharing $NUM_SHARES entries :"
		cat $STF_TMPDIR/time.out.$$
	
		ksh "time share -F nfs $STRESSDIR/sharemnt_${i}_stress" \
			> $STF_TMPDIR/time.out.$$ 2>&1
		if (( $? != 0 )); then
			echo "ERROR: failed to share \c"
			echo "$STRESSDIR/sharemnt_${i}_stress"
			cat $STF_TMPDIR/time.out.$$
			cleanup 1 t
		fi

		echo "\nThe time of sharing one more entry :"
		cat $STF_TMPDIR/time.out.$$
		echo " "
		echo "$NAME: share testing complete - Result PASS"

		cleanup 0 t
		;;
	stress_002)
		mv /etc/dfs/dfstab /etc/dfs/dfstab.sharemnt.stress \
			> $STF_TMPDIR/dfstab.backup.$$ 2>&1
		if (( $? != 0 )); then
			echo "ERROR: failed to backup /etc/dfs/dfstab"
			cat $STF_TMPDIR/dfstab.backup.$$
			cleanup 1 t
		fi
		touch /etc/dfs/dfstab

		# set sharenfs to on, then all zfs are shared
		ksh "time zfs set sharenfs=on $ZFSBASE" \
			> $STF_TMPDIR/time.out.$$ 2>&1
		if (( $? != 0 )); then
			echo "ERROR: setting sharenfs=on failed."
			cat $STF_TMPDIR/time.out.$$
			cat $SHARETAB
			cleanup 1 t
		fi

		# Normally, new created zfs are all shared, so we can
		# get $NUM_SHARES + 1 entries. But for zfs, occasionally
		# some entries are duplicated so here we only check if the
		# shared number is more than $NUM_SHARES.
		# The check is not enough due to the duplication,
		# if there aren't $NUM_SHARES+1 entries directories exported,
		# we still meet failures when mounting these dirs.
		condition="(( \$(cat $SHARETAB \
			| grep $STRESSDIR/sharemnt_ | wc -l \
			| nawk '{print \$1}') > $NUM_SHARES ))"
		wait_now 600 "$condition"
		if (( $? != 0 )); then
			expected=$((NUM_SHARES + 1))
			num=$(cat $SHARETAB \
				| grep $STRESSDIR/sharemnt_ \
				| wc -l | nawk '{print $1}')
			echo "ERROR: expected $expected entries in \c"
			echo "$SHARETAB, but got $num entries"
			echo "======== $SHARETAB ========="
			cat $SHARETAB
			echo "===================================="
			cleanup 1 t
		fi

		echo "\nThe time of sharing $NUM_SHARES entries :"
		cat $STF_TMPDIR/time.out.$$

		unshare $STRESSDIR/sharemnt_0_stress \
			> $STF_TMPDIR/unshare.out.$$ 2>&1
		sleep 10
		cat $SHARETAB | grep $STRESSDIR/sharemnt_0_stress \
			> /dev/null 2>&1
		if (( $? == 0 )); then
			echo "ERROR: failed to unshare \c"
			echo "$STRESSDIR/sharemnt_0_stress"
			cat $STF_TMPDIR/unshare.out.$$
			cleanup 1 t
		fi

		ksh "time share -F nfs $STRESSDIR/sharemnt_0_stress" \
			> $STF_TMPDIR/time.out.$$ 2>&1
		if (( $? != 0 )); then
			echo "ERROR: failed to share \c"
			echo "$STRESSDIR/sharemnt_0_stress"
			cat $STF_TMPDIR/time.out.$$
			cleanup 1 t
		fi

		echo "\nThe time of sharing one more entry :"
		cat $STF_TMPDIR/time.out.$$
		echo " "
		echo "$NAME: share testing complete - Result PASS"

		cleanup 0 t
		;;
	stress_003)
		typeset -i i=0 k=0 j=0 num_in_grp=5 Total=200
		while (( i < Total )); do
			let k=i%6
			$PROG -h ${SHOPT[$k]} $STRESSDIR/sharemnt_${i}_stress \
				>> $STF_TMPDIR/stress.out.$$ 2>&1 &
			let i+=1
			let j+=1
			if (( j == num_in_grp || i == Total )); then
				j=0
				wait
			fi
		done

		grep "ERROR:" $STF_TMPDIR/stress.out.$$ > /dev/null 2>&1
		if (( $? == 0 )); then
			echo "ERROR: some share/unshare failed."
			cat $STF_TMPDIR/stress.out.$$
			cleanup 1
		fi

		echo "$NAME: share/unshare testing complete - Result PASS"
		cleanup 0
		;;
	stress_004)
		# make sure there is no test group created on server
		sharemgr list -P nfs | grep "^shmgr_grp_" \
			> $STF_TMPDIR/grp.out.$$
		num=$(wc -l $STF_TMPDIR/grp.out.$$ | nawk '{print $1}')
		if (( $num > 0 )); then
			echo "ERROR: $num test groups<shmgr_grp_> \c"
			echo " were created before the testing, \c"
			echo " please delete them."
			cat $STF_TMPDIR/grp.out.$$
			cleanup 1
		fi

		# step 1. create groups:
		typeset -i i=0
		while (( $i <= $NUM_GROUPS )); do
			sharemgr create -P nfs shmgr_grp_$i \
				>> $STF_TMPDIR/grp.create.$$ 2>&1 &
			i=$((i + 1))
		done
		# wait for all create commands in background to finish
		condition="! pgrep -z $ZONENAME -P $$ -x sharemgr > /dev/null"
		wait_now $STRESS_TIMEOUT "$condition" 3
		if (( $? != 0 )); then
			echo "ERROR: timeout<$STRESS_TIMEOUT secs> for \c"
			echo "creating share groups..."
			ps -efz $ZONENAME | grep "sharemgr"
			cat $STF_TMPDIR/grp.create.$$
			pkill -z $ZONENAME -P $$ -x sharemgr
			cleanup_sharemgr
		fi
		# check number of created groups
		sharemgr list -v -P nfs | grep "^shmgr_grp_" \
			> $STF_TMPDIR/grp.out.$$
		num=$(grep -w enabled $STF_TMPDIR/grp.out.$$ | wc -l \
			| nawk '{print $1}')
		expected=$((NUM_GROUPS + 1))
		if (( $num != $expected )); then
			echo "ERROR: sharemgr create group was unsuccessful"
			echo "\tExpected to see $expected enabled groups, \c"
			echo "but got $num"
			cat $STF_TMPDIR/grp.out.$$
			cleanup_sharemgr
		fi

		# step 2. add-share:
		i=0
		while (( $i < $NUM )); do
			sharemgr add-share -s $STRESSDIR/sharemnt_${i}_stress \
				shmgr_grp_$NUM_GROUPS >> \
				$STF_TMPDIR/grp.addshr.$$ 2>&1 &
			i=$((i + 1))
		done
		# wait for all add-share commands in background to finish
		condition="! pgrep -z $ZONENAME -P $$ -x sharemgr > /dev/null"
		wait_now $STRESS_TIMEOUT "$condition" 3
		if (( $? != 0 )); then
			echo "ERROR: timeout<$STRESS_TIMEOUT secs> for \c"
			echo "adding share pathes to group..."
			ps -efz $ZONENAME | grep "sharemgr"
			cat $STF_TMPDIR/grp.addshr.$$
			pkill -z $ZONENAME -P $$ -x sharemgr
			cleanup_sharemgr
		fi
		# check number of entries in the last group
		sharemgr show shmgr_grp_$NUM_GROUPS | grep -v \
		    "^shmgr_grp_$NUM_GROUPS" > $STF_TMPDIR/entry.out.$$
		num=$(wc -l $STF_TMPDIR/entry.out.$$ | nawk '{print $1}')
		if (( $num != $NUM )); then
			echo "ERROR: sharemgr add-share was unsuccessful"
			echo "\tExpected to see $NUM entries in the last \c"
			echo "group, but got $num"
			cat $STF_TMPDIR/entry.out.$$
			cleanup_sharemgr
		fi
		# check number of shares
		grep $STRESSDIR/sharemnt_ $SHARETAB > $STF_TMPDIR/share.out.$$
		num=$(wc -l $STF_TMPDIR/share.out.$$ | nawk '{print $1}')
		if (( $num != $NUM )); then
			echo "ERROR: sharemgr add-share was unsuccessful"
			echo "\tExpected to see $NUM shares, but got $num"
			cat $STF_TMPDIR/share.out.$$
			cleanup_sharemgr
		fi

		# step 3. move-share:
		i=0
		while (( $i < $NUM )); do
			k=$((i % NUM_GROUPS))
			sharemgr move-share -s $STRESSDIR/sharemnt_${i}_stress \
				shmgr_grp_$k >> $STF_TMPDIR/grp.mvshr.$$ 2>&1 &
			i=$((i + 1))
		done
		# wait for all move-share commands in background to finish
		condition="! pgrep -z $ZONENAME -P $$ -x sharemgr > /dev/null"
		wait_now $STRESS_TIMEOUT "$condition" 3
		if (( $? != 0 )); then
			echo "ERROR: timeout<$STRESS_TIMEOUT secs> for \c"
			echo "moving share pathes to different groups..."
			ps -efz $ZONENAME | grep "sharemgr"
			cat $STF_TMPDIR/grp.mvshr.$$
			pkill -z $ZONENAME -P $$ -x sharemgr
			cleanup_sharemgr
		fi
		# check number of entries in each group
		i=0
		while (( $i < $NUM_GROUPS )); do
			sharemgr show shmgr_grp_$i | grep -v "^shmgr_grp_$i" \
				> $STF_TMPDIR/entry.out.$$
			num=$(wc -l $STF_TMPDIR/entry.out.$$ | \
			    nawk '{print $1}')
			if (( $num != $NUM_ENTRYS )); then
			    echo "ERROR: sharemgr move-share was unsuccessful"
			    echo "\tExpected to see $NUM_ENTRYS entries in \c"
			    echo "shmgr_grp_$i group, but got $num"
			    cat $STF_TMPDIR/entry.out.$$
			    cleanup_sharemgr
			fi
			i=$((i + 1))
		done
		# check there is no entry in the last group now
		sharemgr show shmgr_grp_$NUM_GROUPS | grep -v \
		    "^shmgr_grp_$NUM_GROUPS" > $STF_TMPDIR/entry.out.$$
		num=$(wc -l $STF_TMPDIR/entry.out.$$ | nawk '{print $1}')
		if (( $num > 0 )); then
			echo "ERROR: sharemgr move-share was unsuccessful"
			echo "\tStill find $num entries in the last group"
			cat $STF_TMPDIR/entry.out.$$
			cleanup_sharemgr
		fi
		# check number of shares again
		grep $STRESSDIR/sharemnt_ $SHARETAB > $STF_TMPDIR/share.out.$$
		num=$(wc -l $STF_TMPDIR/share.out.$$ | nawk '{print $1}')
		if (( $num != $NUM )); then
			echo "ERROR: sharemgr move-share was unsuccessful"
			echo "\tExpected to see $NUM shares, but got $num"
			cat $STF_TMPDIR/share.out.$$
			cleanup_sharemgr
		fi

		# step 4. set-share:
		# NOTICE: -r resource-name is not checked due to bug 6654535,
		# after this issue is resolved, we must enable this check
		# again in the future.
		i=0
		while (( $i < $NUM )); do
			k=$((i % NUM_GROUPS))
			sharemgr set-share -d "directory $i" \
				-s $STRESSDIR/sharemnt_${i}_stress shmgr_grp_$k \
				>> $STF_TMPDIR/grp.setshr.$$ 2>&1 &
			i=$((i + 1))
		done
		# wait for all set-share commands in background to finish
		condition="! pgrep -z $ZONENAME -P $$ -x sharemgr > /dev/null"
		wait_now $STRESS_TIMEOUT "$condition" 3
		if (( $? != 0 )); then
			echo "ERROR: timeout<$STRESS_TIMEOUT secs> for \c"
			echo "setting entries' properties..."
			ps -efz $ZONENAME | grep "sharemgr"
			cat $STF_TMPDIR/grp.setshr.$$
			pkill -z $ZONENAME -P $$ -x sharemgr
			cleanup_sharemgr
		fi
		# check property of each entry
		sharemgr show -v -P nfs > $STF_TMPDIR/entry.out.$$
		i=0
		while (( $i < $NUM ));do
			grep "$STRESSDIR/sharemnt_${i}_stress" \
			    $STF_TMPDIR/entry.out.$$ | grep "directory $i" \
			    > /dev/null
			if (( $? != 0 )); then
			    echo "ERROR: sharemgr set-share was unsuccessful"
			    echo "\tCannot find \"directory $i\" property \c"
			    echo "from $STRESSDIR/sharemnt_${i}_stress entry"
			    cat $STF_TMPDIR/entry.out.$$
			    cleanup_sharemgr
			fi
			i=$((i + 1))
		done

		# step 5. set groups:
		i=0
		while (( $i <= $NUM_GROUPS )); do
			sharemgr set -P nfs -p anon="1234" shmgr_grp_$i
				>> $STF_TMPDIR/grp.set.$$ 2>&1 &
			i=$((i + 1))
		done
		# wait for all set commands in background to finish
		condition="! pgrep -z $ZONENAME -P $$ -x sharemgr > /dev/null"
		wait_now $STRESS_TIMEOUT "$condition" 3
		if (( $? != 0 )); then
			echo "ERROR: timeout<$STRESS_TIMEOUT secs> for \c"
			echo "setting groups' properties..."
			ps -efz $ZONENAME | grep "sharemgr"
			cat $STF_TMPDIR/grp.set.$$
			pkill -z $ZONENAME -P $$ -x sharemgr
			cleanup_sharemgr
		fi
		# check property of each group
		i=0
		while (( $i <= $NUM_GROUPS )); do
			sharemgr show -p shmgr_grp_$i > $STF_TMPDIR/grp.prop.$$
			grep "^shmgr_grp_$i " $STF_TMPDIR/grp.prop.$$ | grep \
				"nfs=(anon=\"1234\")" > /dev/null
			if (( $? != 0 )); then
			    echo "ERROR: sharemgr set group was unsuccessful"
			    echo "\tCannot find nfs=(anon=\"1234\") property \c"
			    echo "from shmgr_grp_$i group"
			    cat $STF_TMPDIR/grp.prop.$$
			    cleanup_sharemgr
			fi
			i=$((i + 1))
		done
		# check property of each entry
		grep $STRESSDIR/sharemnt_ $SHARETAB > $STF_TMPDIR/entry.prop.$$
		i=0
		while (( $i < $NUM ));do
			grep "$STRESSDIR/sharemnt_${i}_stress" \
			    $STF_TMPDIR/entry.prop.$$ | grep "anon=1234" \
			    > /dev/null
			if (( $? != 0 )); then
			    echo "ERROR: sharemgr set group was unsuccessful"
			    echo "\tCannot find \"anon=1234\" property \c"
			    echo "from $STRESSDIR/sharemnt_${i}_stress entry"
			    cat $STF_TMPDIR/entry.prop.$$
			    cleanup_sharemgr
			fi
			i=$((i + 1))
		done

		# step 6. unset groups:
		i=0
		while (( $i <= $NUM_GROUPS )); do
			sharemgr unset -P nfs -p anon shmgr_grp_$i
				>> $STF_TMPDIR/grp.unset.$$ 2>&1 &
			i=$((i + 1))
		done
		# wait for all unset commands in background to finish
		condition="! pgrep -z $ZONENAME -P $$ -x sharemgr > /dev/null"
		wait_now $STRESS_TIMEOUT "$condition" 3
		if (( $? != 0 )); then
			echo "ERROR: timeout<$STRESS_TIMEOUT secs> for \c"
			echo "unsetting groups' properties..."
			ps -efz $ZONENAME | grep "sharemgr"
			cat $STF_TMPDIR/grp.unset.$$
			pkill -z $ZONENAME -P $$ -x sharemgr
			cleanup_sharemgr
		fi
		# check property of groups
		sharemgr show -p -P nfs | grep "^shmgr_grp_" \
			> $STF_TMPDIR/grp.prop.$$
		grep -w anon $STF_TMPDIR/grp.prop.$$ > /dev/null
		if (( $? == 0 )); then
			echo "ERROR: sharemgr unset group was unsuccessful"
			echo "\tStill find anon property from some groups"
			cat $STF_TMPDIR/grp.prop.$$
			cleanup_sharemgr
		fi
		# check property of entries
		grep $STRESSDIR/sharemnt_ $SHARETAB > $STF_TMPDIR/entry.prop.$$
		grep -w anon $STF_TMPDIR/entry.prop.$$ > /dev/null
		if (( $? == 0 )); then
			echo "ERROR: sharemgr unset group was unsuccessful"
			echo "\tStill find anon property from some entries"
			cat $STF_TMPDIR/entry.prop.$$
			cleanup_sharemgr
		fi

		# step 7. disable groups:
		sharemgr disable -va > $STF_TMPDIR/grp.disable.$$ 2>&1
		if (( $? != 0 )); then
			echo "ERROR: sharemgr disable was unsuccessful"
			cat $STF_TMPDIR/grp.disable.$$
			cleanup_sharemgr
		fi
		# check number of disabled groups
		sharemgr list -v -P nfs | grep "^shmgr_grp_" \
			> $STF_TMPDIR/grp.out.$$
		num=$(grep -w disabled $STF_TMPDIR/grp.out.$$ | wc -l \
			| nawk '{print $1}')
		expected=$((NUM_GROUPS + 1))
		if (( $num != $expected )); then
			echo "ERROR: sharemgr disable was unsuccessful"
			echo "\tExpected to see $expected disabled groups, \c"
			echo "but got $num"
			cat $STF_TMPDIR/grp.out.$$
			cleanup_sharemgr
		fi
		# check there is no test dirs exported
		grep $STRESSDIR/sharemnt_ $SHARETAB > $STF_TMPDIR/share.out.$$
		num=$(wc -l $STF_TMPDIR/share.out.$$ | nawk '{print $1}')
		if (( $num > 0 )); then
			echo "ERROR: sharemgr disable was unsuccessful"
			echo "\tStill find $num shared entries"
			cat $STF_TMPDIR/share.out.$$
			cleanup_sharemgr
		fi

		# step 8. enable groups:
		sharemgr enable -va > $STF_TMPDIR/grp.enable.$$ 2>&1
		if (( $? != 0 )); then
			echo "ERROR: sharemgr enable was unsuccessful"
			cat $STF_TMPDIR/grp.enable.$$
			cleanup_sharemgr
		fi
		# check number of enabled groups
		sharemgr list -v -P nfs | grep "^shmgr_grp_" \
			> $STF_TMPDIR/grp.out.$$
		num=$(grep -w enabled $STF_TMPDIR/grp.out.$$ | wc -l \
			| nawk '{print $1}')
		expected=$((NUM_GROUPS + 1))
		if (( $num != $expected )); then
			echo "ERROR: sharemgr enable was unsuccessful"
			echo "\tExpected to see $expected enabled groups, \c"
			echo "but got $num"
			cat $STF_TMPDIR/grp.out.$$
			cleanup_sharemgr
		fi
		# check number of shares again
		grep $STRESSDIR/sharemnt_ $SHARETAB > $STF_TMPDIR/share.out.$$
		num=$(wc -l $STF_TMPDIR/share.out.$$ | nawk '{print $1}')
		if (( $num != $NUM )); then
			echo "ERROR: sharemgr enable was unsuccessful"
			echo "\tExpected to see $NUM shares, but got $num"
			cat $STF_TMPDIR/share.out.$$
			cleanup_sharemgr
		fi

		# step 9. remove-share:
		i=$NUM
		while (( $i >= 0 )); do
			i=$((i - 1))
			k=$((i % NUM_GROUPS))
			sharemgr remove-share \
				-s $STRESSDIR/sharemnt_${i}_stress shmgr_grp_$k \
				>> $STF_TMPDIR/grp.rmvshr.$$ 2>&1 &
		done
		# wait for all remove-share commands in background to finish
		condition="! pgrep -z $ZONENAME -P $$ -x sharemgr > /dev/null"
		wait_now $STRESS_TIMEOUT "$condition" 3
		if (( $? != 0 )); then
			echo "ERROR: timeout<$STRESS_TIMEOUT secs> for \c"
			echo "removing share pathes from groups..."
			ps -efz $ZONENAME | grep "sharemgr"
			cat $STF_TMPDIR/grp.rmvshr.$$
			pkill -z $ZONENAME -P $$ -x sharemgr
			cleanup_sharemgr
		fi
		# check number of entries in each group
		i=0
		while (( $i < $NUM_GROUPS )); do
			sharemgr show shmgr_grp_$i | grep -v "^shmgr_grp_$i" \
				> $STF_TMPDIR/entry.out.$$
			num=$(wc -l $STF_TMPDIR/entry.out.$$ | \
			    nawk '{print $1}')
			if (( $num > 0 )); then
			    echo "ERROR: sharemgr remove-share was unsuccessful"
			    echo "\tStill find $num entries in shmgr_grp_$i"
			    cat $STF_TMPDIR/entry.out.$$
			    cleanup_sharemgr
			fi
			i=$((i + 1))
		done
		# check there is no test dirs exported any more
		grep $STRESSDIR/sharemnt_ $SHARETAB > $STF_TMPDIR/share.out.$$
		num=$(wc -l $STF_TMPDIR/share.out.$$ | nawk '{print $1}')
		if (( $num > 0 )); then
			echo "ERROR: sharemgr remove-share was unsuccessful"
			echo "\tStill find $num shared entries"
			cat $STF_TMPDIR/share.out.$$
			cleanup_sharemgr
		fi

		# step 10. delete groups:
		i=$NUM_GROUPS
		while (( $i >= 0 )); do
			sharemgr delete shmgr_grp_$i \
				>> $STF_TMPDIR/grp.delete.$$ 2>&1 &
			i=$((i - 1))
		done
		# wait for all delete commands in background to finish
		condition="! pgrep -z $ZONENAME -P $$ -x sharemgr > /dev/null"
		wait_now $STRESS_TIMEOUT "$condition" 3
		if (( $? != 0 )); then
			echo "ERROR: timeout<$STRESS_TIMEOUT secs> for \c"
			echo "deleting share groups..."
			ps -efz $ZONENAME | grep "sharemgr"
			cat $STF_TMPDIR/grp.delete.$$
			pkill -z $ZONENAME -P $$ -x sharemgr
			cleanup_sharemgr
		fi
		# check number of groups
		sharemgr list -P nfs | grep "^shmgr_grp_" \
			> $STF_TMPDIR/grp.out.$$
		num=$(wc -l $STF_TMPDIR/grp.out.$$ | nawk '{print $1}')
		if (( $num > 0 )); then
			echo "ERROR: sharemgr delete group was unsuccessful"
			echo "\tStill find $num groups remained"
			cat $STF_TMPDIR/grp.out.$$
			cleanup_sharemgr
		fi

		echo "$NAME: sharemgr testing complete - Result PASS"
		cleanup 0
		;;
	cleanup_001)
		[[ -f /etc/dfs/dfstab.sharemnt.stress ]] && \
			mv /etc/dfs/dfstab.sharemnt.stress /etc/dfs/dfstab
		unshareall > $STF_TMPDIR/unshare.out.$$ 2>&1
		if (( $? != 0 )); then
			echo "ERROR: failed to unshareall"
			cat $STF_TMPDIR/unshare.out.$$
			cleanup 1 t
		fi
		svcadm restart nfs/server
		sleep 10

		cleanup 0 t
		;;
	cleanup_002)
		zfs set sharenfs=off $ZFSBASE > $STF_TMPDIR/time.out.$$ 2>&1
		if (( $? != 0 )); then
			echo "ERROR: setting sharenfs=off failed."
			cat $STF_TMPDIR/time.out.$$
			cleanup 1 t
		fi

		[[ -f /etc/dfs/dfstab.sharemnt.stress ]] && \
			mv /etc/dfs/dfstab.sharemnt.stress /etc/dfs/dfstab
		svcadm restart nfs/server
		sleep 10

		cleanup 0 t
		;;
	\?)
		;;
	esac
	;;
h)
	typeset opt=$OPTARG
	shift $((OPTIND - 1))
	typeset path=$1
	$MISCSHARE default share $path $opt
	if (( $? != 0 )); then
		echo "ERROR: failed to share $path"
		cleanup 1
	fi
	condition="grep -w $path $SHARETAB | grep -w $opt"
	wait_now 900 "$condition" 20
	if (( $? != 0 )); then
		echo "ERROR: failed to find $path shared with $opt"
		cleanup 1
	fi
	$MISCSHARE default unshare $path
	if (( $? != 0 )); then
		echo "ERROR: failed to unshare $path"
		cleanup 1
	fi
	grep -w $path $SHARETAB > /dev/null 2>&1
	if (( $? == 0 )); then
		echo "ERROR: $path is still in $SHARETAB"
		cleanup 1
	fi
	echo "Done with $path"
	;;
c)
	if [[ -n $ZFSPOOL ]]; then
		i=0
		while (( $i <= $NUM_SHARES )); do
			Zfs=`zfs list | grep "$STRESSDIR/sharemnt_${i}_stress" \
				| nawk '{print $1}'`
			if [[ -n $Zfs ]]; then
				zfs destroy -f $Zfs \
				    > $STF_TMPDIR/cleanFS.out.$$ 2>&1
				if (( $? != 0 )); then
				    echo "WARNING, unable to cleanup [$Zfs];"
				    cat $STF_TMPDIR/cleanFS.out.$$
				    echo "\t Please clean it up manually."
				    cleanup 1
				fi
			fi
			i=$((i+1))
		done
	fi
	rm -rf $STRESSDIR

	echo "Done - cleanup OKAY"
	;;
\?)
	echo $Usage
	exit 2
	;;
esac

cleanup 0
