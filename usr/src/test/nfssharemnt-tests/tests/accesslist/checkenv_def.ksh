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

TASK=$1

#
# Need to make sure the SHROPTS, MNTOPTS and VEROPTS are set correctly
#
if [[ $TASK == "CONFIGURE" ]]; then
	ret=0
	if [[ -z $SHROPTS ]]; then
		echo "SHROPTS is not set correctly, please make sure your share"
		echo "\toptions are selected from [$SHROPTS_LIST] list only!"
		ret=$((ret + 1))
	fi

	if [[ -z $MNTOPTS ]]; then
		echo "MNTOPTS is not set correctly, please make sure your mount"
		echo "\toptions are selected from [$MNTOPTS_LIST] list only!"
		ret=$((ret + 1))
	fi

	if [[ -z $VEROPTS ]]; then
		echo "VEROPTS is not set correctly, please make sure your vers"
		echo "\toptions are selected from [$VEROPTS_LIST] list only!"
		ret=$((ret + 1))
	fi

	(( $ret != 0 )) && exit 1
fi
exit 0
