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

TASK=$1

#
# Though we check in configure.ksh that the SERVER variable
# is defined and the machine is up, we recheck here to make
# sure that nothing has happened since the stf_configure
# script ran which might cause the tests to fail.
#
if [[ $TASK == "CONFIGURE" || $TASK == "EXECUTE" ]]; then
	for libfile in $STF_TOOLS/include/stf.kshlib \
	    $STC_NFSUTILS/include/nfs-util.kshlib \
	    $STC_GENUTILS/include/nfs-tx.kshlib \
	    $STC_GENUTILS/include/nfs-smf.kshlib \
	    $STC_GENUTILS/include/libsmf.shlib; do
		ce_file_exist $libfile
		ret=$?
		(( $ret != 0 )) && echo "$libfile not found on $(hostname), exiting."
		save_results $ret
	done

	. $STC_GENUTILS/include/nfs-tx.kshlib
	. $STC_NFSUTILS/include/nfs-util.kshlib

	check_tx_zonepath
	check_system SERVER
	if [[ -z $NFSMNTDIR ]]; then
		echo "NFSMNTDIR is not set, exiting."
		exit 1
	fi
fi
