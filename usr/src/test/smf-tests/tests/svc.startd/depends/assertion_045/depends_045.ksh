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
# ASSERTION: depends_045
# DESCRIPTION:
#  A service with a single dependency of optional_all will transition to
#  online state even if the optional_all dependency is in the offline state.
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
	rm -f $service_state1 $service_state2 $service_state3
}

trap cleanup 0 1 2 15

readonly ME=$(whence -p ${0})
readonly MYLOC=$(dirname ${ME})

DATA=$MYLOC

readonly registration_template=$DATA/service_045.xml

extract_assertion_info $ME

# make sure that the svc.startd is running
verify_daemon
if [ $? -ne 0 ]; then
	echo "--DIAG: $assertion: svc.startd is not executing. Cannot "
	echo "  continue"
	exit $STF_UNRESOLVED
fi

# Make sure the environment is clean - the test service isn't running
echo "--INFO: Cleanup any old $test_service state"
service_cleanup $test_service
if [ $? -ne 0 ]; then
	echo "--DIAG: $assertion: cleanup of a previous instance failed"
	exit $STF_UNRESOLVED
fi

echo "--INFO: generating manifest for importation into repository"
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

echo "--INFO: Importing service into repository"
manifest_purgemd5 $registration_file
svccfg -v import $registration_file >$svccfg_errfile 2>&1

if [ $? -ne 0 ]; then
	echo "--DIAG: $assertion: Unable to import $test_service."
        echo "  Error messages from svccfg: "
        echo "  \"$(cat $svccfg_errfile)\""
	exit $STF_UNRESOLVED
fi
service_setup=1

echo "--INFO: List service dependencies"
for svc in $test_FMRI1 $test_FMRI2 $test_FMRI3; do
	svcs -l $svc | grep ^[fd]
	echo " "
done

echo "--INFO: Verifying $test_FMRI1 transitions to online state"
service_wait_state $test_FMRI1 online
if [ $? -ne 0 ]; then
	echo "--DIAG: $assertion: Service $test_FMRI1 is not online
	Current state: $(svcprop -p restarter/state $test_FMRI1)"
	exit $STF_FAIL
fi

typeset -i rc=0
echo "--INFO: [ Phase 1 ] Enable $test_FMRI3.
	This should transition $test_FMRI2 to online state"
svcadm enable $test_FMRI3
rc=$?
if [ $rc -ne 0 ]; then
	echo "--DIAG: $assertion: svcadm enable $test_FMRI3 failed
	EXPECTED: return 0
	OBSERVED: return $rc"
	exit $STF_UNRESOLVED
fi

echo "--INFO: Verify $test_FMRI2 is online"
service_wait_state $test_FMRI2 online
if [ $? -ne 0 ]; then
	echo "--DIAG: $assertion: Service $test_FMRI2 is not online
	Current state: $(svcprop -p restarter/state $test_FMRI2)"
	exit $STF_UNRESOLVED
fi

echo "--INFO: Verify $test_FMRI1 is still online"
service_wait_state $test_FMRI1 online
if [ $? -ne 0 ]; then
	echo "--DIAG: $assertion: Service $test_FMRI1 is not online
	Current state: $(svcprop -p restarter/state $test_FMRI1)"
	exit $STF_FAIL
fi

echo "--INFO: [ Phase 2 ] disable $test_FMRI3.
	This should transition $test_FMRI2 to offline state"
svcadm disable $test_FMRI3
rc=$?
if [ $rc -ne 0 ]; then
	echo "--DIAG: $assertion: svcadm disable $test_FMRI3 -- failed
	EXPECTED: return 0
	OBSERVED: return $rc"
	exit $STF_UNRESOLVED
fi

echo "--INFO: Verify $test_FMRI2 is offline"
service_wait_state $test_FMRI2 offline
if [ $? -ne 0 ]; then
	echo "--DIAG: $assertion: Service $test_FMRI2 is not offline
	Current state: $(svcprop -p restarter/state $test_FMRI2)"
	exit $STF_UNRESOLVED
fi

echo "--INFO: Verify $test_FMRI1 is still online"
service_wait_state $test_FMRI1 online
if [ $? -ne 0 ]; then
	echo "--DIAG: $assertion: Service $test_FMRI1 is not online
	Current state: $(svcprop -p restarter/state $test_FMRI1)"
	exit $STF_FAIL
fi

echo "--INFO: [ Phase 3 ] disable $test_FMRI2.
	$test_FMRI1 should stay online"
svcadm disable $test_FMRI2
rc=$?
if [ $rc -ne 0 ]; then
	echo "--DIAG: $assertion: svcadm disable $test_FMRI2 -- failed
	EXPECTED: return 0
	OBSERVED: return $rc"
	exit $STF_UNRESOLVED
fi

echo "--INFO: Verify $test_FMRI1 is online"
service_wait_state $test_FMRI1 online
if [ $? -ne 0 ]; then
	echo "--DIAG: $assertion: Service $test_FMRI1 is not online
	Current state: $(svcprop -p restarter/state $test_FMRI1)"
	exit $STF_FAIL
fi

echo "--INFO: Cleaning up services"
cleanup

exit $STF_PASS
