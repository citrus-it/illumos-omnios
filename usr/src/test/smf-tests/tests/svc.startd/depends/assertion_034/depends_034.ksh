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
# ASSERTION: depends_034
# DESCRIPTION:
#  A service may specify both file and service dependencies in 
#  it entities property.
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
	rm -f $service_state1 $dependency_file
}

trap cleanup 0 1 2 15

readonly ME=$(whence -p ${0})
readonly MYLOC=$(dirname ${ME})

DATA=$MYLOC

readonly registration_template=$DATA/service_034.xml

extract_assertion_info $ME

# make sure that the svc.startd is running
verify_daemon
if [ $? -ne 0 ]; then
	print -- "--DIAG: $assertion: svc.startd is not executing. Cannot "
	print -- "  continue"
	exit $STF_UNRESOLVED
fi

# Make sure the environment is clean - the test service isn't running
print -- "--INFO: Cleanup any old $test_FMRI1 state"
service_cleanup $test_service
if [ $? -ne 0 ]; then
	print -- "--DIAG: $assertion: cleanup of a previous instance failed"
	exit $STF_UNRESOLVED
fi

if [ -f $dependency_file ]; then
	rm $dependency_file
	if [ $? -ne 0 ]; then
		print -- "--DIAG: Can't remove dependency file for setup"
		exit $STF_UNRESOLVED
	fi
fi

print -- "--INFO: generating manifest for importation into repository"
manifest_generate $registration_template \
	TEST_SERVICE=$test_service \
	TEST_INSTANCE1=$test_instance1 \
	TEST_INSTANCE2=$test_instance2 \
	SERVICE_APP=$service_app \
	LOGFILE=$service_log \
	STATEFILE1=$service_state1 \
	STATEFILE2=$service_state2 \
	DEPENDENCY_FILE=$dependency_file \
	> $registration_file

print -- "--INFO: Importing service into repository"
manifest_purgemd5 $registration_file
svccfg -v import $registration_file >$svccfg_errfile 2>&1

if [ $? -ne 0 ]; then
	print -- "--DIAG: $assertion: Unable to import the service $test_FMRI1"
        print -- "  error messages from svccfg: "
        print -- "  \"$(cat $svccfg_errfile)\""
	exit $STF_UNRESOLVED
fi
service_setup=1

print -- "--INFO: Wait for $test_FMRI2 to go to offline"
service_wait_state $test_FMRI2 offline
if [ $? -ne 0 ]; then
	print -- "--DIAG: $assertion: Service $test_FMRI2 didn't go to offline"
	exit $STF_FAIL
fi

print -- "--INFO: Enable $test_FMRI1"
svcadm enable $test_FMRI1

print -- "--INFO: Wait for $test_FMRI1 to go to online"
service_wait_state $test_FMRI1 online
if [ $? -ne 0 ]; then
	print -- "--DIAG: $assertion: Service $test_FMRI1 didn't go to online"
	exit $STF_FAIL
fi

print -- "--INFO: Wait for $test_FMRI2 to go to online - it shouldn't"
service_wait_state $test_FMRI2 online
if [ $? -eq 0 ]; then
	print -- "--DIAG: $assertion: Service $test_FMRI2 went online"
	exit $STF_FAIL
fi

print -- "--INFO: Creating the dependency file"
touch $dependency_file
if [ $? -ne 0 ]; then
	print -- "--DIAG: Can't create $dependency_file"
	exit $STF_UNRESOLVED
fi

print -- "--INFO: refreshing the service $test_FMRI2"
svcadm refresh $test_FMRI2
if [ $? -ne 0 ]; then
	print -- "--DIAG: $assertion: $test_FMRI2 did not refresh"
	exit $STF_UNRESOLVED
fi

print -- "--INFO: Waiting for $test_FMRI2 to come online"
service_wait_state $test_FMRI2 online
if [ $? -ne 0 ]; then
	print -- "--DIAG: $assertion: Service $test_FMRI2 did not come online"
	exit $STF_FAIL
fi

print -- "--INFO: Cleaning up service"
cleanup

exit $STF_PASS
