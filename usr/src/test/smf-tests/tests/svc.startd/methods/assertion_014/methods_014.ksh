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
# ASSERTION: methods_014
# DESCRIPTION:
#  If svc.startd invokes a service's refresh method and that method
#  returns a non zero exit code other than the defined straight to
#  maintenance exit code then the service is considered
#  to have failed, but not irrevocably. svc.startd will retry the
#  refresh method. Should the method succeed before the error
#  threshold is reached then the service will stay in the online state.
#
# end __stf_assertion__
#

. ${STF_TOOLS}/include/stf.kshlib
. ${STF_SUITE}/include/gltest.kshlib
. ${STF_SUITE}/include/svc.startd_config.kshlib
. ${STF_SUITE}/tests/svc.startd/include/svc.startd_common.kshlib

typeset service_setup=0
function cleanup {
        rm -f $svcadm_errfile $returncode_file
	common_cleanup
	return $?
}

trap cleanup 0 1 2 15

readonly ME=$(whence -p ${0})
readonly MYLOC=$(dirname ${ME})

DATA=$MYLOC

registration_template=$DATA/service_014.xml

extract_assertion_info $ME

# make sure that the svc.startd is running
verify_daemon
if [ $? -ne 0 ]; then
	print -- "--DIAG: $assertion: svc.startd is not executing. Cannot "
	print -- "  continue"
	exit $STF_UNRESOLVED
fi

# make sure that the features are available
features=`feature_test REFRESH`
if [ $? -ne 0 ]; then
	print -- "--DIAG: $assertion: $features missing from startd"
	exit $STF_UNTESTED
fi

# Make sure the environment is clean - the test service isn't running
print -- "--INFO: Cleanup any old $test_FMRI state"
service_cleanup $test_service
rm -f $service_state
if [ $? -ne 0 ]; then
	print -- "--DIAG: $assertion: unable to clean up any pre-existing state"
	exit $STF_UNRESOLVED
fi

print -- "--INFO: Creating returncode file"
echo "returncode $SVC_METHOD_OTHEREXIT" > $returncode_file
if [ $? -ne 0 ]; then
	print -- "--DIAG: $assertion: unable to create returncode file"
	exit $STF_UNRESOLVED
fi

print -- "--INFO: generating manifest for importation into repository"
manifest_generate $registration_template \
	TEST_SERVICE=$test_service \
	TEST_INSTANCE=$test_instance \
	SERVICE_APP=$service_app \
	LOGFILE=$service_log \
	STATEFILE=$service_state \
	REFRESH_EVENT=$returncode_file \
	> $registration_file

print -- "--INFO: Importing service into repository"
manifest_purgemd5 $registration_file
svccfg -v import $registration_file >$svccfg_errfile 2>&1
if [ $? -ne 0 ]; then
	print -- "--DIAG: $assertion: Unable to import the service $test_FMRI"
	print -- "  error messages from svccfg: \"$(cat $svccfg_errfile)\""
	exit $STF_UNRESOLVED
fi
service_setup=1

print -- "--INFO: enabling $test_FMRI"
svcadm enable $test_FMRI
if [ $? -ne 0 ]; then
	print -- "--DIAG: $assertion: enable command of $test_FMRI failed"
	exit $STF_FAIL
fi

print -- "--INFO: waiting for execution of start method"
service_wait_method $test_FMRI start
if [ $? -ne 0 ]; then
	print -- "--DIAG: $assertion: Service $test_FMRI didn't issue start"
	exit $STF_UNRESOLVED
fi

print -- "--INFO: Validating service reaches online mode"
service_wait_state $test_FMRI online
if [ $? -ne 0 ]; then
	print -- "--DIAG: $assertion: Service $test_FMRI didn't"
	print -- "  transition to online mode"
	exit $STF_FAIL
fi

print -- "--INFO: Issuing refresh request"
svcadm refresh $test_FMRI >$svcadm_errfile 2>&1
if [ $? -ne 0 -o -s $svcadm_errfile ]; then
	print -- "--DIAG: $assertion: refresh of Service $test_FMRI did not "
	print -- "  succeed. Error output: \"$(cat $svcadm_errfile)\""
	exit $STF_UNRESOLVED
fi

print -- "--INFO: Waiting for call of $test_FMRI refresh method"
service_wait_method $test_FMRI refresh
if [ $? -ne 0 ]; then
	print -- "--DIAG: $assertion: didn't issue refresh method"
	exit $STF_FAIL
fi

print -- "--INFO: Changing the returncode for refresh to 0"
print --  "returncode 0" >$returncode_file
if [ $? -ne 0 ]; then
	print -- "--DIAG: $assertion: could not alter returncode to 0"
	print -- "  for the stop method."
	exit $STF_UNRESOLVED
fi

print -- "--INFO: Validating service doesn't reach maintenance state"
service_wait_state $test_FMRI maintenance
if [ $? -eq 0 ]; then
	print -- "--DIAG: $assertion: Service $test_FMRI did"
	print -- "  transition to the maintenance state."
	exit $STF_FAIL
fi

print -- "--INFO: Validating many invocations of refresh method"
service_countcall -f $service_state -s $test_service -i $test_instance refresh
countstops=$?
if [ $countstops -lt $((ERROR_THRESHOLD / 2)) ]; then
	print -- "--DIAG: $assertion: Service $test_FMRI called"
	print -- "  stop method insufficient times"
	exit $STF_FAIL
fi

print -- "--INFO: Validating the online state of the service"
service_wait_state $test_FMRI online
if [ $? -ne 0 ]; then
	print -- "--DIAG: $assertion: Service $test_FMRI is not "
	print -- "  online."
	exit $STF_FAIL
fi

print -- "--INFO: Cleaning up service"
cleanup

exit $STF_PASS
