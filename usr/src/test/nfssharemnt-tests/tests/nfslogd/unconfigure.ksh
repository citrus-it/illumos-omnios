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

# Turn on debug info, if requested
export STC_GENUTILS_DEBUG=$STC_GENUTILS_DEBUG:$SHAREMNT_DEBUG
[[ :$SHAREMNT_DEBUG: == *:${NAME}:* \
	|| :${SHAREMNT_DEBUG}: == *:all:* ]] && set -x

function cleanup {		# cleanup and exit
	rm -f $STF_TMPDIR/*.out.$$
	exit ${1}
}

# Now cleanup the server..
SRVDEBUG=$SHAREMNT_DEBUG
[[ :$SHAREMNT_DEBUG: == *:RSH:* ]] && SRVDEBUG=all
RSH root $SERVER \
	"export SHAREMNT_DEBUG=$SRVDEBUG; \
	F=$SRV_TMPDIR/sharemnt.nfslogd; \
	if [[ -f \$F ]]; then \$F -c; else echo NeedlessToDo; fi" \
	> $STF_TMPDIR/rsh.out.$$ 2>&1
rc=$?
print_debug $STF_TMPDIR/rsh.out.$$

egrep "^NeedlessToDo$" $STF_TMPDIR/rsh.out.$$ > /dev/null 2>&1
if (( $? == 0 && $rc == 0 )); then
	echo "$NAME: script<$SRV_TMPDIR/sharemnt.nfslogd> does not exist"
	echo "\t it seems we don't need to cleanup on SERVER<$SERVER>"
	cleanup $STF_PASS
fi

grep "Done" $STF_TMPDIR/rsh.out.$$ > /dev/null 2>&1
if (( $? != 0 || $rc != 0 )); then
	echo "$NAME: run cleanup script on SERVER<$SERVER> failed :"
	cat $STF_TMPDIR/rsh.out.$$
	echo "\t please cleanup the SERVER manually"
	cleanup $STF_FAIL
fi

cleanup $STF_PASS
