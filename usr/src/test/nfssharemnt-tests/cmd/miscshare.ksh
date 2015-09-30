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
STF_TMPDIR=${STF_TMPDIR:-"STF_TMPDIR_from_client"}
SHAREMNT_DEBUG=${SHAREMNT_DEBUG:-"SHAREMNT_DEBUG_from_client"}

. $STF_TMPDIR/srv_config.vars

# Include common STC utility functions
if [[ -s $STC_GENUTILS/include/nfs-util.kshlib ]]; then
	. $STC_GENUTILS/include/nfs-util.kshlib
else
	. $STF_TMPDIR/nfs-util.kshlib
fi

export STC_GENUTILS_DEBUG=$SHAREMNT_DEBUG
[[ :$SHAREMNT_DEBUG: == *:all:* ]] && set -x

function usage {
	typeset FNAME=usage
	[[ :$SHAREMNT_DEBUG: == *:$FNAME:* \
		|| :$SHAREMNT_DEBUG: == *:all:* ]] && set -x

	[[ -n $1 ]] && echo $1
	echo "Usage: $NAME <group> share <directory> [options]"
	echo "       $NAME <group> unshare <directory>"
	exit 1
}

if [[ $# != 3 && $# != 4 ]]; then
	usage
fi

share_group=$1
operate=$2
dir=$3
share_opts=${4:-"rw"}

sharemgr list | grep -w $share_group > /dev/null 2>&1
if (( $? != 0 )); then
	usage "$NAME: group<$share_group> is not existed"
fi

if [[ -z $dir ]]; then
	usage "$NAME: exported diretory name not found"
fi

ret=0
case $operate in
share)
	share_type=$(( RANDOM % 3 ))
	if (( share_type == 0 )); then
		share2cmd="sharemgr_share $share_group $dir $share_opts"
	elif (( share_type == 1 )); then
		share2cmd="zfs_share $dir $share_opts"
	else
		share2cmd="share -F nfs -o $share_opts $dir"
	fi

	eval "$share2cmd" > $STF_TMPDIR/$NAME.out.$$ 2>&1
	ret=$?
	if (( $ret != 0 )); then
		echo "$NAME: share $dir with $share_opts failed."
		echo "command: $share2cmd"
		cat $STF_TMPDIR/$NAME.out.$$
	fi
	;;

unshare)
	auto_unshare $dir $share_group > $STF_TMPDIR/$NAME.out.$$ 2>&1
	ret=$?
	if (( $ret != 0 )); then
		echo "$NAME: unshare $dir failed."
		cat $STF_TMPDIR/$NAME.out.$$
	fi
	;;

*)
	usage
	;;
esac

[[ :$SHAREMNT_DEBUG: == *:all:* ]] && cat $STF_TMPDIR/$NAME.out.$$ 1>&2

rm -f $STF_TMPDIR/$NAME.out.$$
exit $ret
