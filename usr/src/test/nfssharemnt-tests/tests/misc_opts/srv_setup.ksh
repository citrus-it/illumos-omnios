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
"
if (( $# < 1 )); then
	echo $Usage
	exit 99
fi

# variables gotten from client system:
STF_TMPDIR=STF_TMPDIR_from_client
SHAREMNT_DEBUG=${SHAREMNT_DEBUG:-"SHAREMNT_DEBUG_from_client"}

. $STF_TMPDIR/srv_config.vars

# Include common STC utility functions
if [[ -s $STC_GENUTILS/include/nfs-util.kshlib ]]; then
	. $STC_GENUTILS/include/nfs-util.kshlib
else
	. $STF_TMPDIR/nfs-util.kshlib
fi

PROG=$STF_TMPDIR/srv_setup

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
	# Create few test files/dirs
	cd $SHRDIR
	rm -rf dir0777 dir0755 dir0700
	mkdir -m 0777 dir0777
	mkdir -m 0755 dir0755
	mkdir -m 0700 dir0700
		
	if [[ -z $ZFSPOOL ]]; then
		F_LOFI=$STF_TMPDIR/lofifile.$(($$+1))

		# cleanup possible stuff which will block the coming setup 
		# a) cleanup quotadir
		if [[ -d $QUOTADIR ]]; then
			umount -f $QUOTADIR >/dev/null 2>&1
			rm -rf $QUOTADIR
		fi
		# b) cleanup lofi
		lofiadm | sed '1,2d' > $STF_TMPDIR/lofiadm.out.$$
		while read ldev file; do
			if [[ @$file == @${STF_TMPDIR}* ]]; then
				# it maybe created by current client but not destroyed
				lofiadm -d $ldev
			fi
		done < $STF_TMPDIR/lofiadm.out.$$

		create_lofi_fs $F_LOFI $QUOTADIR > $STF_TMPDIR/clofi.out.$$ 2>&1
		if (( $? != 0 )); then
		    echo "$NAME: failed to create_lofi_fs $QUOTADIR"
		    cat $STF_TMPDIR/clofi.out.$$
		    cleanup 1
		fi

		$PROG -f disable $QUOTA_FMRI | grep Done > /dev/null 2>&1
		if (( $? != 0 )); then
		    echo "$NAME: unable to disable $QUOTA_FMRI:"
		    cleanup 2
		fi

		touch $QUOTADIR/quotas
		quotaoff $QUOTADIR
		edquota $TUSER01 <<-EOF
		:s/hard = 0/hard = 10/
		:s/hard = 0/hard = 5/
		:wq
		EOF
		quotaon $QUOTADIR
		quota -v $TUSER01 | sed '1,2d' | grep "$QUOTADIR" > /dev/null 2>&1
		if (( $? != 0 )); then
			echo "$NAME: setup quota for user<$TUSER01> failed"
			cleanup 3
		fi

		$PROG -f enable $QUOTA_FMRI | grep Done > /dev/null 2>&1
		if (( $? != 0 )); then
		    echo "$NAME: unable to enable $QUOTA_FMRI:"
		    cleanup 4
		fi
	else
		create_zfs_fs $ZFSBASE $QUOTADIR 2m \
			> $STF_TMPDIR/czfs.out.$$ 2>&1
		if (( $? != 0 )); then
			echo "$NAME: failed to create_zfs_fs $QUOTADIR"
			cat $STF_TMPDIR/czfs.out.$$
			cleanup 5
		fi
	fi

	echo "Done - setup QUOTA."
	;;

c)
	EXIT_CODE=0
	$MISCSHARE $TESTGRP unshare $QUOTADIR \
		> $STF_TMPDIR/unshare.$$ 2>&1
	if (( $? != 0 )); then
		echo "WARNING: unshare $QUOTADIR failed"
		cat $STF_TMPDIR/unshare.$$
		echo "\t Please clean it up manually."
		EXIT_CODE=2
	fi

	if [[ -n $ZFSPOOL ]]; then
		Zfs=$(zfs list | grep "$QUOTADIR" | nawk '{print $1}')
		zfs destroy -f $Zfs > $STF_TMPDIR/cleanFS.out.$$ 2>&1
		if (( $? != 0 )); then
			echo "WARNING, unable to cleanup [$Zfs];"
			cat $STF_TMPDIR/cleanFS.out.$$
			echo "\t Please clean it up manually."
			EXIT_CODE=2
		fi
	else
		/bin/df -n /dev/lofi/* 2>/dev/null \
			| grep "$QUOTADIR:" > /dev/null 2>&1
		if (( $? != 0 )); then
			umount -f $QUOTADIR > /dev/null 2>&1
			rm -rf $QUOTADIR
		else
			destroy_lofi_fs $QUOTADIR \
				> $STF_TMPDIR/cleanFS.out.$$ 2>&1
			if (( $? != 0 )); then
				echo "WARNING, unable to cleanup [$QUOTADIR];"
				cat $STF_TMPDIR/cleanFS.out.$$
				echo "\t Please clean it up manually."
				EXIT_CODE=2
			fi
		fi
	fi
	(( EXIT_CODE == 0 )) && echo "Done - cleanup server program."
	cleanup $EXIT_CODE
	;;

\?)
	echo $Usage
	exit 2
	;;

esac

cleanup 0
