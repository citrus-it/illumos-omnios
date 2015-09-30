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

# This setup file do nothing related to setup; it only print out
# test environment information to journal file, so cleanup isn't needed.

. $STC_NFSUTILS/include/nfs-util.kshlib

echo "\nCurrent test has set the following environment info:"
echo "\tlocal host:\t`hostname` \
	\n\tSERVER:\t\t$SERVER \
	\n\tNFSSHRDIR:\t$NFSSHRDIR \
	\n\tSHRDIR:\t\t$SHRDIR \
	\n\tNFSMNTDIR:\t$NFSMNTDIR \
	\n\tMNTOPT:\t\t$MNTOPT \
	\n\tZFSPOOL:\t$ZFSPOOL \
	\n\tTESTRDMA:\t$TESTRDMA"

# print test system information
echo ""
print_system_info
echo ""
print_system_info $SERVER
echo ""

exit 0
