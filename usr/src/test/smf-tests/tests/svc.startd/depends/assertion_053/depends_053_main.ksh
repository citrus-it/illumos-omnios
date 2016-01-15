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
# DESCRIPTION:
#  A service with multiple dependencies in a "require_all" grouping
#  property. All of the dependencies are satisfied.
#  svc.startd will transition the service into the online state.
#  Services: a, b, c; c depends on a and b
#  All services are instances of different services

. ${STF_TOOLS}/include/stf.kshlib
. ${STF_SUITE}/include/gltest.kshlib
. ${STF_SUITE}/include/svc.startd_config.kshlib
. ${STF_SUITE}/tests/svc.startd/include/svc.startd_common.kshlib

typeset service_setup=0
function cleanup {
	again=$service_setup

	test_service=$test_service1
	common_cleanup

	service_setup=$again
	test_service=$test_service2
	common_cleanup

	service_setup=$again
	test_service=$test_service3
	common_cleanup

	rm -f $service_state1 $service_state2 $service_state3
}

trap cleanup 0 1 2 15

readonly ME=$(whence -p ${0})
readonly MYLOC=$(dirname ${ME})

DATA=$MYLOC

# make sure that the svc.startd is running
verify_daemon
if [ $? -ne 0 ]; then
	print -- "--DIAG: $assertion: svc.startd is not executing. Cannot "
	print -- "  continue"
	exit $STF_UNRESOLVED
fi

# Make sure the environment is clean - the test service isn't running
print -- "--INFO: Cleanup any old $test_FMRI1, $test_FMRI2, $test_FMRI3 state"
service_cleanup $test_service1
if [ $? -ne 0 ]; then
	print -- "--DIAG: $assertion: cleanup of a previous instance failed"
	exit $STF_UNRESOLVED
fi
service_cleanup $test_service2
if [ $? -ne 0 ]; then
	print -- "--DIAG: $assertion: cleanup of a previous instance failed"
	exit $STF_UNRESOLVED
fi
service_cleanup $test_service3
if [ $? -ne 0 ]; then
	print -- "--DIAG: $assertion: cleanup of a previous instance failed"
	exit $STF_UNRESOLVED
fi

print -- "--INFO: generating manifest for importation into repository"
manifest_generate $registration_template \
	TEST_SERVICE1=$test_service1 \
	TEST_SERVICE2=$test_service2 \
	TEST_SERVICE3=$test_service3 \
	TEST_INSTANCE1=$test_instance1 \
	TEST_INSTANCE2=$test_instance2 \
	TEST_INSTANCE3=$test_instance3 \
	SERVICE_APP=$service_app \
	LOGFILE=$service_log \
	STATEFILE1=$service_state1 \
	STATEFILE2=$service_state2 \
	STATEFILE3=$service_state3 \
	> $registration_file

print -- "--INFO: Importing service into repository"
manifest_purgemd5 $registration_file
svccfg -v import $registration_file >$svccfg_errfile 2>&1

if [ $? -ne 0 ]; then
	print -- "--DIAG: $assertion: Unable to import the services $test_FMRI1"
        print -- "  $test_FMRI2 and $test_FMRI3 error messages from svccfg: "
        print -- "  \"$(cat $svccfg_errfile)\""
	exit $STF_UNRESOLVED
fi
service_setup=1

print -- "--INFO: Wait for $test_FMRI3 to come online - it should not"
service_wait_state $test_FMRI3 online
if [ $? -eq 0 ]; then
	print -- "--DIAG: $assertion: Service $test_FMRI3 came online"
	exit $STF_FAIL
fi

print -- "--INFO: Enabling service $test_FMRI1"
svcadm enable $test_FMRI1
if [ $? -ne 0 ]; then
        print -- "--DIAG: $assertion: Service $test_FMRI1 did not enable"
        exit $STF_FAIL
fi

print -- "--INFO: Waiting for $test_FMRI1 to come online"
service_wait_state $test_FMRI1 online
if [ $? -ne 0 ]; then
	print -- "--DIAG: $assertion: Service $test_FMRI1 did not come online"
	exit $STF_FAIL
fi

print -- "--INFO: Wait for $test_FMRI3 to come online - it should not"
service_wait_state $test_FMRI3 online
if [ $? -eq 0 ]; then
	print -- "--DIAG: $assertion: Service $test_FMRI3 came online"
	exit $STF_FAIL
fi

print -- "--INFO: Enabling service $test_FMRI2"
svcadm enable $test_FMRI2
if [ $? -ne 0 ]; then
        print -- "--DIAG: $assertion: Service $test_FMRI2 did not enable"
        exit $STF_FAIL
fi

print -- "--INFO: Waiting for $test_FMRI2 to come online"
service_wait_state $test_FMRI2 online
if [ $? -ne 0 ]; then
	print -- "--DIAG: $assertion: Service $test_FMRI2 did not come online"
	exit $STF_FAIL
fi

print -- "--INFO: Waiting for $test_FMRI3 to come online"
service_wait_state $test_FMRI3 online
if [ $? -ne 0 ]; then
	print -- "--DIAG: $assertion: Service $test_FMRI3 did not come online"
	exit $STF_FAIL
fi

print -- "--INFO: Cleaning up service"
cleanup

exit $STF_PASS
