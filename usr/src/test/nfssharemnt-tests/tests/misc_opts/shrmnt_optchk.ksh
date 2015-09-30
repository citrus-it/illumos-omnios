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
# ID: shrmnt_optchk
#
# DESCRIPTION:
#   For the mount and export the SERVER's filesystems with
#   other options, including share: public, anon, nosuid;
#   mount: public, rsize, wsize, quota|noquota, intr|nointr
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

# Function to do interrupt test when mounting fs
#   Usage: do_intr_test mnt_opt
#
function do_intr_test {
	typeset Fname=do_intr_test
	[[ :$SHAREMNT_DEBUG: == *:${NAME}:* \
		|| :$SHAREMNT_DEBUG: == *:$Fname:* \
		|| :${SHAREMNT_DEBUG}: == *:all:* ]] && set -x
	set -o monitor
	typeset mntopt="$*"

	echo "Doing intr access testing at $MNTDIR ... \c"

	# disable the nfs/server
	typeset fmri="svc:/network/nfs/server:default"
	typeset SRVDEBUG=$STC_GENUTILS_DEBUG:$SHAREMNT_DEBUG
	[[ :$SRVDEBUG: == *:RSH:* ]] && SRVDEBUG=all
	RSH root $SERVER \
		"export SHAREMNT_DEBUG=$SRVDEBUG; \
		$SRV_TMPDIR/srv_setup -f disable $fmri" \
		> $STF_TMPDIR/rsh.out.$$ 2>&1
	rc=$?
	[[ :$SRVDEBUG: == *:all:* ]] && cat $STF_TMPDIR/rsh.out.$$
	grep "Done" $STF_TMPDIR/rsh.out.$$ > /dev/null 2>&1
	if [[ $? != 0 || $rc != 0 ]]; then
		echo "\n$Fname: Failed to disable nfs/server running on $SERVER"
		cat $STF_TMPDIR/rsh.out.$$
		cleanup $STF_FAIL
	fi

	# try to kill the hung cat process with INT signal
	cat $MNTDIR/rofile > /dev/null 2>&1 &
	typeset -i app_pid=$!
	/usr/bin/kill -INT $app_pid > /dev/null 2>&1
	pgrep cat | grep -w $app_pid > /dev/null 2>&1
	typeset -i res=$?
	if (( $res == 0 )); then
		kill -9 $app_pid > /dev/null 2>&1
		wait_now 120 "! pgrep cat | grep -w $app_pid > /dev/null 2>&1" 3
	fi

	# enable the nfs/server
	RSH root $SERVER \
		"export SHAREMNT_DEBUG=$SRVDEBUG; \
		$SRV_TMPDIR/srv_setup -f enable $fmri" \
		> $STF_TMPDIR/rsh.out.$$ 2>&1
	rc=$?
	[[ :$SRVDEBUG: == *:all:* ]] && cat $STF_TMPDIR/rsh.out.$$
	grep "Done" $STF_TMPDIR/rsh.out.$$ > /dev/null 2>&1
	if [[ $? != 0 || $rc != 0 ]]; then
		echo "\n$Fname: Failed to enable nfs/server running on $SERVER"
		cat $STF_TMPDIR/rsh.out.$$
		cleanup $STF_FAIL
	fi

	# check whether the hung process was/wasn't killed as expected
	typeset -i exp_res=1
	[[ $mntopt == *nointr* ]] && exp_res=0
	if (( $res != $exp_res )); then
		echo "\n$Fname: cat is not as expected, \c"
		(( $res == 0 )) && echo "it is not interruptted" \
			|| echo "it is interruptted"
		cleanup $STF_FAIL
	fi

	echo "OK"
}


# Function to do set uid/gid test
#   Usage: do_sid_test shr_opt
#
function do_sid_test {
	typeset Fname=do_sid_test
	[[ :$SHAREMNT_DEBUG: == *:${NAME}:* \
		|| :$SHAREMNT_DEBUG: == *:$Fname:* \
		|| :${SHAREMNT_DEBUG}: == *:all:* ]] && set -x
	typeset shropt="$*"
	typeset is_sid="set"
	typeset exp_sid="set"
	[[ $shropt == *nosuid* ]] && exp_sid="not set"
	typeset tfile="$MNTDIR/$Fname.out.$$"

	echo "Doing suid testing at $MNTDIR ... \c"

	# create a file and set access permission
	rm -f $tfile
	touch $tfile
	chmod 777 $tfile
	typeset perm=$(ls -l $tfile | awk '{print $1}')
	if [[ $perm != "-rwxrwxrwx" ]]; then
		echo "\n$Fname: Failed to create $tfile with \c"
		echo "the permission of rwx"
		ls -l $tfile
		rm -f $tfile
		cleanup $STF_FAIL
	fi

	# try to enable the file's setuid mode bits and verify
	chmod u+s $tfile
	[[ ! -u $tfile ]] && is_sid="not set"
	if [[ $is_sid != $exp_sid ]]; then
		echo "\n$Fname: the setuid bit is not expected, it is $is_sid"
		ls -l $tfile
		rm -f $tfile
		cleanup $STF_FAIL
	fi

	# try to enable the file's setgid mode bits and verify
	chmod g+s $tfile
	[[ ! -g $tfile ]] && is_sid="not set"
	if [[ $is_sid != $exp_sid ]]; then
		echo "\n$Fname: the setgid bit is not expected, it is $is_sid"
		ls -l $tfile
		rm -f $tfile
		cleanup $STF_FAIL
	fi

	echo "OK"
	rm -f $tfile
}

# Function to check read/write buffer size
#   Usage: do_rwsize_test mnt_dir mnt_opt
#
function do_rwsize_test {
	typeset Fname=do_rwsize_test
	[[ :$SHAREMNT_DEBUG: == *:${NAME}:* \
		|| :$SHAREMNT_DEBUG: == *:$Fname:* \
		|| :${SHAREMNT_DEBUG}: == *:all:* ]] && set -x
	typeset mntdir=$1
	typeset mntopt=$2

	echo "Doing rsize/wsize check at $mntdir ... \c"
	nfsstat -m $mntdir > $STF_TMPDIR/nstat.out.$$ 2>&1
	if (( $? != 0 )); then
		echo "\n$Fname: nfsstat $mntdir failed"
		cat $STF_TMPDIR/nstat.out.$$
		cleanup $STF_UNRESOLVED
	fi

	typeset -i rsize wsize default_size
	typeset MNT_STAT=$(grep "^ Flags:" $STF_TMPDIR/nstat.out.$$ \
		| awk '{print $2}')

	# set initial expected size
	for opt in $(echo $mntopt | sed 's/,/ /g'); do
		case $opt in
		rsize=*|wsize=*)
			eval $opt
			;;
		esac
	done

	# get the default size for different nfs version
	case $MNT_STAT in
	vers=2*)
		default_size=8192 #8K
		;;
	vers=3*)
		default_size=32768 #32K
		;;
	*)
		default_size=1048576 #1M
		;;
	esac

	# set real expected size
	(( rsize == 0 || rsize > default_size )) && rsize=default_size
	(( wsize == 0 || wsize > default_size )) && wsize=default_size
	typeset expt="rsize=$rsize,wsize=$wsize"

	if [[ ,$MNT_STAT, != *,$expt,* ]]; then
		echo "\n$Fname: didn't get correct rsize/wsize \c"
		echo "with mntopt<$mntopt>, expected: $expt"
		cat $STF_TMPDIR/nstat.out.$$
		cleanup $STF_FAIL
	fi

	echo "OK"
}

# Function to check fs quota
#   Usage: do_quota_test mnt_opt
#
function do_quota_test {
	typeset Fname=do_quota_test
	[[ :$SHAREMNT_DEBUG: == *:${NAME}:* \
		|| :$SHAREMNT_DEBUG: == *:$Fname:* \
		|| :${SHAREMNT_DEBUG}: == *:all:* ]] && set -x
	typeset mntopt=$1

	echo "Doing quota check ... \c"

	# try to make a file with greater size than quota allowed and
	# expect to see quota exceed if ZFS
	if [[ -n $ZFSPOOL ]]; then
		/usr/sbin/mkfile 3m $MNTDIR/file 2>&1 | \
		    grep "Disc quota exceeded" > /dev/null 2>&1
		if (( $? != 0 )); then
			echo "\n$Fname: disk quota is not in effect"
			rm -f $MNTDIR/file*
			cleanup $STF_FAIL
		fi
		echo "OK"
		rm -f $MNTDIR/file*
		return
	fi

	# try to make a file with greater size than quota allowed with
	# $TUSER01 and expect to see quota exceed
	su $TUSER01 -c "/usr/sbin/mkfile 11k $MNTDIR/file" 2>&1 | \
		grep "Disc quota exceeded" > /dev/null 2>&1
	if (( $? != 0 )); then
		echo "\n$Fname: disk quota is not in effect"
		rm -f $MNTDIR/file*
		cleanup $STF_FAIL
	fi
	rm -f $MNTDIR/file*

	# try to make more files than limited number with $TUSER01 and
	# expect to see quota exceed too
	su $TUSER01 -c "/usr/sbin/mkfile 1k $MNTDIR/file1 $MNTDIR/file2 \
		$MNTDIR/file3 $MNTDIR/file4" 2>&1
	if (( $? != 0 )); then
		echo "\n$Fname: expect to create 4 files, but failed"
		quota -v $TUSER01
		rm -f $MNTDIR/file*
		cleanup $STF_FAIL
	fi
	su $TUSER01 -c "/usr/sbin/mkfile 1k $MNTDIR/file5" 2>&1 | \
		grep "Disc quota exceeded" > /dev/null 2>&1
	if (( $? != 0 )); then
		echo "\n$Fname: file quota is not in effect"
		rm -f $MNTDIR/file*
		cleanup $STF_FAIL
	fi

	# check quota of $TUSER01
	quota -v $TUSER01 > $STF_TMPDIR/quota.out.$$ 2>&1
	if (( $? != 0 )); then
		echo "\n$Fname: quota -v $TUSER01 failed"
		cat $STF_TMPDIR/quota.out.$$
		rm -f $MNTDIR/file*
		cleanup $STF_FAIL
	fi

	typeset out=$(grep "$MNTDIR" $STF_TMPDIR/quota.out.$$)
	if [[ ,$mntopt, != *,noquota,* ]]; then
		if [[ $out != $MNTDIR ]]; then
			echo "\n$Fname: quota failed"
			cat $STF_TMPDIR/quota.out.$$
			rm -f $MNTDIR/file*
			cleanup $STF_FAIL
		fi
		out=$(tail -1 $STF_TMPDIR/quota.out.$$ | awk '{print $3,$6}')
		if [[ $out != "10 5" ]]; then
			echo "\n$Fname: didn't get correct quota"
			cat $STF_TMPDIR/quota.out.$$
			rm -f $MNTDIR/file*
			cleanup $STF_FAIL
		fi
	fi

	echo "OK"
	rm -f $MNTDIR/file*
}

function check_for_quotadir {
	typeset Fname=check_for_quotadir
	[[ :$SHAREMNT_DEBUG: == *:${NAME}:* \
		|| :$SHAREMNT_DEBUG: == *:$Fname:* \
		|| :${SHAREMNT_DEBUG}: == *:all:* ]] && set -x

	typeset quota_dir=$1
	if [[ -n $ZFSPOOL ]]; then
		#
		# If quoda_dir is based on UFS, and server is rebooted,
		# setup for quota_dir via LOFI will be lost;
		# If quota_dir is based on ZFS, even if server is rebooted, 
		# setup for quota_dir via zfs is always there.
		#
		return 0
	fi

	RSH root $SERVER \
		"/bin/df -n /dev/lofi/* | grep ${quota_dir}:" \
		> /dev/null 2>&1
	if (( $? != 0 )); then
		echo "$NAME: current test is based on UFS,"
		echo "setup for quota test at<$quota_dir> not found"
		return 1
	fi

	return 0
}

typeset Tname=$1
typeset opt1=$2
typeset opt2=$3

case $Tname in
MNT_INTR*)
	check_for_cipso "$SHRDIR" "$MNTDIR" "$opt1" || return $STF_UNSUPPORTED
	share_check "rw"
	domount_check $opt1
	do_intr_test $opt1
	;;
SH_SUID*)
	check_for_cipso "$SHRDIR" "$MNTDIR" "$opt2" || return $STF_UNSUPPORTED
	[[ $opt1 == "nosuid" ]] && share_check $opt1 || share_check "rw"
	domount_check $opt2
	automount_check $opt2
	do_sid_test $opt1
	;;
MNT_SIZE*)
	check_for_cipso "$SHRDIR" "$MNTDIR" "$opt1" || return $STF_UNSUPPORTED
	share_check "rw"
	domount_check $opt1
	automount_check $opt1
	do_rwsize_test $MNTDIR $opt1
	;;
MNT_QUOTA*)
	check_for_cipso "$QUOTADIR" "$MNTDIR" "$opt1" || \
	    return $STF_UNSUPPORTED
	check_for_quotadir "$QUOTADIR" || return $STF_UNTESTED
	share_check "rw" $QUOTADIR
	domount_check "$opt1" "rw" $QUOTADIR
	automount_check "$opt1" "rw" "QUOTA" $QUOTADIR
	do_quota_test "$opt1"
	;;
*)
	echo "$Tname: error - it is unreachable - Result FAIL"
	exit $STF_UNRESOLVED
	;;
esac

unmount_check

echo "$NAME: testing complete - Result PASS"
cleanup $STF_PASS
