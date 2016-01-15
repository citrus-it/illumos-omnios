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
# ASSERTION: methods_008
# DESCRIPTION:
#  If a service in the online state receives a request to disable
#  svc.startd will invoke the stop method for the service. If the stop
#  method returns a non zero value other than the defined straight to
#  maintenance return code. svc.startd will retry the stop method.
#  It will end the effort if the stop method succeeds before exceeding
#  the error threshold.
#  The service will enter the disabled state.
#
# end __stf_assertion__
#

. ${STF_TOOLS}/include/stf.kshlib
. ${STF_SUITE}/include/gltest.kshlib
. ${STF_SUITE}/include/svc.startd_config.kshlib
. ${STF_SUITE}/tests/svc.startd/include/svc.startd_common.kshlib

typeset service_setup=0
function cleanup {
	rm -f $servicecode_file
	[[ $service_setup -ne 0 ]] && $service_app -s $test_service \
			-i $test_instance -m force_stop
	common_cleanup
	return $?
}

trap cleanup 0 1 2 15

readonly ME=$(whence -p ${0})
readonly MYLOC=$(dirname ${ME})

DATA=$MYLOC

registration_template=$DATA/service_008.xml

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
print --  "returncode $SVC_METHOD_OTHEREXIT" >$servicecode_file
if [ $? -ne 0 ]; then
	printf -- "--DIAG: $assertion: unable to write to $servicecode_file"
	exit $STF_UNRESOLVED
fi

print -- "--INFO: generating manifest for importation into repository"
manifest_generate $registration_template \
	TEST_SERVICE=$test_service \
	TEST_INSTANCE=$test_instance \
	SERVICE_APP=$service_app \
	LOGFILE=$service_log \
	STATEFILE=$service_state \
	RETURNCODE="$servicecode_file" \
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

print -- "--INFO: Wait for $test_FMRI to come online"
service_wait_state $test_FMRI online
if [ $? -ne 0 ]; then
	print -- "--DIAG: $assertion: Service $test_FMRI didn't go online"
	exit $STF_FAIL
fi

print -- "--INFO: disabling service"
svcadm disable $test_FMRI
if [ $? -ne 0 ]; then
	print -- "$assertion: disable command for Service $test_FMRI didn't "
	print -- "  succeed"
	exit $STF_UNRESOLVED
fi

print -- "--INFO: Waiting for issue of $test_FMRI stop method"
service_wait_method $test_FMRI stop
if [ $? -ne 0 ]; then
	print -- "--DIAG: $assertion: Service $test_FMRI didn't issue stop"
	exit $STF_FAIL
fi

print -- "--INFO: Validating service is not in maintenance mode"
service_wait_state $test_FMRI maintenance $((ERROR_THRESHOLD / 2))
if [ $? -eq 0 ]; then
	print -- "--DIAG: $assertion: Service $test_FMRI transitioned "
	print -- "  to maintenance mode"
	exit $STF_FAIL
fi

print -- "--INFO: Validating many invocations of stop method"
service_countcall -f $service_state -s $test_service -i $test_instance stop
countstops=$?
if [ $countstops -lt $((ERROR_THRESHOLD / 2)) ]; then
	print -- "--DIAG: $assertion: Service $test_FMRI called "
	print -- "  stop method insufficient times"
	exit $STF_FAIL
fi

print -- "--INFO: Changing the returncode for stop to 0"
print --  "returncode 0" >$servicecode_file
if [ $? -ne 0 ]; then
	print -- "--DIAG: $assertion: could not alter returncode to 0"
	print -- "  for the stop method."
	exit $STF_UNRESOLVED
fi

# XXX: this is a hack. Maintenance mode may by reached because of the low
# error threshold (3)
service_wait_state $test_FMRI maintenance $ERROR_THRESHOLD
if [ $? -eq 0 ]; then
	print -- "--INFO: clearing maintenance state from service"
	svcadm clear $test_FMRI
	if [ $? -ne 0 ]; then
		print -- "--DIAG: $assertion: could not clear $test_FMRI"
		print -- "  from maintenance state."
		exit $STF_UNRESOLVED
	fi
fi

print -- "--INFO: Validating the disabling of the service"
service_wait_state $test_FMRI disabled
if [ $? -ne 0 ]; then
	print -- "--DIAG: $assertion: Service $test_FMRI did not "
	print -- "  transition to disabled mode"
	exit $STF_FAIL
fi

print -- "--INFO: Cleaning up service"
cleanup

exit $STF_PASS
