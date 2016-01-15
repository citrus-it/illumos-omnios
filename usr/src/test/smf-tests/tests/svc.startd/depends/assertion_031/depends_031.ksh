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

#
# start __stf_assertion__
#
# ASSERTION: depends_031
# DESCRIPTION:
#  If a service, service_A, has a dependency on another which has a
#  dependency on another service and so on and one of the service has a
#  dependency on service_A then the last service registered will go to
#  the maintenance state while all other services will reach the offline
#  state.
#  (multi-link circular dependency)
#
# end __stf_assertion__
#

. ${STF_TOOLS}/include/stf.kshlib
. ${STF_SUITE}/include/gltest.kshlib
. ${STF_SUITE}/include/svc.startd_config.kshlib
. ${STF_SUITE}/tests/svc.startd/include/svc.startd_common.kshlib

typeset service_setup=0
function cleanup {
	common_cleanup
	rm -f $service_state1 $service_state2 $service_state3 $service_state4
}

trap cleanup 0 1 2 15

readonly ME=$(whence -p ${0})
readonly MYLOC=$(dirname ${ME})

DATA=$MYLOC

readonly registration_template=$DATA/service_031.xml

extract_assertion_info $ME

# make sure that the svc.startd is running
verify_daemon
if [ $? -ne 0 ]; then
	print -- "--DIAG: $assertion: svc.startd is not executing. Cannot "
	print -- "  continue"
	exit $STF_UNRESOLVED
fi

# Make sure the environment is clean - the test service isn't running
print -- "--INFO: Cleanup any old $test_FMRI1, $test_FMRI2, "
print -- "  $test_FMRI3 and $test_FMRI4 state"
service_cleanup $test_service
if [ $? -ne 0 ]; then
	print -- "--DIAG: $assertion: cleanup of a previous instance failed"
	exit $STF_UNRESOLVED
fi

print -- "--INFO: generating manifest for importation into repository"
manifest_generate $registration_template \
	TEST_SERVICE=$test_service \
	TEST_INSTANCE1=$test_instance1 \
	TEST_INSTANCE2=$test_instance2 \
	TEST_INSTANCE3=$test_instance3 \
	TEST_INSTANCE4=$test_instance4 \
	SERVICE_APP=$service_app \
	LOGFILE=$service_log \
	STATEFILE1=$service_state1 \
	STATEFILE2=$service_state2 \
	STATEFILE3=$service_state3 \
	STATEFILE4=$service_state4 \
	> $registration_file

print -- "--INFO: Importing service into repository"
manifest_purgemd5 $registration_file
svccfg -v import $registration_file >$svccfg_errfile 2>&1

if [ $? -ne 0 ]; then
	print -- "--DIAG: $assertion: Unable to import the services under"
        print -- "  svc:/$test_service error messages from svccfg: "
        print -- "  \"$(cat $svccfg_errfile)\""
	exit $STF_UNRESOLVED
fi
service_setup=1


# one of the services wil reach maintenance, all the others will be offline
inmaint=0
inoffline=0

print -- "--INFO: check for $test_FMRI4 to go offline"
service_wait_state $test_FMRI4 offline
if [ $? -ne 0 ]; then
	state=$(svcs -H -o STATE $test_FMRI4)
	if [ "$state" != maintenance ]; then
		print -- "--DIAG: $assertion: Service $test_FMRI4 didn't go to "
		print -- "  maintenance. It is in \"$state\" state."
		exit $STF_FAIL
	else
		inmaint=$((inmaint + 1))
	fi
else
	inoffline=$((inoffline + 1))
fi

print -- "--INFO: Verifying that $test_FMRI1 is in the offline state"
service_wait_state $test_FMRI1 offline
if [ $? -ne 0 ]; then
	state=$(svcs -H -o STATE $test_FMRI1)
	if [ "$state" != maintenance ]; then
        	print -- "--DIAG: $assertion: Service $test_FMRI1 did not go to the"
		print -- "  maintenance state. It is in the \"$state\" state."
		exit $STF_FAIL
	else
		inmaint=$((inmaint + 1))
	fi
else
	inoffline=$((inoffline + 1))
fi

print -- "--INFO: Verifying that $test_FMRI2 is in the offline state"
service_wait_state $test_FMRI2 offline
if [ $? -ne 0 ]; then
	state=$(svcs -H -o STATE $test_FMRI2)
	if [ "$state" != maintenance ]; then
        	print -- "--DIAG: $assertion: Service $test_FMRI2 did not go to the"
		print -- "  maintenance state. It is in the \"$state\" state."
	        exit $STF_FAIL
	else
		inmaint=$((inmaint + 1))
	fi
else
	inoffline=$((inoffline + 1))
fi

print -- "--INFO: Verifying that $test_FMRI3 is in the offline state"
service_wait_state $test_FMRI3 offline
if [ $? -ne 0 ]; then
	state=$(svcs -H -o STATE $test_FMRI3)
	if [ "$state" != maintenance ]; then
        	print -- "--DIAG: $assertion: Service $test_FMRI3 did not go to the"
		print -- "  maintenance state. It is in the \"$state\" state."
		exit $STF_FAIL
	else
		inmaint=$((inmaint + 1))
	fi
else
	inoffline=$((inoffline + 1))
fi

if [ $inoffline -ne 3 -o $inmaint -ne 1 ]; then
	print -- "--DIAG: $assertion: $inoffline services are in the offline state"
	print -- "  and $inmaint services are in the maintenance state. This should"
	print -- "  be 3 offline, and 1 in maintenance."
	svcs -l $test_service
	exit $STF_FAIL
	
fi

print -- "--INFO: Cleaning up service"
cleanup

exit $STF_PASS
