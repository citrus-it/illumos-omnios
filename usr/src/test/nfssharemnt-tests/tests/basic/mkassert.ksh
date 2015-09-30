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

for so in $Share_opts; do
	# For possible assertions, skip invalid options
	# e.g.: 'rw,sec=' or 'ro,sec=' (sec= must go first)
	echo "$so" | egrep "rw,sec=|ro,sec=" && continue
	[[ $so == *anon=*anon=* ]] && continue

	for mo in $Mount_opts; do
		# For possible assertions, skip the conflict options
		# e.g. 2 of "sec="
		ck=$(echo "$mo" | \
			nawk -F\, '{k=0; for (i=1; i<=NF; i++) \
			{if ($i ~ /sec=/) k++}; print k}')
		(( $ck > 1 )) && continue
		# or "rw" & "ro" conflicts
		ck=$(echo "$mo" | \
			nawk -F\, '{k=0; for (i=1; i<=NF; i++) \
			{if (($i ~ /rw/) || ($i ~ /ro/)) k++;} print k}')
		(( $ck > 1 )) && continue

		# skip proto=rdma tests if TESTRDMA=no
		if [[ $mo == *proto=rdma* ]]; then
			echo "$TESTRDMA" | grep -i no > /dev/null 2>&1
			(( $? == 0 )) && continue
		fi

		for v in $Versions; do
			[[ $v == "default" ]] && mo2=${mo} || mo2=${mo},${v}
			if [[ $mo2 == *proto=udp*vers=4 ]]; then
			    if (( $so_flg == 0 )); then
				so_flg=1
				tag="NEG_MNT"
				Tname=${tag}${mo2}
				echo "adding $Tname test"
				stf_addassert -u root -t $Tname -c neg_test \
					"$Tname $mo2"
			    fi
			    continue
			fi
			Tname=SH${so}_MNT${mo2}
			echo "adding $Tname test"
			stf_addassert -u root -t $Tname -c runtests \
				"$Tname $so $mo2"
		done
	done
done

# The followings are the negative cases
tag="NEG_MNT"
Mount_opts="sec=* sec=JUNK"
for mo in $Mount_opts; do
	Tname=${tag}${mo}
	echo "adding $Tname test"
	stf_addassert -u root -t $Tname -c neg_test "$Tname $mo"
done

tag="NEG_SH"
Share_opts="sec=* sec=JUNK rw,ro ro,rw"
for so in $Share_opts; do
	Tname=${tag}${so}
	echo "adding $Tname test"
	stf_addassert -u root -t $Tname -c neg_test "$Tname $so"
done
Share_opts="$CLIENT_S $CLIENT_S:$SERVER_S $SERVER_S:$CLIENT_S"
for i in $Share_opts; do
	for j in $Share_opts; do
		for so in "ro=$i,rw=$j" "rw=$i,ro=$j"; do
			Tname=$(echo ${tag}${so} | \
				sed -e "s%$CLIENT_S%CLNT%g" \
				    -e "s%$SERVER_S%SRV%g")
			echo "adding $Tname test"
			stf_addassert -u root -t $Tname -c neg_test "$Tname $so"
		done
	done
done

exit 0
