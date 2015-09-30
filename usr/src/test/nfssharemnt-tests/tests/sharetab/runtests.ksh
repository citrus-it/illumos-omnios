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

export STC_GENUTILS_DEBUG=$STC_GENUTILS_DEBUG:$SHAREMNT_DEBUG
[[ :$SHAREMNT_DEBUG: == *:$NAME:* \
	|| :$SHAREMNT_DEBUG: == *:all:* ]] && set -x

typeset SRVDEBUG=$STC_GENUTILS_DEBUG:$SHAREMNT_DEBUG
[[ :$SRVDEBUG: == *:RSH:* ]] && SRVDEBUG=all


USAGE="Usage: $NAME <Tname> <opt>"

if (( $# < 2 )); then
	echo "$USAGE"
	exit $STF_UNRESOLVED
fi
typeset Tname=$1
typeset opt=$2

case $opt in
i|a|m|z|u)
	# i: initial check
	# a: Access /etc/dfs/sharetab as end user.
	# m: Change the state of mountd, verify the consistence of
	#    /etc/dfs/sharetab.
	# z/u: Share/unshare zfs as end user
	#      This section will not run until zfs delegation putback
	RSH root "$SERVER" \
		"export SHAREMNT_DEBUG=$SRVDEBUG; \
		$SRV_TMPDIR/sharemnt.shtab -$opt" \
		> $STF_TMPDIR/rsh.out.$$ 2>&1
	rc=$?
	;;
r)
	if [[ $SHRTAB_REBOOT != "TRUE" ]]; then
		echo "\n$Tname: UNTESTED, This case need to reboot server, \c"
		echo "if you want to test it,\n"
		echo "\tplease define SHRTAB_REBOOT as TRUE\n"
		exit $STF_UNTESTED
	fi
	# save the SHARETAB on server and reboot server
	RSH root "$SERVER" \
		"export SHAREMNT_DEBUG=$SRVDEBUG; \
		$SRV_TMPDIR/sharemnt.shtab -r 1" \
		> $STF_TMPDIR/rsh.out.$$ 2>&1
	grep "Done" $STF_TMPDIR/rsh.out.$$ > /dev/null 2>&1
	if [[ $? != 0 ]]; then
		echo "$Tname: run $SRV_TMPDIR/sharemnt.shtab \c"
		echo "-$opt 1 in $SERVER failed"
		cat $STF_TMPDIR/rsh.out.$$
		cleanup $STF_FAIL
	fi
	# make sure server is not alive
	condition="ping $SERVER | grep \"^no answer\" >/dev/null 2>&1"
	wait_now 60 "$condition"
	if [[ $? != 0 ]]; then
		echo "$Tname: run $SRV_TMPDIR/sharemnt.shtab \c"
		echo "-$opt 1 in $SERVER failed,\n"
		echo "$SERVER is still alive after 60 seconds"
		cleanup $STF_FAIL
	fi
	# check the consistence of /etc/dfs/sharetab.
	condition="RSH root $SERVER ls > /dev/null"
	wait_now 1200 "$condition" 60
	RSH root "$SERVER" \
		"export SHAREMNT_DEBUG=$SRVDEBUG; \
		$SRV_TMPDIR/sharemnt.shtab -r 2" \
		> $STF_TMPDIR/rsh.out.$$ 2>&1
	rc=$?
	;;
*)
	echo $Usage
	exit 2
	;;
esac

[[ :$SRVDEBUG: == *:all:* ]] && cat $STF_TMPDIR/rsh.out.$$

grep "Done" $STF_TMPDIR/rsh.out.$$ > /dev/null 2>&1
if [[ $? != 0 || $rc != 0 ]]; then
	echo "$Tname: run $SRV_TMPDIR/sharemnt.shtab \c"
	echo "-$opt in $SERVER failed"
	cat $STF_TMPDIR/rsh.out.$$
	cleanup $STF_FAIL
fi

echo "$Tname: testing complete - Result PASS"
cleanup $STF_PASS
