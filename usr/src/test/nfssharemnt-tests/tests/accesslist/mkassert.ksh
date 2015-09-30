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
Mount_opts="$MNTOPTS"
Versions="$VEROPTS"

acc_host=$CLIENT_S
oth_host=$SERVER_S
acc_netgroup=""
oth_netgroup="NotExistGroup"
acc_domain=""
oth_domain=".NotExistDomain"

# share_nfs doesn't support using IPv6 address
# hard-coded with IPv4 address for the access network
[[ -z $acc_host ]] && acc_host=$(hostname)
hostip=$(is_IPv6 $acc_host)
if (( $? == 0 )); then
    acc_network="@$(echo $hostip | awk -F\. '{print $1"."$2"."$3}')/24"
    oth_network="@$(echo $hostip | awk -F\. '{print $1"."$2"."($3+1)}')/24"
    acc_hostIP="@$hostip"
elif (( $? == 1 )); then
    # now IPv6 isn't supported in access list, we should set the
    # following variables with IPv6 address once 6481391 is fixed
    acc_network=""
    oth_network=""
    acc_hostIP=""
else
    echo "$NAME: $hostip"
    exit 1
fi

# create a list with each element includes the client test machine
acc_list="$acc_host $acc_netgroup $acc_domain $acc_network $acc_hostIP"

# create a list with each element excludes the client test machine
den_list=""
for i in $acc_list; do
    den_list="$den_list -$i"
done

# create a list with each element doesn't include the client test machine
oth_list="$oth_host $oth_netgroup $oth_domain $oth_network"


# test the client in "ro/rw/root" access list
for i in $acc_list "null"; do
    for mo in $Mount_opts; do
	for v in $Versions; do
	    [[ $v == "default" ]] && mo2=${mo} || mo2=${mo},${v}

	    for j in "ro" "rw"; do
		[[ $i == "null" ]] && so="$j" || so="$j=$i"

		# "share -o ro|rw[=access_foo][,root=access_foo]"
		for root_i in $acc_list; do
		    so2="$so,root=$root_i"
		    Tname=$(echo ACCESS_SH${so2}_MNT${mo2} | \
				sed "s%$acc_host%CLNT%g")
		    [[ -n $acc_network ]] && Tname=$(echo $Tname | \
				sed "s%$acc_network%CLNTIP%g")
		    [[ -n $acc_hostIP ]] && Tname=$(echo $Tname | \
				sed "s%$acc_hostIP%HOSTIP%g")
		    echo "adding $Tname test"
		    stf_addassert -u root -t $Tname -c access_test \
			"$Tname $so2 $mo2 $j root"
		done

		# "share -o ro|rw[=access_foo]
		#           [,root=not_access_foo|deny_access_foo]"
		for root_i in $den_list $oth_list "null"; do
		    [[ $root_i == "null" ]] && so2="$so" \
					    || so2="$so,root=$root_i"
		    Tname=$(echo ACCESS_SH${so2}_MNT${mo2} | \
			    sed -e "s%$acc_host%CLNT%g" \
				-e "s%$oth_host%SRV%g")
		    [[ -n $acc_network ]] && Tname=$(echo $Tname | \
				sed "s%$acc_network%CLNTIP%g")
		    [[ -n $oth_network ]] && Tname=$(echo $Tname | \
				sed "s%$oth_network%SRVIP%g")
		    [[ -n $acc_hostIP ]] && Tname=$(echo $Tname | \
				sed "s%$acc_hostIP%HOSTIP%g")
		    echo "adding $Tname test"
		    stf_addassert -u root -t $Tname -c access_test \
			"$Tname $so2 $mo2 $j noroot"
		done
	    done

	    if [[ $i != "null" ]]; then
		for j in "rw,ro" "ro,rw"; do
		    [[ $j == *"ro" ]] && expopt=ro || expopt=rw
		    so="$j=$i"

		    # "share -o <rw,ro>|<ro,rw>=access_foo[,root=access_foo]"
		    for root_i in $acc_list; do
			so1="$so,root=$root_i"
			Tname=$(echo ACCESS_SH${so1}_MNT${mo2} | \
				sed "s%$acc_host%CLNT%g")
			[[ -n $acc_network ]] && Tname=$(echo $Tname | \
				sed "s%$acc_network%CLNTIP%g")
			[[ -n $acc_hostIP ]] && Tname=$(echo $Tname | \
				sed "s%$acc_hostIP%HOSTIP%g")
			echo "adding $Tname test"
			stf_addassert -u root -t $Tname -c access_test \
			    "$Tname $so1 $mo2 $expopt root"
		    done

		    # "share -o <rw,ro>|<ro,rw>=access_foo
		    #           [,root=not_access_foo|deny_access_foo]"
		    for root_i in $den_list $oth_list "null"; do
			[[ $root_i == "null" ]] && so1="$so" \
						|| so1="$so,root=$root_i"
			Tname=$(echo ACCESS_SH${so1}_MNT${mo2} | \
				sed -e "s%$acc_host%CLNT%g" \
				    -e "s%$oth_host%SRV%g")
			[[ -n $acc_network ]] && Tname=$(echo $Tname | \
				sed "s%$acc_network%CLNTIP%g")
			[[ -n $oth_network ]] && Tname=$(echo $Tname | \
				sed "s%$oth_network%SRVIP%g")
			[[ -n $acc_hostIP ]] && Tname=$(echo $Tname | \
				sed "s%$acc_hostIP%HOSTIP%g")
			echo "adding $Tname test"
			stf_addassert -u root -t $Tname -c access_test \
			    "$Tname $so1 $mo2 $expopt noroot"
		    done
		done
	    fi
	done
    done
done

# test combination options
so=$(echo $oth_list | sed 's/ /:/g')
for i in $acc_list "null"; do
    for mo in $Mount_opts; do
	for v in $Versions; do
	    [[ $v == "default" ]] && mo2=${mo} || mo2=${mo},${v}

	    if [[ $i == "null" ]]; then
		so1=$(echo $den_list | sed 's/ /:/g')

		# "share -o ro|rw=not_access_foo:deny_access_foo"
		# "share -o ro=not_access_foo,rw=deny_access_foo"
		# "share -o rw=not_access_foo,ro=deny_access_foo"
		for so2 in "ro=$so:$so1" "rw=$so:$so1" \
		    "ro=$so,rw=$so1" "rw=$so,ro=$so1"; do
		    tag="NEG_ACCESS"
		    Tname=$(echo ${tag}_SH${so2}_MNT${mo2} | \
			sed -e "s%$acc_host%CLNT%g" \
			    -e "s%$oth_host%SRV%g")
		    [[ -n $acc_network ]] && Tname=$(echo $Tname | \
				sed "s%$acc_network%CLNTIP%g")
		    [[ -n $oth_network ]] && Tname=$(echo $Tname | \
				sed "s%$oth_network%SRVIP%g")
		    [[ -n $acc_hostIP ]] && Tname=$(echo $Tname | \
				sed "s%$acc_hostIP%HOSTIP%g")
		    echo "adding $Tname test"
		    stf_addassert -u root -t $Tname -c neg_access \
			"$Tname $mo2 $so2"
		done
	    else
		# "share -o ro|rw=not_access_foo:access_foo"
		for so2 in "ro=$so:$i" "rw=$so:$i"; do
		    expopt=$(echo $so2 | awk -F= '{print $1}')
		    Tname=$(echo ACCESS_SH${so2}_MNT${mo2} | \
			sed -e "s%$acc_host%CLNT%g" \
			    -e "s%$oth_host%SRV%g")
		    [[ -n $acc_network ]] && Tname=$(echo $Tname | \
				sed "s%$acc_network%CLNTIP%g")
		    [[ -n $oth_network ]] && Tname=$(echo $Tname | \
				sed "s%$oth_network%SRVIP%g")
		    [[ -n $acc_hostIP ]] && Tname=$(echo $Tname | \
				sed "s%$acc_hostIP%HOSTIP%g")
		    echo "adding $Tname test"
		    stf_addassert -u root -t $Tname -c access_test \
			"$Tname $so2 $mo2 $expopt noroot"
		done
	    fi
	done
    done
done

tag="NEG_ACCESS"
so="anon=-1"
for v in $Versions; do
	[[ $v == "default" ]] && mo=rw || mo=rw,${v}
	Tname=${tag}_SH${so}_MNT${mo}
	echo "adding $Tname test"
	stf_addassert -u root -t $Tname -c neg_access "$Tname $mo $so"
done


exit 0
