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

. $STC_NFSUTILS/include/nfs-util.kshlib

# Turn on debug info, if requested
export STC_GENUTILS_DEBUG=$STC_GENUTILS_DEBUG:$SHAREMNT_DEBUG
[[ :$SHAREMNT_DEBUG: == *:$NAME:* \
	|| :$SHAREMNT_DEBUG: == *:all:* ]] && set -x

function cleanup {		# cleanup and exit
	rm -f $STF_TMPDIR/sharemnt.misc_opts $STF_TMPDIR/*.out.$$
	exit $1
}

#
# Now setup the server..
#
# Firstly add environment variables to srv_setup script and
# create a new script called sharemnt.misc_opts which will be
# copied over to the server:
sed -e "s%STF_TMPDIR_from_client%$SRV_TMPDIR%" \
    -e "s%SHAREMNT_DEBUG_from_client%$SHAREMNT_DEBUG%" \
	$DIR/srv_setup > $STF_TMPDIR/sharemnt.misc_opts \
	2> $STF_TMPDIR/sed.out.$$
if (( $? != 0 )); then
	echo "$NAME: failed to create [sharemnt.others] file:"
	cat $STF_TMPDIR/sed.out.$$
	echo "PATH is $PATH"
	cleanup $STF_UNINITIATED
fi

scp $STF_TMPDIR/sharemnt.misc_opts root@$SERVER:$SRV_TMPDIR \
	> $STF_TMPDIR/scp.out.$$ 2>&1
if (( $? != 0 )); then
	echo "$NAME: failed to copy \
		<$STF_TMPDIR/sharemnt.misc_opts> to $SERVER:"
	cat $STF_TMPDIR/scp.out.$$
	cleanup $STF_FAIL
fi

# ..finally execute the script on the server.
SRVDEBUG=$STC_GENUTILS_DEBUG:$SHAREMNT_DEBUG
[[ :$SRVDEBUG: == *:RSH:* ]] && SRVDEBUG=all
RSH root $SERVER \
	"export SHAREMNT_DEBUG=$SRVDEBUG; \
	 F=$SRV_TMPDIR/sharemnt.misc_opts; \
	 chmod 0555 \$F && \$F -s" \
	> $STF_TMPDIR/rsh.out.$$ 2>&1
rc=$?
[[ :$SRVDEBUG: == *:all:* ]] && cat $STF_TMPDIR/rsh.out.$$
grep "Done" $STF_TMPDIR/rsh.out.$$ > /dev/null 2>&1
if (( $? != 0 || $rc != 0 )); then
	echo "$NAME: run <$SRV_TMPDIR/sharemnt.misc_opts -s> \
		in $SERVER failed:"
	cat $STF_TMPDIR/rsh.out.$$
	cleanup $STF_FAIL
fi

cleanup $STF_PASS
