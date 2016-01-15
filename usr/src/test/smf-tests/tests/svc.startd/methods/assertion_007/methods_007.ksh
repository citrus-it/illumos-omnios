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
# ASSERTION: methods_007
# DESCRIPTION:
#  A service that is online is disabled. It's stop method exits with a
#  non-zero value that is not the defined straight to maintenance exit
#  code. The starter will continue to try to stop the service until
#  the error threshold is reached, at which point the service will
#  transition to the maintenance mode.
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
	ret=$?

	# Forcibly kill the start/stop method process
	pkill -9 -z $(zonename) service_app

	return $ret
}

trap cleanup 0 1 2 15

readonly ME=$(whence -p ${0})
readonly MYLOC=$(dirname ${ME})

DATA=$MYLOC

registration_template=$DATA/service_007.xml

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

print -- "--INFO: generating manifest for importation into repository"
manifest_generate $registration_template \
	TEST_SERVICE=$test_service \
	TEST_INSTANCE=$test_instance \
	SERVICE_APP=$service_app \
	LOGFILE=$service_log \
	STATEFILE=$service_state \
	STOP_EVENT="returncode $SVC_METHOD_OTHEREXIT" \
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
	print -- "--DIAG: $assertion: disable command for Service "
	print -- "  $test_FMRI didn't succeed"
	exit $STF_UNRESOLVED
fi

print -- "--INFO: Waiting for issue of $test_FMRI stop method"
service_wait_method $test_FMRI stop
if [ $? -ne 0 ]; then
	print -- "--DIAG: $assertion: Service $test_FMRI didn't issue stop"
	exit $STF_FAIL
fi

print -- "--INFO: Validating service is in maintenance mode"
service_wait_state $test_FMRI maintenance
if [ $? -ne 0 ]; then
	print -- "--DIAG: $assertion: Service $test_FMRI did not "
	print -- "  transition to maintenance mode"
	exit $STF_FAIL
fi

print -- "--INFO: Validating many invocations of stop method"
service_countcall -f $service_state -s $test_service -i $test_instance stop
countstops=$?
if [ $countstops -lt $ERROR_THRESHOLD ]; then
	print -- "--DIAG: $assertion: Service $test_FMRI called stop method"
	print -- "  insufficient times"
	exit $STF_FAIL
fi

print -- "--INFO: Cleaning up service"
cleanup

exit $STF_PASS
