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
configfile=$1
#ZONE_PATH=${ZONE_PATH%%/}
#STRESSDIR=${ZONE_PATH}${NFSSHRDIR}/stress
#STRESSMNT=${ZONE_PATH}${NFSMNTDIR}/stress

. $STF_SUITE/include/sharemnt.kshlib

function cleanup {              # cleanup and exit
	[[ :$SHAREMNT_DEBUG: = *:${NAME}:* \
		|| :${SHAREMNT_DEBUG}: == *:all:* ]] && set -x

	rm -f $STF_TMPDIR/*.out.$$ $STF_TMPDIR/sharemnt.stress
	exit ${1}
}

# Turn on debug info, if requested
export STC_GENUTILS_DEBUG=$STC_GENUTILS_DEBUG:$SHAREMNT_DEBUG
[[ :$SHAREMNT_DEBUG: == *:$NAME:* \
	|| :$SHAREMNT_DEBUG: == *:all:* ]] && set -x

client_num=$(get_clients_num)
if (( $? != 0 )); then
	echo "\n$NAME: RSH failed, $client_num"
	exit $STF_UNRESOLVED
elif (( $client_num != 1 )); then
	echo "\n$NAME: multiple srv_shmnt files were found on the server."
	echo "\tthe stress tests don't support multiple clients\n"
	exit $STF_UNTESTED
fi

if [[ -z "$STRESS_TIMEOUT" ]]; then
	if (( $NUM_SHARES <= 2000 && \
		$((NUM_GROUPS * NUM_ENTRYS)) <= 1000 )); then
		let STRESS_TIMEOUT=60*60
	else
		let STRESS_TIMEOUT=2*60*60
	fi
fi

#
# Now setup the server..
#
# Firstly add environment variables to srv_setup script and
# create a new script called server.stress which will be copied over to
# the server:
sed -e "s%STF_TMPDIR_from_client%${SRV_TMPDIR}%" \
	-e "s%STRESS_TIMEOUT_from_client%${STRESS_TIMEOUT}%" \
	-e "s%NUM_SHARES_from_client%${NUM_SHARES}%" \
	-e "s%NUM_GROUPS_from_client%${NUM_GROUPS}%" \
	-e "s%NUM_ENTRYS_from_client%${NUM_ENTRYS}%" \
	-e "s%SHAREMNT_DEBUG_from_client%${SHAREMNT_DEBUG}%" \
	$DIR/srv_setup > ${STF_TMPDIR}/sharemnt.stress \
	2> $STF_TMPDIR/sed.out.$$
if (( $? != 0 )); then
	echo "$NAME: failed to create [sharemnt.stress] file."
	cat $STF_TMPDIR/sed.out.$$
	echo "PATH is $PATH"
	cleanup $STF_UNINITIATED
fi

#
# copy srv_setup to server, which is called to do setup/cleanup.
scp $STF_TMPDIR/sharemnt.stress root@$SERVER:$SRV_TMPDIR/sharemnt.stress \
	> $STF_TMPDIR/scp.out.$$ 2>&1
if (( $? != 0 )); then
	echo "$NAME: failed to copy <srv_setup> to $SERVER:"
	cat $STF_TMPDIR/scp.out.$$
	cleanup $STF_FAIL
fi

# ..finally execute the script on the server.
SRVDEBUG=$STC_GENUTILS_DEBUG:$SHAREMNT_DEBUG
[[ :$SRVDEBUG: == *:RSH:* ]] && SRVDEBUG=all
RSH root $SERVER \
	"export SHAREMNT_DEBUG=$SRVDEBUG; \
	 F=$SRV_TMPDIR/sharemnt.stress; \
	 chmod 0555 \$F && \$F -s" \
	> $STF_TMPDIR/rsh.out.$$ 2>&1
rc=$?
print_debug $STF_TMPDIR/rsh.out.$$
grep "Done" $STF_TMPDIR/rsh.out.$$ > /dev/null 2>&1
if [[ $? != 0 || $rc != 0 ]]; then
	echo "$NAME: run $SRV_TMPDIR/sharemnt.stress in $SERVER failed:"
	cat $STF_TMPDIR/rsh.out.$$
	cleanup $STF_FAIL
fi

# create some dirs as mount point in client.
i=0
while (( $i <= $NUM_SHARES )); do
	mkdir -p $STRESSMNT/mntdir_${i}_stress
	i=$((i+1))
done

# the stress test might need to increase STF_TIMEOUT
# set STF_TIMEOUT and STRESS_TIMEOUT to stf configuration file
let STF_TIMEOUT=${STRESS_TIMEOUT}*4
echo "export STF_TIMEOUT=$STF_TIMEOUT" > $configfile
echo "export STRESS_TIMEOUT=$STRESS_TIMEOUT" >> $configfile

cleanup $STF_PASS
