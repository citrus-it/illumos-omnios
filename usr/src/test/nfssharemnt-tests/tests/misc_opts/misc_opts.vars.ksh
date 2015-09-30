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
# Copyright 2008 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#

# The share options array for test combination generation
# Note: There should be NO conflict options and
#       sec= option must go before rw,ro
#
#SHROPTS_LIST="sec=sys sec=none sec=none:sys rw anon=0"
# There can't have two sec= options, due to a panic bug
SHROPTS_LIST="sec=sys ro anon=0 anon=98765 public"
if [[ -n $SHROPTS ]]; then
	ret=0
	for opt in $SHROPTS; do
		echo $SHROPTS_LIST | grep -w $opt > /dev/null 2>&1
		if (( $? != 0 )); then
			ret=1
			break
		fi
	done
	(( $ret != 0 )) && unset SHROPTS
else
	SHROPTS=$SHROPTS_LIST
fi

# The mount options array for test combination generation
MNTOPTS_LIST="rw sec=sys ro hard proto=tcp proto=udp proto=rdma public"
if [[ -n $MNTOPTS ]]; then
	ret=0
	for opt in $MNTOPTS; do
		echo $MNTOPTS_LIST | grep -w $opt > /dev/null 2>&1
		if (( $? != 0 )); then
			ret=1
			break
		fi
	done
	(( $ret != 0 )) && unset MNTOPTS
else
	MNTOPTS=$MNTOPTS_LIST
fi

# The NFS version options array for test combination generation
#
VEROPTS_LIST="default vers=4 vers=3 vers=2"
if [[ -n $VEROPTS ]]; then
	ret=0
	for opt in $VEROPTS; do
		echo $VEROPTS_LIST | grep -w $opt > /dev/null 2>&1
		if (( $? != 0 )); then
			ret=1
			break
		fi
	done
	(( $ret != 0 )) && unset VEROPTS
else
	VEROPTS=$VEROPTS_LIST
fi

STF_VARIABLES="SHROPTS_LIST MNTOPTS_LIST VEROPTS_LIST SHROPTS MNTOPTS VEROPTS"
