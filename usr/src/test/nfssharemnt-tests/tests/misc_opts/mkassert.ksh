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
CDIR=$(dirname $0)

. $CDIR/${CDIR##*/}.vars
. $STF_SUITE/include/sharemnt.kshlib
. $STC_GENUTILS/include/libsmf.shlib
. $STC_GENUTILS/include/nfs-tx.kshlib

export STC_GENUTILS_DEBUG=$STC_GENUTILS_DEBUG:$SHAREMNT_DEBUG
[[ :$SHAREMNT_DEBUG: == *:$NAME:* \
	|| :$SHAREMNT_DEBUG: == *:all:* ]] && set -x

# check again if proto=rdma tests should be generated
echo "$TESTRDMA" | grep -i no > /dev/null 2>&1
(( $? != 0 )) && . $STF_CONFIG/stf_config.suite

typeset -i so_flg=0
Share_opts=$(gen_opt_list "$SHROPTS")
Mount_opts=$(gen_opt_list "$MNTOPTS")
Versions="$VEROPTS"

# The "nosub" option
so="nosub"
for mo in $Versions; do
	[[ $mo == "default" ]] && mo=""
	Tname=SH${so}_MNT${mo}
	echo "adding $Tname test"
	stf_addassert -u root -t $Tname -c nosub_test "$Tname $so $mo"
done

tag="MNT_INTR"
Mount_opts="intr nointr hard,intr hard,nointr"
for mo in $Mount_opts; do
	for v in $Versions; do
		[[ $v == "default" ]] && mo2=${mo} || mo2=${mo},${v}
		Tname=${tag}${mo2}
		echo "adding $Tname test"
		stf_addassert -u root -t $Tname -c shrmnt_optchk "$Tname $mo2"
	done
done

tag="SH_SUID"
Share_opts="default nosuid"
for so in $Share_opts; do
	for mo in $Versions; do
		[[ $so == "default" ]] && so=""
		[[ $mo == "default" ]] && mo=""
		Tname=${tag}${so}_MNT${mo}
		echo "adding $Tname test"
		stf_addassert -u root -t $Tname -c shrmnt_optchk \
			"$Tname $so $mo"
	done
done

# The rsize and wsize
mntopts="rsize=0 wsize=0 rsize=1 wsize=8195 rsize=32767 wsize=32769 \
	rsize=1048577 wsize=1048575 proto=tcp proto=udp"
[[ $TESTRDMA == "yes" ]] && mntopts="$mntopts proto=rdma"
Mount_opts=$(gen_opt_list "$mntopts")
for mo in $Mount_opts "default"; do
	[[ $mo == @(*rsize=*rsize=*|*wsize=*wsize=*) ]] && continue
	[[ $mo == @(*proto=*proto=*|*proto=*proto=*proto=*) ]] && continue
	for v in $Versions; do
		[[ $mo == "default" ]] && mo=""
		[[ $v == "default" ]] && mo2=${mo} || mo2=${mo},${v}
		mo2=${mo2#,}
		[[ $mo2 == *proto=udp*vers=4 ]] && continue
		if [[ $mo2 == @(*rsize=0*|*wsize=0*) ]]; then
			tag="NEG_MNT"
			Tname=${tag}${mo2}
			echo "adding $Tname test"
			stf_addassert -u root -t $Tname -c neg_test \
				"$Tname $mo2"
		else
			tag="MNT_SIZE"
			Tname=${tag}${mo2}
			echo "adding $Tname test"
			stf_addassert -u root -t $Tname -c shrmnt_optchk \
				"$Tname $mo2"
		fi
	done
done

tag="MNT_QUOTA"
Mount_opts="default quota noquota"
for mo in $Mount_opts; do
	for v in $Versions; do
		[[ $mo == "default" ]] && mo=""
		[[ $v == "default" ]] && mo2=${mo} || mo2=${mo},${v}
		mo2=${mo2#,}
		Tname=${tag}${mo2}
		echo "adding $Tname test"
		stf_addassert -u root -t $Tname -c shrmnt_optchk "$Tname $mo2"
	done
done

exit 0
