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
# ASSERTION: depends_036
# DESCRIPTION:
#  Dependencies can be added to an offline service.
#
# end __stf_assertion__
#

. ${STF_TOOLS}/include/stf.kshlib
. ${STF_SUITE}/include/gltest.kshlib
. ${STF_SUITE}/include/svc.startd_config.kshlib
. ${STF_SUITE}/tests/svc.startd/include/svc.startd_common.kshlib

typeset service_setup=0
function cleanup {
	rm -f $svccfg_errfile
	common_cleanup
	rm -f $service_state1 $service_state2 $service_state3
	return $?
}

trap cleanup 0 1 2 15

readonly ME=$(whence -p ${0})
readonly MYLOC=$(dirname ${ME})

DATA=$MYLOC

readonly registration_template=$DATA/service_036.xml

extract_assertion_info $ME

# make sure that the svc.startd is running
verify_daemon
if [ $? -ne 0 ]; then
	print -- "--DIAG: $assertion: svc.startd is not executing. Cannot "
	print -- "  continue"
	exit $STF_UNRESOLVED
fi

# Make sure the environment is clean - the test service isn't running
print -- "--INFO: Cleanup any old svc:/$test_service state"
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
	print -- "--DIAG: $assertion: Unable to import svc:/$test_service"
        print -- "  error messages from svccfg: "
        print -- "  \"$(cat $svccfg_errfile)\""
	exit $STF_UNRESOLVED
fi
service_setup=1

print -- "--INFO: Waiting for $test_FMRI2 to come online"
service_wait_state $test_FMRI2 online
if [ $? -ne 0 ]; then
	print -- "--DIAG: $assertion: Service $test_FMRI2 did not come online
	Current state: `svcprop -p restarter/state $test_FMRI2`"
	exit $STF_FAIL
fi

print -- "--INFO: Wait for $test_FMRI3 to go to offline"
service_wait_state $test_FMRI3 offline
if [ $? -ne 0 ]; then
	print -- "--DIAG: $assertion: Service $test_FMRI3 did not go offline
	Current state: `svcprop -p restarter/state $test_FMRI3`"
	exit $STF_FAIL
fi

print -- "--INFO: Add dependency from $test_FMRI3 -> $test_FMRI2"
service_dependency_elt_add $test_FMRI3 cdepa svc:/$test_FMRI2

print -- "--INFO: Dump out new service manifest"
svccfg export ${test_FMRI3%:*}

print -- "--INFO: Refresh $test_FMRI3"
svcadm refresh $test_FMRI3
if [ $? -ne 0 ]; then
	print -- "--DIAG: $assertion: Could not refresh $test_FMRI3"
	exit $STF_FAIL
fi

print -- "--INFO: Verify service $test_FMRI3 is online"
service_wait_state $test_FMRI3 online
if [ $? -ne 0 ]; then
	print -- "--DIAG: $assertion: Service $test_FMRI3 isn't online
	Current state: `svcprop -p restarter/state $test_FMRI3`"
	exit $STF_FAIL
fi

print -- "--INFO: Disable the service $test_FMRI2"
svcadm disable $test_FMRI2
if [ $? -ne 0 ]; then
	print -- "--DIAG: could not disable $test_FMRI2"
	exit $STF_FAIL
fi

print -- "--INFO: Verify that service $test_FMRI3 goes offline"
service_wait_state $test_FMRI3 offline
if [ $? -ne 0 ]; then
	print -- "--DIAG: service $test_FMRI3 didn't go offline
	Current state: `svcprop -p restarter/state $test_FMRI3`"
	exit $STF_FAIL
fi

print -- "--INFO: Cleaning up service"
cleanup

exit $STF_PASS
