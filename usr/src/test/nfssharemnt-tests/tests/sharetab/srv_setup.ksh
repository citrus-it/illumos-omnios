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

# Setup the SERVER for testing sharetab.

NAME=$(basename $0)

Usage="Usage: $NAME -s | -c | -i | -a | -m | -r phase\n
		-s: setup this host for sharetab test\n
		-c: cleanup\n
		-i: initial check\n
		-a: access test\n
		-m: mountd test\n
		-r: reboot test\n
			phase: 1. prepare the file; 2. compare the files\n
"
#		-z: zfs test\n
#		-u: zfs unshare test\n
if (( $# < 1 )); then
	echo $Usage
	exit 99
fi

# variables gotten from client system:
STF_TMPDIR=STF_TMPDIR_from_client
SHAREMNT_DEBUG=${SHAREMNT_DEBUG:-"SHAREMNT_DEBUG_from_client"}

. $STF_TMPDIR/srv_config.vars

# Include common STC utility functions for SMF
if [[ -s $STC_GENUTILS/include/libsmf.shlib ]]; then
	. $STC_GENUTILS/include/libsmf.shlib
	. $STC_GENUTILS/include/nfs-smf.kshlib
else
	. $STF_TMPDIR/libsmf.shlib
	. $STF_TMPDIR/nfs-smf.kshlib
fi
. $STF_TMPDIR/sharemnt.kshlib

# Turn on debug info, if requested
export STC_GENUTILS_DEBUG=$SHAREMNT_DEBUG
[[ :$SHAREMNT_DEBUG: == *:$NAME:* \
	|| :$SHAREMNT_DEBUG: == *:all:* ]] && set -x

# share is not supported in non-global zone
ck_zone "This test is not supported since share cannot run in non-global zone."

typeset var1 var2

# cleanup function on all exit
function cleanup {
	[[ :$SHAREMNT_DEBUG: == *:$NAME:* \
		|| :$SHAREMNT_DEBUG: == *:all:* ]] && set -x

	#zfs unallow bin share,sharenfs $SHARETABDIR >/dev/null 2>&1
	rm -fr $STF_TMPDIR/$NAME.*.$$
	exit $1
}

# Timeout (in seconds) for SMF commands to complete
SMF_TIMEOUT=60

getopts sciamr: opt
case $opt in
s)
	if [[ ! -d $SHARETABDIR ]]; then
		mkdir -pm 0777 $SHARETABDIR
		[[ $? != 0 ]] && \
			echo "could not create $SHARETABDIR" && exit 1
	fi

	rm -rf $SHARETABDIR/dir?
	mkdir -m 0777 $SHARETABDIR/dir1 > /dev/null 2>&1
	mkdir -m 0777 $SHARETABDIR/dir2 > /dev/null 2>&1
	mkdir -m 0777 $SHARETABDIR/dir3 > /dev/null 2>&1
	[[ ! -w $SHARETABDIR/dir1 || ! -w $SHARETABDIR/dir2 || ! -w $SHARETABDIR/dir3 ]] && \
		echo "can not create dir1/dir2/dir3 under $SHARETABDIR" && \
		cleanup 1

	# set up ZFS
	if [[ -n $ZFSPOOL ]]; then
	    create_zfs_fs $ZFSBASE $SHARETABDIR/dir1 > $STF_TMPDIR/$NAME.zfs.$$ 2>&1
	    if [[ $? != 0 ]]; then
	        echo "$NAME: failed to create_zfs_fs $SHARETABDIR/dir1"
	        cat $STF_TMPDIR/$NAME.zfs.$$
	        cleanup 99
	    fi
		print_debug $STF_TMPDIR/$NAME.zfs.$$

	    create_zfs_fs $ZFSBASE $SHARETABDIR/dir2 > $STF_TMPDIR/$NAME.zfs.$$ 2>&1
	    if [[ $? != 0 ]]; then
	        echo "$NAME: failed to create_zfs_fs $SHARETABDIR/dir2"
	        cat $STF_TMPDIR/$NAME.zfs.$$
	        cleanup 99
	    fi
		print_debug $STF_TMPDIR/$NAME.zfs.$$
	fi
	echo "$SHARETABDIR/dir1 share -F nfs -o rw $SHARETABDIR/dir1" \
		> $STF_TMPDIR/$NAME.cn.$$
	echo "$SHARETABDIR/dir2 share -F nfs -o rw $SHARETABDIR/dir2" \
		>> $STF_TMPDIR/$NAME.cn.$$
	nfs_smf_setup "file" $STF_TMPDIR/$NAME.cn.$$ $SMF_TIMEOUT \
		> $STF_TMPDIR/$NAME.shr.$$ 2>&1
	if [[ $? != 0 ]]; then
		echo "\n$NAME: failed to share path"
		cat $STF_TMPDIR/$NAME.shr.$$
		cleanup 1
	fi
	print_debug $STF_TMPDIR/$NAME.shr.$$
	sleep 5

	echo "Done - sharetab setup PASS"
	;;
c)
	echo "$SHARETABDIR/dir1"  > $STF_TMPDIR/$NAME.cn.$$
	echo "$SHARETABDIR/dir2"  >> $STF_TMPDIR/$NAME.cn.$$
	nfs_smf_clean $STF_TMPDIR/$NAME.cn.$$ $SMF_TIMEOUT
	unshare $SHARETABDIR/dir3 > /dev/null 2>&1
	sleep 5

	if [[ -n $ZFSPOOL ]]; then
		typeset Zfs=""
		for Zfs in $(zfs list | grep "$SHARETABDIR" | awk '{print $1}'); do
			zfs destroy -f $Zfs > $STF_TMPDIR/$NAME.cleanFS.$$ 2>&1
			if (( $? != 0 )); then
				echo "WARNING: unable to cleanup [$Zfs];"
				cat $STF_TMPDIR/$NAME.cleanFS.$$
				echo "\t Please clean it up manually."
				cleanup 2
			fi
		done
	fi

	rm -rf $STF_TMPDIR/sharemnt.shtab $SHARETABDIR
	echo "Done - sharetab cleanup PASS"
	;;
i)
	# fs check
	rval=$(df -F sharefs 2> $STF_TMPDIR/$NAME.df.$$)
	if [[ $? != 0 ]]; then
		echo "\n$NAME: failed to get the status of sharefs"
		cat $STF_TMPDIR/$NAME.df.$$
		cleanup 2
	fi

	# mount point check
	rval=$(echo $rval | awk '{print $1}')
	if [[ $rval != $SHARETAB ]]; then
	    echo "\n$NAME: the mount point <$rval> is not expected <$SHARETAB>"
	    cleanup 2
	fi

	# SHARETAB check
	[[ ! -f $SHARETAB ]] && \
		echo "\n$NAME: test failed for $SHARETAB does not exist" && \
		cleanup 2

	rm -f $SHARETAB
	[[ ! -f $SHARETAB ]] && \
		echo "\n$NAME: test failed for $SHARETAB can be removed." && \
		cleanup 2

	# umount check
	# non-empty SHARETAB can not be umounted
	umount $SHARETAB > $STF_TMPDIR/$NAME.mnt.$$ 2>&1
	rval=$(df -F sharefs | awk '{print $1}')
	if [[ $rval != $SHARETAB ]]; then
		echo "\n$NAME: test failed for <$SHARETAB> is umounted."
		cat $STF_TMPDIR/$NAME.mnt.$$
		cleanup 2
	fi
	unshareall > $STF_TMPDIR/$NAME.unshare.$$ 2>&1
	if [[ $? != 0 ]]; then
		echo "\n$NAME: failed to unshareall"
		cat $STF_TMPDIR/$NAME.unshare.$$
		cleanup 2
	fi
	if [[ -s $SHARETAB ]]; then
		echo "\n$NAME: test failed for $SHARETAB is not empty."
		echo "\tafter unsharell:"
		cat $SHARETAB
		cleanup 2
	fi
	# empty SHARETAB can be umounted
	umount $SHARETAB > $STF_TMPDIR/$NAME.mnt.$$ 2>&1
	rval=$(df -F sharefs)
	if [[ -n $rval ]]; then
		echo "\n$NAME: test failed for <$SHARETAB> is still mounted."
		echo "\t$rval"
		cat $STF_TMPDIR/$NAME.mnt.$$
		cleanup 2
	fi
	sharemgr add-share -s $SHARETABDIR/dir3 default > $STF_TMPDIR/$NAME.shr.$$ 2>&1
	if [[ $? != 0 ]]; then
		echo "\n$NAME: failed to add-share $SHARETABDIR/dir3."
		cat $STF_TMPDIR/$NAME.shr.$$
		cleanup 2
	fi
	rval=$(share)
	if [[ -n $rval ]]; then
		echo "\n$NAME: test failed for non-empty output of share."
		echo "\t$rval"
		cleanup 2
	fi
	sharemgr show default | tr -d ' 	' | \
		grep -ws $SHARETABDIR/dir3 > /dev/null
	if [[ $? != 0 ]]; then
		echo "\n$NAME: sharemgr failed to find $SHARETABDIR/dir3."
		cleanup 2
	fi
	# get it back
	mount -F sharefs sharefs $SHARETAB > $STF_TMPDIR/$NAME.mnt.$$ 2>&1
	if [[ $? != 0 ]]; then
		echo "\n$NAME: failed to mount $SHARETAB back."
		cat $STF_TMPDIR/$NAME.mnt.$$
		cleanup 2
	fi
	grep -ws $SHARETABDIR/dir3 $SHARETAB > /dev/null
	if [[ $? != 0 ]]; then
		echo "\n$NAME: failed to find $SHARETABDIR/dir3 in $SHARETAB."
		cleanup 2
	fi
	shareall > $STF_TMPDIR/$NAME.shr.$$ 2>&1
	if [[ $? != 0 ]]; then
		echo "\n$NAME: failed to shareall"
		cat $STF_TMPDIR/$NAME.shr.$$
		cleanup 2
	fi
	for i in 1 2 3; do
		grep -ws $SHARETABDIR/dir$i $SHARETAB
		if [[ $? != 0 ]]; then
			echo "\n$NAME: failed to find $SHARETABDIR/dir$i in $SHARETAB."
			cleanup 2
		fi
	done
	sharemgr remove-share -s $SHARETABDIR/dir3 default \
	    > $STF_TMPDIR/$NAME.shr.$$ 2>&1
	if [[ $? != 0 ]]; then
		echo "\n$NAME: failed to remove-share $SHARETABDIR/dir3."
		cat $STF_TMPDIR/$NAME.shr.$$
		cleanup 2
	fi

	# readonly check
	var1=$(/usr/bin/sum $SHARETAB)
	echo readonly >> $SHARETAB 2>/dev/null
	var2=$(/usr/bin/sum $SHARETAB)
	if [[ $var1 != $var2 || $? != 0 ]]; then
		echo "\n$NAME: test failed for $SHARETAB is changed"
		cat $SHARETAB
		cleanup 1
	fi

	# chmod check
	chmod -f 777 $SHARETAB
	var1=$(ls -l $SHARETAB | awk '{print $1}')
	if [[ $var1 != "-r--r--r--" || $? != 0 ]]; then
		echo "\n$NAME: test failed for the permissions mode"
		echo "\t of $SHARETAB is wrong"
		ls -l $SHARETAB
		cleanup 1
	fi

	# acl check
	var1=$(ls -v $SHARETAB | /usr/bin/sum)
	if [[ -n $ZFSBASE ]]; then
		chmod A+user:root:write_data:allow $SHARETAB >/dev/null 2>&1
	else
		chmod A+user:root:rw- $SHARETAB >/dev/null 2>&1
	fi
	rc=$?
	var2=$(ls -v $SHARETAB | /usr/bin/sum)
	if [[ $var1 != $var2 || $rc == 0 ]]; then
		echo "\n$NAME: test failed for the ACL mode of"
		echo "\t $SHARETAB is changed"
		ls -v $SHARETAB
		cleanup 1
	fi

	echo "Done - sharetab initial check PASS"
	;;
a)
	# Access /etc/dfs/sharetab as end user.
	echo "Access $SHARETAB as end user ... \c"
	var1=$(/usr/bin/sum $SHARETAB)
	var2=$(su bin -c "/usr/bin/sum $SHARETAB")
	if [[ $var1 != $var2 || $? != 0 ]]; then
		echo "\n$NAME: the user of bin got a different $SHARETAB -"
		echo "\t $var2, root got $var1"
		cat $SHARETAB
		cleanup 1
	fi

	echo "OK"

	echo "Done - sharetab access test PASS"
	;;
m)
	# Change the state of mountd, verify the consistence of
	# /etc/dfs/sharetab.
	echo "Disable mountd and check $SHARETAB ... \c"
	/usr/sbin/svcadm refresh $SRV_FMRI
	sleep 5
	typeset pid1 pid2
	pid1=$(pgrep -z global -x mountd)
	if [[ $? != 0 || -z $pid1 ]]; then
		echo "\n$NAME: failed to get mountd"
		cleanup 1
	fi
	var1=$(/usr/bin/sum $SHARETAB)
	pstop $pid1
	if [[ $? != 0 ]]; then
		echo "\n$NAME: failed to stop mountd"
		cleanup 1
	fi
	var2=$(/usr/bin/sum $SHARETAB)
	if [[ $var1 != $var2 ]]; then
		echo "\n$NAME: test failed for $SHARETAB changed"
		echo "\tafter mountd is stopped, $var1 to $var2"
		cat $SHARETAB
		cleanup 1
	fi
	echo "OK"

	echo "Resume mountd and check $SHARETAB ... \c"
	prun $pid1
	if [[ $? != 0 ]]; then
		echo "\n$NAME: failed to resume mountd"
		cleanup 1
	fi
	var2=$(/usr/bin/sum $SHARETAB)
	if [[ $var1 != $var2 ]]; then
		echo "\n$NAME: test failed for $SHARETAB change"
		echo "\tafter mountd is resumed, $var1 to $var2"
		cat $SHARETAB
		cleanup 1
	fi
	echo "OK"

	echo "Restart mountd and check $SHARETAB ... \c"
	share $SHARETABDIR/dir3 > $STF_TMPDIR/$NAME.shr.$$ 2>&1
	if [[ $? != 0 ]]; then
		echo "\n$NAME: failed to share $SHARETABDIR/dir3"
		cat $STF_TMPDIR/$NAME.shr.$$
		cleanup 1
	fi
	sleep 5
	var2=$(/usr/bin/sum $SHARETAB)
	if [[ $var1 == $var2 ]]; then
		echo "\n$NAME: test failed for $SHARETAB is not updated"
		echo "\tafter $SHARETABDIR/dir3 is shared"
		cat $SHARETAB
		cleanup 1
	fi
	kill -9 $pid1

	sleep 10 # wait for mountd
	pid2=$(pgrep -z global -x mountd)
	if [[ $? != 0 || -z $pid2 || $pid1 == $pid2 ]]; then
		echo "\n$NAME: failed to restart mountd"
		cleanup 1
	fi
	# Note: no in-kernel sharetab, after killing mountd, manual share lost
	var2=$(/usr/bin/sum $SHARETAB)
	if [[ $var1 != $var2 ]]; then
		echo "\n$NAME: $SHARETAB changed after mountd is restarted"
		echo "\t $var1 to $var2"
		cat $SHARETAB
		cleanup 1
	fi
	echo "OK"

	echo "Done - sharetab mountd test PASS"
	;;
r)
	case $OPTARG in
	1)
		# Test SHARETAB is consistent after the machine reboots
		# prepare for reboot
		echo "backup $SHARETAB and ready to reboot... \c"
		cp -f $SHARETAB $STF_TMPDIR/_SHARETAB \
		    > $STF_TMPDIR/$NAME.shr.$$ 2>&1
		if [[ $? != 0 ]]; then
			echo "\n$NAME: failed to backup $SHARETAB"
			cat $STF_TMPDIR/$NAME.shr.$$
			rm -f $STF_TMPDIR/_SHARETAB
			cleanup 1
		fi
		echo "OK"

		echo "Done - $SHARETAB prepare PASS for reboot"
		reboot
		;;
	2)
		# check the consistence of /etc/dfs/sharetab
		echo "reboot and check $SHARETAB ... \c"
		diff $SHARETAB $STF_TMPDIR/_SHARETAB
		if [[ $? != 0 ]]; then
			echo "\n$NAME: $SHARETAB is inconsistent after reboot"
			echo "--- before reboot ---"
			cat $STF_TMPDIR/_SHARETAB
			echo "--- after reboot ---"
			cat $SHARETAB
			rm -f $STF_TMPDIR/_SHARETAB
			cleanup 1
		fi
		echo "OK"
		rm -f $STF_TMPDIR/_SHARETAB

		echo "Done - $SHARETAB test PASS after reboot"
		;;
	\?)
		cleanup 2
		;;
	esac

	;;
z)
	# zfs delegation has dependency on sharetab, ;-)
	# This section will not run until zfs delegation putback
	# and maybe some codes need update
	# share zfs as end user
	echo "zfs share test ... \c"
	zfs allow bin share,sharenfs $SHARETABDIR
	typeset Zfs=""
	Zfs=$(zfs list | grep "$SHARETABDIR/dir" | awk '{print $1}')
	[[ -z $Zfs ]] && echo "\n$NAME: Can not test zfs share" && cleanup 1

	su bin -c "zfs share $SHARETABDIR/dir1"
	su bin -c "zfs share $SHARETABDIR/dir2"
	grep -w "$SHARETABDIR/dir1" $SHARETAB > /dev/null 2>&1
	[[ $? != 0 ]] && \
		echo "\n$NAME: failed to share $SHARETABDIR/dir1" && cleanup 1
	grep -w "$SHARETABDIR/dir2" $SHARETAB > /dev/null 2>&1
	[[ $? != 0 ]] && \
		echo "\n$NAME: failed to share $SHARETABDIR/dir2" && cleanup 1

	echo "OK"
	echo "Done - sharetab zfs share PASS"
	;;
u)
	# zfs delegation has dependency on sharetab, ;-)
	# This section will not run until zfs delegation putback
	# and maybe some codes need update
	# unshare zfs as end user
	echo "zfs unshare test ... \c"
	su bin -c "zfs unshare $SHARETABDIR/dir1"
	su bin -c "zfs unshare $SHARETABDIR/dir2"
	grep -w "$SHARETABDIR/dir" $SHARETAB > /dev/null 2>&1
	[[ $? != 0 ]] && \
		echo "\n$NAME: failed to unshare $SHARETABDIR/dir" && cleanup 1

	echo "OK"
	echo "Done - sharetab zfs unshare PASS"
	;;
\?)
	echo $Usage
	exit 2
	;;

esac

cleanup 0
