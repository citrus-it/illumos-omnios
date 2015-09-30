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
# Copyright 2007 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#

NAME=$(basename $0)
CDIR=$(dirname $0)

. $CDIR/${CDIR##*/}.vars

[[ :${SHAREMNT_DEBUG}: = *:${NAME}:*  \
	|| :${SHAREMNT_DEBUG}: = *:all:* ]] && set -x

for lo in $Log_opts
do
	so="rw,$lo"
	Mount_opt="rw"
	tag=$(echo $lo | awk -F\= '{print $2}')
	[[ $tag == "" ]] && tag=global
	
	Versions="$VEROPTS"
	for v in $Versions; do
	    mo=${Mount_opt},${v}
	    Tname=NFSLOGD_${lo}_${v}
	    echo "adding $Tname test"
	    stf_addassert -u root -t $Tname -c runtests "$Tname $tag $so $mo"
	done
done

exit 0
