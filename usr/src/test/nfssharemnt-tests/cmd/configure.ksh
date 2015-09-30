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

DIR=$(dirname $0)
NAME=$(basename $0)

# Directory for tmp files
STF_TMPDIR=${STF_TMPDIR:-$STF_CONFIG}
STF_TMPDIR=$STF_TMPDIR/TMPDIR_$(date "+%Y-%m-%d-%H-%M-%S" | sed 's/-//g')
mkdir -m 0777 -p $STF_TMPDIR

. $STF_SUITE/include/sharemnt.kshlib
. $STC_GENUTILS/include/libsmf.shlib
. $STC_GENUTILS/include/nfs-tx.kshlib
. $STC_GENUTILS/include/nfs-smf.kshlib

# Turn on debug info, if requested
export STC_GENUTILS_DEBUG=$STC_GENUTILS_DEBUG:$SHAREMNT_DEBUG
[[ :$SHAREMNT_DEBUG: == *:${NAME}:* \
	|| :${SHAREMNT_DEBUG}: == *:all:* ]] && set -x

CLIENT=${CLIENT:-$(hostname)}
clt_ip=$(getent ipnodes $CLIENT | head -1 | awk '{print $1}')
srv_ip=$(getent ipnodes $SERVER | head -1 | awk '{print $1}')
if [[ $clt_ip == $srv_ip ]]; then
	echo "$NAME: SERVER<$SERVER> can't be set to \c"
	echo "the same as localhost."
	exit $STF_UNINITIATED
fi

SERVER_S=$(getent ipnodes $srv_ip | head -1 | awk '{print $NF}')
if (( $? != 0 )); then
	echo "$NAME: Can't get SERVER's name on $CLIENT"
	exit $STF_UNINITIATED
fi
ping $SERVER_S > $STF_TMPDIR/ping.out.$$ 2>&1
if (( $? != 0 )); then
	echo "$NAME: SERVER<$SERVER_S> not responding to pings:"
	cat $STF_TMPDIR/ping.out.$$
	rm -f $STF_TMPDIR/ping.out.$$
	exit $STF_UNINITIATED
fi

CLIENT_S=$(get_hostname_remote $clt_ip $SERVER_S)
if (( $? != 0 )); then
	echo "$NAME: Can't get CLIENT's name on $SERVER"
	exit $STF_UNINITIATED
fi

TUID01=$(get_free_uid $SERVER)
(( $? != 0 )) && echo "$NAME: Can't get a unused uid" && exit $STF_UNINITIATED
TUSER01="SM"$TUID01 # SM is Share-Mount for short
# Unique name for client used on server
TCLIENT=$CLIENT_S"."$clt_ip

function cleanup {	# cleanup and exit
	[[ :$SHAREMNT_DEBUG: == *:${NAME}:* \
		|| :${SHAREMNT_DEBUG}: == *:all:* ]] && set -x

	rm -f $STF_TMPDIR/*.out.$$ $STF_TMPDIR/srv_setup
	exit ${1}
}

configfile=$1
export ZONE_PATH=${ZONE_PATH%%/}
# Path for SERVER to test
export BASEDIR=${ZONE_PATH}${NFSSHRDIR}
export TESTDIR=${ZONE_PATH}${NFSSHRDIR}/$TCLIENT
# Path for SERVER to share
export SHRDIR=$TESTDIR/common
# Path for sharetab to share
export SHARETABDIR=$TESTDIR/sharetab
# Path for nfslogd to share
export NFSLOGDDIR=$TESTDIR/nfslogd
# Path for stress test
export STRESSDIR=$TESTDIR/stress
# Path for localhost to mount
export MNTDIR=$ZONE_PATH$NFSMNTDIR/common
# Path for stress test to mount
export STRESSMNT=$ZONE_PATH$NFSMNTDIR/stress
# Path for others to share
export OTHERDIR=$TESTDIR/others
# Path for misc_opts to share
export QUOTADIR=$TESTDIR/misc_opts_quota
# Path for automount to mount
export AUTOIND=$ZONE_PATH$AUTOIND
export CLIENT=$CLIENT
export CLIENT_S=$CLIENT_S
export SERVER_S=$SERVER_S
# Testing Group for SERVER to test
export TESTGRP=${TESTGRP:-"shmnt_grp"}
export TUID01=$TUID01
export TUSER01=$TUSER01
export STF_TMPDIR=$STF_TMPDIR
export SRV_TMPDIR=/var/tmp/TMPDIR_shmnt_$TCLIENT # must be fixed dir

# also write path variable to $configfile
cat >> $configfile <<-EOF
export ZONE_PATH=$ZONE_PATH
export BASEDIR=$BASEDIR
export TESTDIR=$TESTDIR
export SHRDIR=$SHRDIR
export NFSLOGDDIR=$NFSLOGDDIR
export STRESSDIR=$STRESSDIR
export MNTDIR=$MNTDIR
export STRESSMNT=$STRESSMNT
export AUTOIND=$AUTOIND
export OTHERDIR=$OTHERDIR
export QUOTADIR=$QUOTADIR
export SHARETABDIR=$SHARETABDIR
export CLIENT=$CLIENT
export CLIENT_S=$CLIENT_S
export SERVER_S=$SERVER_S
export TESTGRP=$TESTGRP
export TUID01=$TUID01
export TUSER01=$TUSER01
export STF_TMPDIR=$STF_TMPDIR
export SRV_TMPDIR=$SRV_TMPDIR
export STC_GENUTILS=$STC_GENUTILS
EOF
(( $? != 0 )) && echo "Could not write to $configfile file" && \
	exit $STF_UNINITIATED

#
# create srv_config.vars
#
cat >> $STF_TMPDIR/srv_config.vars <<-EOF
export PATH=/usr/sbin:/usr/bin:/usr/lib/nfs:$SRV_TMPDIR:$STC_GENUTILS/bin:\$PATH
export STC_GENUTILS=$STC_GENUTILS
export STF_TMPDIR=$SRV_TMPDIR
export CLIENT_S=$CLIENT_S
export BASEDIR=$BASEDIR
export TESTDIR=$TESTDIR
export SHRDIR=$SHRDIR
export NFSLOGDDIR=$NFSLOGDDIR
export SHARETABDIR=$SHARETABDIR
export OTHERDIR=$OTHERDIR
export QUOTADIR=$QUOTADIR
export STRESSDIR=$STRESSDIR
export TESTGRP=$TESTGRP
export TUID01=$TUID01
export TUSER01=$TUSER01
export TUSER_UTAG=$TUSER_UTAG
export SRV_FMRI="svc:/network/nfs/server:default"
export LCK_FMRI="svc:/network/nfs/nlockmgr:default"
export STAT_FMRI="svc:/network/nfs/status:default"
export QUOTA_FMRI="svc:/network/nfs/rquota:default"
export SMF_TIMEOUT=60
export SHARETAB="/etc/dfs/sharetab"
export MISCSHARE=$SRV_TMPDIR/miscshare
EOF

#
# Now setup the server..
#
# Check TX related info
check_for_cipso "$TESTDIR" "$MNTDIR" "$MNTOPT" || return $STF_UNSUPPORTED

#
# create SRV_TMPDIR on server
#
RSH root $SERVER \
	"rm -rf $SRV_TMPDIR; \
	 mkdir -pm 0777 $SRV_TMPDIR"
	> $STF_TMPDIR/rsh.out.$$ 2>&1
if (( $? != 0 )); then
	echo "$NAME: failed to create <$SRV_TMPDIR> on $SERVER:"
	cat $STF_TMPDIR/rsh.out.$$
	cleanup $STF_UNINITIATED
fi


#
# add environment variables to srv_setup script and
# create a new script called srv_setup which will be copied over to
# the server:
#
sed -e "s%STF_TMPDIR_from_client%${SRV_TMPDIR}%" \
	-e "s%SHAREMNT_DEBUG_from_client%${SHAREMNT_DEBUG}%" \
	$DIR/srv_setup > ${STF_TMPDIR}/srv_setup \
	2> $STF_TMPDIR/sed.out.$$
if (( $? != 0 )); then
	echo "$NAME: failed to create [srv_setup] file."
	cat $STF_TMPDIR/sed.out.$$
	echo "PATH is $PATH"
	cleanup $STF_UNINITIATED
fi

# setup miscshare on server
sed -e "s%STF_TMPDIR_from_client%${SRV_TMPDIR}%" \
	-e "s%SHAREMNT_DEBUG_from_client%${SHAREMNT_DEBUG}%" \
	$STF_SUITE/bin/miscshare > $STF_TMPDIR/miscshare \
	2> $STF_TMPDIR/sed.out.$$
if (( $? != 0 )); then
	echo "$NAME: failed to create [miscshare] file."
	cat $STF_TMPDIR/sed.out.$$
	echo "PATH is $PATH"
	cleanup $STF_UNINITIATED
fi
chmod 0555 $STF_TMPDIR/miscshare

# remove stf.kshlib from sharemnt.kshlib as it is not necessary on server
sed '/stf.kshlib/d' $STF_SUITE/include/sharemnt.kshlib > \
	$STF_TMPDIR/sharemnt.kshlib 2> $STF_TMPDIR/sed.out.$$
if (( $? != 0 )); then
	echo "$NAME: failed to create [sharemnt.kshlib] file."
	cat $STF_TMPDIR/sed.out.$$
	echo "PATH is $PATH"
	cleanup $STF_UNINITIATED
fi

if RSH root $SERVER "[[ ! -s $STC_NFSUTILS/include/nfs-util.kshlib ]]"; then
	server_files="\
		$STC_GENUTILS/bin/stc_genutils \
		$STC_NFSUTILS/include/nfs-util.kshlib \
		$STC_GENUTILS/include/nfs-smf.kshlib \
		$STC_GENUTILS/include/libsmf.shlib"
fi
server_files="$server_files \
	$STF_TMPDIR/sharemnt.kshlib \
	$STF_TMPDIR/srv_config.vars \
	$STF_TMPDIR/srv_setup \
	$STF_TMPDIR/miscshare"
scp $server_files root@$SERVER:$SRV_TMPDIR > $STF_TMPDIR/scp.out.$$ 2>&1
if (( $? != 0 )); then
	echo "$NAME: failed to copy <$server_files> to $SERVER:"
	cat $SRV_TMPDIR/scp.out.$$
	cleanup $STF_FAIL
fi

# ..finally execute the script on the server.
SRVDEBUG=$STC_GENUTILS_DEBUG:$SHAREMNT_DEBUG
[[ :$SRVDEBUG: == *:RSH:* ]] && SRVDEBUG=all
RSH root $SERVER \
	"export SHAREMNT_DEBUG=$SRVDEBUG; \
	 F=$SRV_TMPDIR/srv_setup; \
	 chmod 0555 \$F && \$F -s" \
	> $STF_TMPDIR/rsh.out.$$ 2> $STF_TMPDIR/rsh.err.$$
rc=$?
print_debug $STF_TMPDIR/rsh.err.$$
grep "Done" $STF_TMPDIR/rsh.out.$$ > /dev/null 2>&1
if (( $? != 0 || $rc != 0 )); then
	echo "$NAME: run $SRV_TMPDIR/srv_setup in $SERVER failed:"
	cat $STF_TMPDIR/rsh.out.$$
	cat $STF_TMPDIR/rsh.err.$$
	cleanup $STF_FAIL
fi

fs_info=$(egrep '^SRV_FS=' $STF_TMPDIR/rsh.out.$$ | awk -F= '{print $2}')
fs_type=$(echo $fs_info | awk '{print $1}')
if [[ $fs_type == zfs ]]; then
	ZFSPOOL=$(echo $fs_info | awk '{print $2}')
	echo "ZFSPOOL=$ZFSPOOL; export ZFSPOOL" >> $configfile
fi

# Now setup the client

# set NFSMAPID_DOMAIN
NFSMAPID_DOMAIN=$(egrep '^SRV_NFSMAPID_DOMAIN=' $STF_TMPDIR/rsh.out.$$ \
					| awk -F= '{print $2}')
set_nfs_property nfsmapid_domain $NFSMAPID_DOMAIN $1 \
	>$STF_TMPDIR/mapid.out.$$ 2>&1
if (( $? != 0 )); then
	echo "$NAME: failed to set nfsmapid_domain"
	cat $STF_TMPDIR/mapid.out.$$
	cleanup $STF_FAIL
fi
print_debug $STF_TMPDIR/mapid.out.$$

#
# We set the auto_enable property of all the NFS services to "false" to
# ensure that they are not silenty re-enabled after we have disabled
# them.
#
# Note svc:/network/nfs/client does not have an auto_enable property.
#
set_fmri_svcprop $STF_CONFIG/svc_prop.orig $SERVICES
if (( $? != 0 )); then
	echo "$NAME: failed to set auto_enable property for <$SERVICES>"
	cleanup $STF_UNINITIATED
fi

#
# Mount the NFS directory with specified options
#
[[ ! -d $MNTDIR ]] && mkdir -pm 0777 $MNTDIR > /dev/null 2>&1
umount -f $MNTDIR > /dev/null 2>&1
mount -o ${MNTOPT} ${SERVER}:${SHRDIR} $MNTDIR > $STF_TMPDIR/mnt.out.$$ 2>&1
if (( $? != 0 )); then
	echo "$NAME: client<$CLIENT> failed to mount ${SERVER}:${SHRDIR}."
	cat $STF_TMPDIR/mnt.out.$$
	cleanup $STF_FAIL
fi

# Ensure that we do not start the tests until the server is responding
# properly (i.e. is not in grace etc)
#
echo "This is a rofile for sharemnt Testing" > $MNTDIR/rofile 2>&1
if (( $? != 0 )); then
	echo "$NAME: client<$CLIENT> failed to create <$MNTDIR/rofile> file"
	cat $MNTDIR/rofile
	cleanup $STF_UNRESOLVED
fi
chmod 444 $MNTDIR/rofile

# Prepare client's automount maps as well
#
if [[ ! -f $STF_CONFIG/auto_master.shmnt.orig ]]; then
	cp -p /etc/auto_master $STF_CONFIG/auto_master.shmnt.orig \
		2> $STF_TMPDIR/cp.out.$$
	if (( $? != 0 )); then
		echo "$NAME: client<$CLIENT> failed to save auto_master file"
		cat $STF_TMPDIR/cp.out.$$
		cleanup $STF_UNRESOLVED
	fi
fi
egrep -v "shmnt|sharemnt" $STF_CONFIG/auto_master.shmnt.orig > /etc/auto_master
echo "##\n# Added for testing sharemnt tests\n#" >> /etc/auto_master
echo "/- $STF_TMPDIR/auto_direct.shmnt" >> /etc/auto_master
echo "$AUTOIND $STF_TMPDIR/auto_indirect.shmnt" >> /etc/auto_master
echo "# this file is used for direct auto-map of sharemnt tests" \
	> $STF_TMPDIR/auto_direct.shmnt
echo "# this file is used for indirect auto-map of sharemnt tests" \
	> $STF_TMPDIR/auto_indirect.shmnt

smf_fmri_transition_state "do" $AUTO_FMRI "restart" 60 \
	> $STF_TMPDIR/smf.out.$$ 2>&1
if (( $? != 0 )); then
	echo "$NAME: client<$CLIENT> failed to restart autofs service"
	cat $STF_TMPDIR/smf.out.$$
	cleanup $STF_FAIL
fi
print_debug $STF_TMPDIR/smf.out.$$

# create test user
useradd -u $TUID01 -c $TUSER_UTAG -d /tmp $TUSER01 \
	>$STF_TMPDIR/useradd.out.$$ 2>&1
ckresult $? "$NAME: failed to add $TUSER01" $STF_TMPDIR/useradd.out.$$ \
	|| cleanup $STF_FAIL

# verify the RDMA connection if TESTRDMA=yes
echo $TESTRDMA | grep -i no > /dev/null 2>&1
if (( $? != 0 )); then
	# user wants to test NFS/RDMA
	nfsstat -m $MNTDIR > $STF_TMPDIR/nstat.out.$$ 2>&1
	grep 'Flags:' $STF_TMPDIR/nstat.out.$$ | grep 'proto=rdma' > /dev/null 2>&1
	if (( $? != 0 )); then
		echo "$NAME: WARNING:"
		echo "\t TESTRDMA=<$TESTRDMA>, but client didn't mount <proto=rdma>"
		echo "\t nfsstat -m $MNTDIR got:"
		cat $STF_TMPDIR/nstat.out.$$
		echo "\t No <proto=rdma> will be generated and run."
		echo "export TESTRDMA=no" >> $configfile
	else
		echo "export TESTRDMA=yes" >> $configfile
	fi
fi

cleanup $STF_PASS
