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
# This subdir tests share a file; it should be cleaned up for other tests.
#

DIR=$(dirname $0)
NAME=$(basename $0)

. $STF_SUITE/include/sharemnt.kshlib

[[ :$SHAREMNT_DEBUG: == *:${NAME}:* \
	|| :${SHAREMNT_DEBUG}: == *:all:* ]] && set -x

[[ :$SHAREMNT_DEBUG: == *:RSH:* ]] && SRVDEBUG=all

# unshare the exported file, just in case, for other testing
RSH root $SERVER \
	"export SHAREMNT_DEBUG=$SRVDEBUG; \
	$SRV_TMPDIR/srv_setup -u $SHRDIR" > $STF_TMPDIR/rsh.out.$$ 2>&1
if (( $? != 0 )); then
	echo "$NAME: failed to unshare <$SHRDIR>"
	cat $STF_TMPDIR/rsh.out.$$
	echo "\t Tests that follow in execution may fail"
	cleanup $STF_FAIL
fi

[[ :$SRVDEBUG: == *:all:* ]] && cat $STF_TMPDIR/rsh.out.$$

cleanup $STF_PASS
