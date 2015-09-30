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

[[ :$SHAREMNT_DEBUG: == *:${NAME}:* \
	|| :${SHAREMNT_DEBUG}: == *:all:* ]] && set -x

. $STF_SUITE/include/sharemnt.kshlib

# Cleanup the entries in auto_indirect map for possible next run
grep -v "^SM_" $STF_TMPDIR/auto_indirect.shmnt > $STF_TMPDIR/ind_tmp.$$
mv $STF_TMPDIR/ind_tmp.$$ $STF_TMPDIR/auto_indirect.shmnt
if (( $? != 0 )); then
	echo "$NAME: Failed to cleanup the entries from the"
	echo "\tindirect map <$STF_TMPDIR/auto_indirect.shmnt>"
	echo "\tRe-run this subdir may have unexpected automount results."
	exit $STF_WARNING
fi

automount

# umount autofs by force if it is still mounted
for mnt in $(nfsstat -m | grep "^$AUTOIND/SM_" \
	| awk '{print $1}'); do
	umount -f $mnt > $STF_TMPDIR/umnt.out.$$ 2>&1
	if (( $? != 0 )); then
		echo "$NAME: Warning - umount $mnt failed -"
		cat $STF_TMPDIR/umnt.out.$$
		echo "\t... please clean it up manually."
	fi
done

cleanup $STF_PASS
