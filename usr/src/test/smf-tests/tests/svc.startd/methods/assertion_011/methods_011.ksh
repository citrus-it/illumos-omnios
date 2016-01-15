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
# ASSERTION: methods_011
# DESCRIPTION:
#  Upon starting a service the svc.startd invokes the service's start
#  method.
#  Should the method returns a non zero exit code other than the defined
#  straight to maintenance exit code then the method is considered to
#  have failed, but not irrevocably. svc.startd will retry the start
#  method. If the method succeeds before the error threshold is exceeded
#  then it will transition to the online state.
#
# end __stf_assertion__
#

. ${STF_TOOLS}/include/stf.kshlib
. ${STF_SUITE}/include/gltest.kshlib
. ${STF_SUITE}/include/svc.startd_config.kshlib
. ${STF_SUITE}/tests/svc.startd/include/svc.startd_common.kshlib

typeset service_setup=0
function cleanup {
	rm -f $returncode_file
	[[ $service_setup -ne 0 ]] && \
		$service_app -s $test_service -i $test_instance -m force_stop
	common_cleanup
	return $?
}

trap cleanup 0 1 2 15

readonly ME=$(whence -p ${0})
readonly MYLOC=$(dirname ${ME})

DATA=$MYLOC

registration_template=$DATA/service_011.xml

extract_assertion_info $ME

# make sure that the svc.startd is running
verify_daemon
if [ $? -ne 0 ]; then
	print -- "--DIAG: $assertion: svc.startd is not executing. Cannot "
	print -- "  continue"
	exit $STF_UNRESOLVED
fi

# Make sure the environment is clean - the test service isn't running
print -- "--INFO: Cleanup any old $test_FMRI state"
service_cleanup $test_service
rm -f $service_state
if [ $? -ne 0 ]; then
	print -- "--DIAG: $assertion: unable to clean up any pre-existing state"
	exit $STF_UNRESOLVED
fi

print -- "--INFO: creating returncode file"
echo "returncode $SVC_METHOD_OTHEREXIT" > $returncode_file
if [ $? -ne 0 ]; then
	print -- "--DIAG: $assertion: unable to write to file $returncode_file"
	exit $STF_UNRESOLVED
fi

print -- "--INFO: generating manifest for importation into repository"
manifest_generate $registration_template \
	TEST_SERVICE=$test_service \
	TEST_INSTANCE=$test_instance \
	SERVICE_APP=$service_app \
	LOGFILE=$service_log \
	STATEFILE=$service_state \
	START_EVENT=$returncode_file \
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

print -- "--INFO: Validating service is not in maintenance mode"
service_wait_state $test_FMRI maintenance $((ERROR_THRESHOLD / 2))
if [ $? -eq 0 ]; then
	print -- "--DIAG: $assertion: Service $test_FMRI went"
	print -- "  to maintenance mode too quickly"
	exit $STF_FAIL
fi

print -- "--INFO: Validating multiple invocations of start method"
service_countcall -f $service_state -s $test_service -i $test_instance start
countstarts=$?
if [ $countstarts -lt $((ERROR_THRESHOLD / 2)) ]; then
	print -- "--DIAG: $assertion: Service $test_FMRI did not "
	print -- "  call start method enough times"
	exit $STF_FAIL
fi

print -- "--INFO: Changing the returncode of start method to 0"
echo "returncode 0" > $returncode_file
if [ $? -ne 0 ]; then
	print -- "--DIAG: $assertion: could not alter returncode to 0"
	print -- "  for the stop method."
	exit $STF_UNRESOLVED
fi

# XXX: hack for maintenance mode and error_threshold being too low
service_wait_state $test_FMRI maintenance $ERROR_THRESHOLD
if [ $? -eq 0 ]; then
	print -- "--INFO: clearing maintenance mode"
	svcadm clear $test_FMRI
	if [ $? -ne 0 ]; then
		print -- "--DIAG: Could not clear maintenance mode"
		exit $STF_UNRESOLVED
	fi
fi

print -- "--INFO: Validating service reaching online state"
service_wait_state $test_FMRI online
if [ $? -ne 0 ]; then
	print -- "--DIAG: $assertion: Service $test_FMRI did not"
	print -- "  transition to the online state"
	exit $STF_FAIL
fi

print -- "--INFO: Cleaning up service"
cleanup

exit $STF_PASS
