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
# ASSERTION: methods_041
# DESCRIPTION:
#  A service that possesses a "failing, but not unrecoverable" enable
#  method will upon the issuing of 'svcadm enable' result in the enable
#  method being executed until the error threshold is reached; However
#  should the enable method succeed before the error threshold is
#  reached then it will transition to the online state.
# end __stf_assertion__
#

. ${STF_TOOLS}/include/stf.kshlib
. ${STF_SUITE}/include/gltest.kshlib
. ${STF_SUITE}/include/svc.startd_config.kshlib
. ${STF_SUITE}/tests/svc.startd/include/svc.startd_common.kshlib

typeset service_setup=0
function cleanup {
	rm -f $returncode_file
	common_cleanup
}

trap cleanup 0 1 2 15

readonly ME=$(whence -p ${0})
readonly MYLOC=$(dirname ${ME})

DATA=$MYLOC

registration_template=$DATA/service_041.xml

extract_assertion_info $ME

# make sure that the svc.startd is running
verify_daemon
if [ $? -ne 0 ]; then
	print -- "--DIAG: $assertion: svc.startd is not executing. "
	print -- "  Cannot continue"
	exit $STF_UNRESOLVED
fi

# feature testing
features=`feature_test METH_ENABLE`
if [ $? -ne 0 ]; then
	print -- "--DIAG: $assertion: $features missing from startd"
	exit $STF_UNTESTED
fi

# Make sure the environment is clean - the test service isn't running
print -- "--INFO: Cleanup any old $test_FMRI state"
service_cleanup $test_service
if [ $? -ne 0 ]; then
	print -- "--DIAG: $assertion: cleanup of previous service state failed"
	exit $STF_UNRESOLVED
fi

print -- "--INFO: generating returncode file"
echo "returncode $SVC_METHOD_OTHEREXIT" > $returncode_file
if [ $? -ne 0 ]; then
	print -- "--DIAG: could not create the returncode file"
	exit $STF_UNRESOLVED
fi

print -- "--INFO: generating manifest for importation into repository"
manifest_generate $registration_template \
	TEST_SERVICE=$test_service \
	TEST_INSTANCE=$test_instance \
	SERVICE_APP=$service_app \
	LOGFILE=$service_log \
	STATEFILE=$service_state \
	ENABLE_RESULT=$returncode_file \
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

print -- "--INFO: Wait for $test_FMRI to come online ... it should not!"
service_wait_state $test_FMRI online
if [ $? -eq 0 ]; then
	print -- "--DIAG: $assertion: Service $test_FMRI went online"
	exit $STF_FAIL
fi

print -- "--INFO: Enabling service"
svcadm enable $test_FMRI
if [ $? -ne 0 ]; then
	print -- "--DIAG: $assertion: Service $test_FMRI didn't try to enable"
	exit $STF_UNRESOLVED
fi

print -- "--INFO: Waiting for invocation of enable method"
service_wait_method $test_FMRI enable
if [ $? -ne 0 ]; then
	print -- "--DIAG: $assertion: $test_FMRI didn't execute enable method"
	exit $STF_FAIL
fi

print -- "--INFO: Waiting for transition of $test_FMRI back to disabled"
service_wait_state $test_FMRI disabled
if [ $? -ne 0 ]; then
	print -- "--DIAG: $assertion: Service $test_FMRI didn't goto disabled"
	exit $STF_FAIL
fi

print -- "--INFO: removing reason to fail to enable"
echo "returncode 0" > $returncode_file

print -- "--INFO: waiting for service to come online"
service_wait_state $test_FMRI online
if [ $? -ne 0 ]; then
	print -- "--DIAG: $assertion: Service $test_FMRI didn't come online"
	exit $STF_FAIL
fi

print -- "--INFO: Validating multiple calls of enable"
service_countcalls -f $service_state -s $test_service -i $test_instance enable
count=$?
if [ $count -lt $((ERROR_THRESHOLD / 2)) ]; then
	print -- "--DIAG: $assertion: Service $test_FMRI didn't call "
	print -- "  enable method enough times."
	print -- "	EXPECTED: called >= $((ERROR_THRESHOLD / 2)) times"
	print -- "	OBSERVED: called    $count times"
	exit $STF_FAIL
fi

print -- "--INFO: Cleaning up service"
cleanup

exit $STF_PASS
