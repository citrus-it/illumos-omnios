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
# ASSERTION: methods_043
# DESCRIPTION:
#  A service that does not leave any processes after returning from the 
#  start method will be repeatedly invoked until it is eventually placed
#  in maintenance.
#
#  The current invocation threshold is defined in $ERROR_THRESHOLD.  The 
#  service will be restarted at least $ERROR_THRESHOLD times.
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
	/bin/rm -f $START_COUNT_FILE
}

trap cleanup 0 1 2 15

readonly ME=$(whence -p ${0})
readonly MYLOC=$(dirname ${ME})

DATA=$MYLOC
START_COUNT_FILE=/tmp/service_043_startcount

readonly registration_template=$DATA/service_043.xml

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
if [ $? -ne 0 ]; then
	print -- "--DIAG: $assertion: cleanup of a previous instance failed"
	exit $STF_UNRESOLVED
fi

print -- "--INFO: generating manifest for importation into repository"
manifest_generate $registration_template \
	TEST_SERVICE=$test_service \
	TEST_INSTANCE=$test_instance \
	TEST_START_METHOD=$STF_SUITE/$STF_EXEC/service_043_startmethod \
	SERVICE_APP=$service_app \
	LOGFILE=$service_log \
	STATEFILE=$service_state > $registration_file

print -- "--INFO: Importing service into repository"
manifest_purgemd5 $registration_file
svccfg -v import $registration_file >$svccfg_errfile 2>&1

if [ $? -ne 0 ]; then
	print -- "--DIAG: $assertion: Unable to import the service $test_FMRI"
	print -- "  error messages from svccfg: \"$(cat $svccfg_errfile)\""
	exit $STF_UNRESOLVED
fi
service_setup=1

# wait for service to enter maintenance state
print -- "--INFO: Check $test_FMRI is in maintenance"
service_wait_state $test_FMRI maintenance
if [[ $? -ne 0 ]]; then
	print -- "--DIAG: $assertion: Service $test_FMRI is not in maintenance"
	print -- "	It is in $(svcprop -p restarter/state $test_FMRI) state"
	exit $STF_FAIL
fi

typeset -i count=$(cat $START_COUNT_FILE)
if [[ $count -lt ${ERROR_THRESHOLD} ]]; then
	print -- "--DIAG: $assertion: incorrect start method invocation count"
	print -- "	EXPECTED: service was restarted >= 3 times"
	print -- "	OBSERVED: service was restarted $count times"
	exit $STF_FAIL
fi

print -- "--INFO: Cleaning up service"
cleanup

exit $STF_PASS
