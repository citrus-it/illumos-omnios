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
. $STC_GENUTILS/include/nfs-smf.kshlib
. $STC_GENUTILS/include/libsmf.shlib
EXIT_CODE=$STF_PASS

# Turn on debug info, if requested
export STC_GENUTILS_DEBUG=$STC_GENUTILS_DEBUG:$SHAREMNT_DEBUG
[[ :$SHAREMNT_DEBUG: == *:${NAME}:* \
	|| :${SHAREMNT_DEBUG}: == *:all:* ]] && set -x

function cleanup {		# cleanup and exit
	[[ :$SHAREMNT_DEBUG: == *:${NAME}:* \
		|| :${SHAREMNT_DEBUG}: == *:all:* ]] && set -x

	rm -fr $STF_TMPDIR

	exit ${1}
}

# umount $MNTDIR if it is still mounted
nfsstat -m $MNTDIR | grep "$MNTDIR" > /dev/null 2>&1
if (( $? == 0 )); then
	umount -f $MNTDIR > $STF_TMPDIR/umnt.out.$$ 2>&1
	if (( $? != 0 )); then
		echo "$NAME: Warning - umount MNTDIR=[$MNTDIR] failed -"
		cat $STF_TMPDIR/umnt.out.$$
		echo "\t... please clean it up manually."
		EXIT_CODE=$STF_FAIL
	else
		rm -fr $ZONE_PATH$NFSMNTDIR
	fi
else
	rm -fr $ZONE_PATH$NFSMNTDIR
fi

# Also cleanup the test automount maps
if [[ ! -f $STF_CONFIG/auto_master.shmnt.orig ]]; then
	echo "$NAME: Warning - can't find the original auto_master"
	echo "\t... please reset the auto_master map manaully"
else
	grep -v "direct\.shmnt" $STF_CONFIG/auto_master.shmnt.orig \
		> /etc/auto_master 2> $STF_TMPDIR/map.out.$$
	if (( $? != 0 )); then
		echo "$NAME: Failed to restore the original auto_master"
		echo "\t... please recreate the auto_master map manaully"
		cat $STF_TMPDIR/map.out.$$ /etc/auto_master
		EXIT_CODE=$STF_FAIL
	fi
fi

smf_fmri_transition_state "do" "$AUTO_FMRI" "restart" 60 \
	> $STF_TMPDIR/smf.out.$$ 2>&1
if (( $? != 0 )); then
	echo "$NAME: client<$(hostname)> failed to restart autofs service"
	cat $STF_TMPDIR/smf.out.$$
fi
print_debug $STF_TMPDIR/smf.out.$$

#
# Restore the auto_enable property of the NFS services back to original value
# Note svc:/network/nfs/client does not have an auto_enable property.
#
restore_fmri_svcprop $STF_CONFIG/svc_prop.orig $SERVICES
if (( $? != 0 )); then
	echo "$NAME: failed to restore auto_enable property for <$SERVICES>"
	cat $STF_CONFIG/svc_prop.orig
	EXIT_CODE=$STF_FAIL
fi

# Make sure automountd is started before exit
MyZone=$(zonename)
condition="pgrep -z $MyZone automountd > /dev/null 2>&1"
wait_now 20 "$condition"
rc=$?
if (( $rc != 0 )); then
	echo "$NAME: client<$(hostname)> automountd is still not running"
	echo "\tafter $rc seconds with <$AUTO_FMRI> is online."
	pgrep -l -z $MyZone automountd
	svcs -lp $AUTO_FMRI
	EXIT_CODE=$STF_FAIL
fi
# still need time to allow automounter to process after the restart
sleep 30
rm -fr $AUTOIND > /dev/null 2>&1

# restore mapid domain
restore_nfs_property nfsmapid_domain $STF_CONFIG/bin/stf_config.suite \
    >$STF_TMPDIR/mapid.out.$$ 2>&1
if (( $? != 0 )); then
	echo "$NAME: failed to restore nfsmapid_domain"
	cat $STF_TMPDIR/mapid.out.$$
	EXIT_CODE=$STF_FAIL
fi
print_debug $STF_TMPDIR/mapid.out.$$

# delete test user(s)
del_users $TUSER_UTAG > $STF_TMPDIR/userdel.out.$$ 2>&1
if (( $? != 0 )); then
	echo "$NAME: failed to delete test users whose tag: $TUSER_UTAG"
	cat $STF_TMPDIR/userdel.out.$$
	EXIT_CODE=$STF_FAIL
fi

# Now cleanup the server..
SRVDEBUG=$STC_GENUTILS_DEBUG:$SHAREMNT_DEBUG
[[ :$SRVDEBUG: == *:RSH:* ]] && SRVDEBUG=all
RSH root $SERVER \
	"export SHAREMNT_DEBUG=$SRVDEBUG; \
	F=$SRV_TMPDIR/srv_setup; \
	if [[ -f \$F ]]; then \$F -c; else echo NeedlessToDo; fi" \
	> $STF_TMPDIR/rsh.out.$$ 2>&1
rc=$?
print_debug $STF_TMPDIR/rsh.out.$$

egrep "^NeedlessToDo$" $STF_TMPDIR/rsh.out.$$ > /dev/null 2>&1
if (( $? == 0 && $rc == 0 )); then
	echo "$NAME: script<$SRV_TMPDIR/srv_setup> does not exist"
	echo "\t it seems we don't need to cleanup on SERVER<$SERVER>"
	cleanup $EXIT_CODE
fi

grep "Done" $STF_TMPDIR/rsh.out.$$ > /dev/null 2>&1
if (( $? != 0 || $rc != 0 )); then
	echo "$NAME: run cleanup script on SERVER<$SERVER> failed :"
	cat $STF_TMPDIR/rsh.out.$$
	echo "\t please cleanup the SERVER manually"
	EXIT_CODE=$STF_FAIL
else
	# Now remove STF_TMPDIR on server
	RSH root $SERVER "/bin/rm -rf $SRV_TMPDIR" \
		> $STF_TMPDIR/rsh.out.$$ 2>&1
	if (( $? != 0 )); then
		echo "$NAME: remove $SRV_TMPDIR on SERVER<$SERVER> failed:"
		cat $STF_TMPDIR/rsh.out.$$
		echo "\t please remove it on SERVER<$SERVER> manually"
		EXIT_CODE=$STF_FAIL
	fi
fi

cleanup $EXIT_CODE
