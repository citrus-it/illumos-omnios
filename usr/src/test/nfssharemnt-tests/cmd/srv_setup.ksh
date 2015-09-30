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

# Setup the SERVER for testing remote locking.

NAME=$(basename $0)

Usage="Usage: $NAME -s | -c | -r | -f | -u \n
		-s: to setup this host with mountd/nfsd\n
		-c: to cleanup the server\n
		-r: to reshare SHRDIR with specified options\n
		-f: to do some FMRI operations on server\n
		-u: find the shared file and unshare it\n
"
if (( $# < 1 )); then
	echo $Usage
	exit 99
fi

# variables gotten from client system:
STF_TMPDIR=STF_TMPDIR_from_client
SHAREMNT_DEBUG=${SHAREMNT_DEBUG:-"SHAREMNT_DEBUG_from_client"}

. $STF_TMPDIR/srv_config.vars

SHROPTS=$STF_TMPDIR/ShrOpts.sharemnt
TESTGRPSTAT=$STF_TMPDIR/FtgStat.sharemnt

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

# cleanup function on all exit
function cleanup {
	[[ :$SHAREMNT_DEBUG: == *:$NAME:* \
		|| :$SHAREMNT_DEBUG: == *:all:* ]] && set -x

	rm -fr $STF_TMPDIR/*.$$
	exit $1
}

getopts scr:f:u: opt
case $opt in
s)
	# get fs type
	strfs=$(get_fstype $TESTDIR)
	if (( $? != 0 )); then
		echo "$NAME: get_fstype<$TESTDIR> failed"
		echo $strfs
		cleanup 1
	fi
	fs_type=$(echo $strfs | awk '{print $2}')
	if [[ $fs_type == ufs ]]; then
		ZFSPOOL=""	
	elif [[ $fs_type == zfs ]]; then
		ZFSPOOL=$(echo $strfs | awk '{print $3}')
		ZFSPOOL_STAT=$(echo $strfs | awk '{print $4}')
		if [[ $ZFSPOOL_STAT != "ONLINE" ]]; then
			echo "zpool<$ZFSPOOL> is not online"
			cleanup 1
		fi

		echo "export ZFSPOOL=$ZFSPOOL" >> $STF_TMPDIR/srv_config.vars
	else
		cleanup 2	
	fi
	# print for client's need
	echo "SRV_FS=$fs_type $ZFSPOOL"

	# create test user
	useradd -u $TUID01 -c $TUSER_UTAG -d /tmp $TUSER01 \
		> $STF_TMPDIR/useradd.out.$$ 2>&1
	if (( $? != 0 )); then
		echo "could not create $TUSER01"
		cat $STF_TMPDIR/useradd.out.$$
		cleanup 1
	fi

	# setup testing group
	if [[ -z $TESTGRP ]]; then
		echo "Testing group <TESTGRP> must be specified"
		cleanup 1
	fi

	sharemgr list | grep -w "$TESTGRP" > /dev/null 2>&1
	if (( $? != 0 )); then
		sharemgr create -P nfs $TESTGRP > $STF_TMPDIR/sh-create.$$ 2>&1
		if (( $? != 0 )); then
			echo "could not create $TESTGRP"
			cat $STF_TMPDIR/sh-create.$$
			cleanup 1
		fi
	else
		GrpStat=$(sharemgr list -v | grep -w "$TESTGRP" | \
			awk '{print $2}')
		if [[ $GrpStat == "disabled" ]]; then
			sharemgr enable $TESTGRP
			if (( $? != 0 )); then
				echo "could not enable $TESTGRP"
				cleanup 1
			fi
		fi
		echo "$TESTGRP $GrpStat" > $TESTGRPSTAT
	fi


	# get NFSMAPID_DOMAIN and client will use it
	srv_nfsmapid_domain=`sharectl get -p nfsmapid_domain nfs| \
		awk -F= '{print $2}'`
	echo "SRV_NFSMAPID_DOMAIN=$srv_nfsmapid_domain"

	# set up ZFS
	if [[ -n $ZFSPOOL ]]; then
		ZFSBASE=$(zfs list -o mountpoint,name \
			| egrep "^$TESTDIR " | nawk '{print $2}')
		if (( $? == 0 )) && [[ -n $ZFSBASE ]]; then
			zfs destroy -r -f $ZFSBASE > $STF_TMPDIR/cleanFS.out.$$ 2>&1
			if (( $? != 0 )); then
				echo "WARNING, unable to destroy [$ZFSBASE];"
				cat $STF_TMPDIR/cleanFS.out.$$
				echo "\t Please clean it up manually."
				cleanup 2
			fi
		fi

		create_zfs_fs $ZFSPOOL $TESTDIR > $STF_TMPDIR/zfs.out.$$ 2>&1
		if (( $? != 0 )); then
			echo "$NAME: failed to create_zfs_fs $TESTDIR - "
			cat $STF_TMPDIR/zfs.out.$$
			cleanup 2
		fi

		ZFSBASE=$(zfs list -o mountpoint,name \
			| egrep "^$TESTDIR " | nawk '{print $2}')
		echo "export ZFSBASE=$ZFSBASE" >> $STF_TMPDIR/srv_config.vars

		create_zfs_fs $ZFSBASE $SHRDIR > $STF_TMPDIR/zfs.out.$$ 2>&1
		if (( $? != 0 )); then
			echo "$NAME: failed to create_zfs_fs $SHRDIR -"
			cat $STF_TMPDIR/zfs.out.$$
			cleanup 2
		fi
		print_debug $STF_TMPDIR/zfs.out.$$
	else
		rm -rf $TESTDIR
		mkdir -pm 0777 $SHRDIR
		if (( $? != 0 )); then
			echo "$NAME: could not create $SHRDIR"
			cleanup 1
		fi
	fi

	nfs_smf_setup "rw" $SHRDIR $SMF_TIMEOUT > $STF_TMPDIR/setup.$$ 2>&1
	if (( $? != 0 )); then
		echo "$NAME: nfs_smf_setup failed for $SHRDIR."
		cat $STF_TMPDIR/setup.$$
		cleanup 1
	fi
	print_debug $STF_TMPDIR/setup.$$

	/usr/sbin/svcadm refresh $SRV_FMRI
	sleep 5

	# Check the state of the SMF FMRI's to verify this.
	for fmri in $LCK_FMRI $STAT_FMRI ; do
		smf_fmri_transition_state "do" $fmri "online" $SMF_TIMEOUT
		if (( $? != 0 )); then
			echo "$NAME: unable to set $fmri to state online"
			cleanup 1
		fi
	done

	# Create few test files for tests/shmnt_file
	cd $SHRDIR
	rm -f rwfile rootfile nopermfile
	cp $0 rwfile; chmod 666 rwfile
	cp $0 rootfile; chmod 644 rootfile
	head -22 $0 > nopermfile; chmod 400 nopermfile

	echo "Done - setup NFSD/MOUNTD, and SHRDIR."
	;;
r)
	SHRDIR=$OPTARG
	# Unshare SHRDIR and reshare it with option provided by client
	if [[ ! -f $SHROPTS ]]; then
		echo "$NAME: Can't find <$SHROPTS> file"
		exit 2
	fi
	ShrOpts=$(cat $SHROPTS)

	$MISCSHARE $TESTGRP unshare $SHRDIR > $STF_TMPDIR/unshare.$$
	if (( $? != 0 )); then
		echo "Failed - unshare $SHRDIR"
		cat $STF_TMPDIR/unshare.$$ 2>&1
		cleanup 2
	fi
	print_debug $STF_TMPDIR/unshare.$$

	$MISCSHARE $TESTGRP share $SHRDIR $ShrOpts > $STF_TMPDIR/share.$$ 2>&1
	if (( $? != 0 )); then
		echo "$NAME: failed to share $SHRDIR with <$ShrOpts> options"
		cat $STF_TMPDIR/share.$$
		cleanup 2
	fi
	print_debug $STF_TMPDIR/share.$$

	# sharemgr/share prints share_options in random order
	NSopts=$(echo $ShrOpts | sed 's/,/ /g')
	for opt in $NSopts; do
		if echo $opt | grep ":" | grep "sec=" > /dev/null; then
			opt_name=${opt%%=*}
			opt=$(echo $opt | sed "s/:/,.*$opt_name=/g")
		fi
		condition="share | grep \" $SHRDIR \" | egrep \"$opt\" \
			> $STF_TMPDIR/share.$$ 2>&1"
		wait_now 10 "$condition"
		if (( $? != 0 )); then
		    echo "$NAME: share -o <$ShrOpts> $SHRDIR was unsuccessful"
		    echo "\tExpected to see <$opt> from share:"
		    share
		    cleanup 2
		fi
	done

	echo "Done - reshare SHRDIR with <$ShrOpts> OK"
	;;
c)
	EXIT_CODE=0
	# cleanup SHRDIR
	nfs_smf_clean $SHRDIR $SMF_TIMEOUT >> $STF_TMPDIR/cleanup.$$ 2>&1
	if (( $? != 0 )); then
		echo "Failed - cleanup server program."
		echo "\t nfs_smf_clean $SHRDIR"
		cat $STF_TMPDIR/cleanup.$$
		(( EXIT_CODE += 1 ))
	fi
	print_debug $STF_TMPDIR/cleanup.$$
	sleep 5

	# destory zfs of TESTDIR
	if [[ -n $ZFSPOOL ]]; then
		ZFSBASE=$(zfs list -o mountpoint,name \
			| egrep "^$TESTDIR " | nawk '{print $2}')
		if (( $? == 0 )) && [[ -n $ZFSBASE ]]; then
			zfs destroy -r -f $ZFSBASE > $STF_TMPDIR/cleanFS.out.$$ 2>&1
			if (( $? != 0 )); then
				echo "WARNING, unable to cleanup [$ZFSBASE];"
				cat $STF_TMPDIR/cleanFS.out.$$
				echo "\t Please clean it up manually."
				(( EXIT_CODE += 1 ))
			fi
		fi
	fi
	rm -rf $SHRDIR $TESTDIR # if ufs, remove it directly

	# remove BASEDIR if needed
	ls -d $BASEDIR/clnt_* > $STF_TMPDIR/cleanup.$$ 2>&1
	if (( $? == 0 )); then
		echo "Warning: $BASEDIR is not removed for the existing dirs:"
		cat $STF_TMPDIR/cleanup.$$
	else
		rm -rf $BASEDIR > $STF_TMPDIR/cleanup.$$ 2>&1
		if (( $? != 0 )); then
			echo "Failed - cleanup server program."
			echo "can not remove the directories $BASEDIR"
			cat $STF_TMPDIR/cleanup.$$
			(( EXIT_CODE += 1 ))
		fi
	fi

	# remove/restore testing group
	if [[ -f $TESTGRPSTAT ]]; then
		GrpStat=$(grep $TESTGRP $TESTGRPSTAT | awk '{print $2}')
		if [[ $GrpStat == "disabled" ]]; then
			sharemgr disable $TESTGRP
			if (( $? != 0 )); then
				echo "Waring: disable $TESTGRP failed."
				sharemgr list -v | grep $TESTGRP
				(( EXIT_CODE += 1 ))
			fi
		fi
		rm -f $TESTGRPSTAT
	else
		sharemgr delete -f $TESTGRP
		if (( $? != 0 )); then
			echo "Warning: $TESTGRP is not removed"
			sharemgr show -pv $TESTGRP
			(( EXIT_CODE += 1 ))
		fi
	fi

	# delete test user
	del_users $TUSER_UTAG > $STF_TMPDIR/userdel.out.$$ 2>&1
	if (( $? != 0 )); then
		echo "WARNING, failed to delete test users whose tag: $TUSER_UTAG"
		cat $STF_TMPDIR/userdel.out.$$
		(( EXIT_CODE += 1 ))
	fi

	(( EXIT_CODE == 0 )) && echo "Done - cleanup server program."
	;;
f)
	typeset do=$OPTARG
	shift $((OPTIND - 1))
	typeset fmri=$1
	typeset expstat="online"
	[[ $do == disable ]] && expstat="disabled"
	svcadm $do $fmri
	if (( $? != 0 )); then
		echo "$NAME: unable to $do $fmri"
		cleanup 1
	fi
	wait_now 10 "svcs $fmri | grep -w $expstat > /dev/null 2>&1"
	if (( $? != 0 )); then
		echo "$NAME: failed to $do $fmri"
		cleanup 1
	fi
	echo "Done - $do $fmri OK"
	;;
u)
	SHRDIR=$OPTARG
	# find the shared file from sharetab and unshare it
	REAL_SHRDIR=$(grep "$SHRDIR" /etc/dfs/sharetab | awk '{print $1}')

	if [[ -n $REAL_SHRDIR ]]; then
		$MISCSHARE $TESTGRP unshare $REAL_SHRDIR >$STF_TMPDIR/unshare.$$
		if (( $? != 0 )); then
			echo "Failed - unshare $REAL_SHRDIR"
			cat $STF_TMPDIR/unshare.$$ 2>&1
			cleanup 2
		fi
		print_debug $STF_TMPDIR/unshare.$$
	fi

	echo "[-u] option: unshare <$REAL_SHRDIR> OK"
	;;

\?)
	echo $Usage
	exit 2
	;;

esac

cleanup $EXIT_CODE
