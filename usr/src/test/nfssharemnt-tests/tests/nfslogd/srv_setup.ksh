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
# Setup the SERVER for testing nfslogd.
#

NAME=$(basename $0)

Usage="Usage: $NAME -s | -c | -C \n
		-s: to setup this host for nfslogd test\n
		-c: to cleanup the server for nfslogd test\n
		-C: to check if nfslogd is running, if not, try to start it\n
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

# cleanup function on all exit
function cleanup {
	[[ :$SHAREMNT_DEBUG: == *:$NAME:* \
		|| :$SHAREMNT_DEBUG: == *:all:* ]] && set -x

	rm -fr $STF_TMPDIR/*.$$
	exit $1
}

# Turn on debug info, if requested
export STC_GENUTILS_DEBUG=$SHAREMNT_DEBUG
[[ :$SHAREMNT_DEBUG: == *:$NAME:* \
	|| :$SHAREMNT_DEBUG: == *:all:* ]] && set -x

Test_Log_Dir="/var/nfs/smtest"
Lock_Dir="/var/tmp/sharemnt_lock"
Zonename=$(zonename)
Timeout=600

getopts scC opt
case $opt in
  s)
	# check if multi-client are talking the server
	# Only first client does real setup, other clients 
	# just wait for the end.
	if [[ -f $Lock_Dir/.stf_configure && \
	    -f $Lock_Dir/.stf_unconfigure ]]; then
		# increase the referent count 
		ref_unconfig=$(cat $Lock_Dir/.stf_unconfigure)
		ref_unconfig=$((ref_unconfig + 1))
		echo "$ref_unconfig" > $Lock_Dir/.stf_unconfigure
		sync

		# wait first client to finish setup
		condition="(( \$(cat $Lock_Dir/.stf_configure) == 0 ))"
		wait_now $Timeout "$condition" 5
		if (( $? == 0 )); then
			echo "Done - Other client has finished the setup."
			exit 0
		fi

		echo "$NAME: TIMEOUT - Other clients can not finish setup \c"
		echo " after 10 minutes"
		exit 1
	else
		# create lock files for three phases.
		mkdir -p $Lock_Dir
		# "1" for config starting
		echo "1" > $Lock_Dir/.stf_configure
		# No test is running
		echo "0" > $Lock_Dir/.stf_execute
		# the current number of client
		echo "1" > $Lock_Dir/.stf_unconfigure
		sync
	fi

	if [[ ! -d $NFSLOGDDIR ]]; then
		mkdir -p -m 0777 $NFSLOGDDIR
		(( $? != 0 )) && echo "could not create $NFSLOGDDIR" && exit 1
	fi

	# set up ZFS
	if [[ -n $ZFSPOOL ]]; then
		create_zfs_fs $ZFSBASE $NFSLOGDDIR > $STF_TMPDIR/$NAME.zfs.$$ 2>&1
		if (( $? != 0 )); then
			echo "$NAME: failed to create_zfs_fs $NFSLOGDDIR"
			cat $STF_TMPDIR/$NAME.zfs.$$
			cleanup 99
		fi
	fi

	# Save original nfslog.conf and nfslogd config files
        mv /etc/nfs/nfslog.conf /etc/nfs/nfslog.conf.orig
        mv /etc/default/nfslogd /etc/default/nfslogd.orig
        cp $STF_TMPDIR/nfslog.conf /etc/nfs/nfslog.conf \
                > $STF_TMPDIR/$NAME.cp.$$ 2>&1
        if (( $? != 0 )); then
                echo "$NAME: ERROR - failed to cp [/etc/nfs/nfslog.conf]"
                cat $STF_TMPDIR/$NAME.cp.$$
                cleanup 99
        fi
        cp $STF_TMPDIR/nfslogd /etc/default/nfslogd \
                > $STF_TMPDIR/$NAME.cp.$$ 2>&1
        if (( $? != 0 )); then
                echo "$NAME: ERROR - failed to cp [/etc/default/nfslogd]"
                cat $STF_TMPDIR/$NAME.cp.$$
                cleanup 99
        fi

        pgrep -z $Zonename -x -u 0 nfslogd > /dev/null 2>&1
        if (( $? == 0 )); then
                # stop the daemon first
                pkill -HUP -z $Zonename -x -u 0 nfslogd
		condition="! pgrep -z $Zonename -x -u 0 nfslogd > /dev/null"
		wait_now 10 "$condition"
		(( $? != 0 )) && pkill -TERM -z $Zonename -x -u 0 nfslogd
        fi
        pgrep -z $Zonename -x -u 0 nfslogd > $STF_TMPDIR/$NAME.pgrep.$$ 2>&1
        if (( $? == 0 )); then
                echo "$NAME: ERROR - failed to kill nfslogd"
                cat $STF_TMPDIR/$NAME.pgrep.$$
                cleanup 99
        fi

	# remove the log file if exists
        rm -rf $Test_Log_Dir/

        # create the log directory
        mkdir $Test_Log_Dir
	cd $Test_Log_Dir; mkdir results defaults absolute;

	# start nfslogd
	touch /etc/nfs/nfslogtab
	/usr/lib/nfs/nfslogd > $STF_TMPDIR/$NAME.nfslogd.$$ 2>&1
	if (( $? != 0 )); then
	    echo "$NAME: ERROR - failed to start nfslogd"
	    cat $STF_TMPDIR/$NAME.nfslogd.$$
	    cleanup 99
	fi
	# wait a while and check nfslogd is running
	condition="pgrep -z $Zonename -x -u 0 nfslogd > /dev/null 2>&1"
	wait_now 20 "$condition"
	if (( $? != 0 )); then
	    echo "$NAME: ERROR - nfslogd is still not running after 20 seconds"
	    cleanup 99
	fi

	echo "0" > $Lock_Dir/.stf_configure	# "0" for setup end
	sync
        echo "Done - setup nfslogd OKAY."
	cleanup 0
        ;;
   c)
	if [[ ! -f $Lock_Dir/.stf_unconfigure ]]; then
	    echo "ERROR - failed to find lock file<$Lock_Dir/.stf_unconfigure>"
	    exit 1
	fi

	ref_unconfig=$(cat $Lock_Dir/.stf_unconfigure)
	if (( $ref_unconfig != 1 )); then
		ref_unconfig=$((ref_unconfig - 1))
		echo "$ref_unconfig" > $Lock_Dir/.stf_unconfigure
		sync
		echo "Done - other clients will do cleanup, ref=$ref_unconfig"
		exit 0
	fi

	# the last client will do real cleanup and remove all lock files
	rm -rf $Lock_Dir

	# stop the daemon and unshare
	pkill -HUP -z $Zonename -x -u 0 nfslogd
	$MISCSHARE $TESTGRP unshare $NFSLOGDDIR
	sleep 5

	if [[ -n $ZFSPOOL ]]; then 
		Zfs=$(zfs list | grep "$NFSLOGDDIR" | nawk '{print $1}')
		zfs destroy -f $Zfs > $STF_TMPDIR/$NAME.cleanFS.$$ 2>&1
		if (( $? != 0 )); then
			echo "WARNING, unable to cleanup [$Zfs];"
			cat $STF_TMPDIR/$NAME.cleanFS.$$
			echo "\t Please clean it up manually."
			cleanup 2
		fi
	fi

        # Restore original nfslog.conf and nfslogd config files:
        [[ -f /etc/nfs/nfslog.conf.orig ]] && \
                mv /etc/nfs/nfslog.conf.orig /etc/nfs/nfslog.conf
        [[ -f /etc/default/nfslogd.orig ]] && \
                mv /etc/default/nfslogd.orig /etc/default/nfslogd

		rm -rf $Test_Log_Dir $STF_TMPDIR/sharemnt.nfslogd.* \
		$STF_TMPDIR/test_nfslogd \
		$STF_TMPDIR/nfslog.conf $STF_TMPDIR/nfslogd

        echo "Done - restore nfslogd configure file OKAY"
        ;;
   C)
	# check if nfslogd is running on server
        pgrep -z $Zonename -x -u 0 nfslogd > /dev/null 2>&1
	if (( $? != 0 )); then
	    # start nfslogd
	    touch /etc/nfs/nfslogtab
	    /usr/lib/nfs/nfslogd > $STF_TMPDIR/$NAME.nfslogd.$$ 2>&1
	    if (( $? != 0 )); then
		echo "$NAME: ERROR - failed to start nfslogd"
		cat $STF_TMPDIR/$NAME.nfslogd.$$
		cleanup 99
	    fi
	    # wait a while and check nfslogd is running
	    condition="pgrep -z $Zonename -x -u 0 nfslogd > /dev/null 2>&1"
	    wait_now 20 "$condition"
	    if (( $? != 0 )); then
		echo "$NAME: ERROR - nfslogd is still not running \c"
		echo " after 20 seconds"
		cleanup 99
	    fi
	fi
        ;;
  \?)
        echo $Usage
        exit 2
        ;;
esac

cleanup 0
