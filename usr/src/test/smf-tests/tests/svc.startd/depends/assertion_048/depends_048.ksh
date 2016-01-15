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
# ASSERTION: depends_048
# DESCRIPTION:
#  A service with a dependency of optional_all will go online even if
#  that dependency is in the maintenance state.
#  The test service has multiple optional_all dependencies.
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

readonly registration_template=$DATA/service_048.xml

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

print -- "--INFO: List service dependencies"
for svc in $test_FMRI1 $test_FMRI2 $test_FMRI3; do
	svcs -l $svc | grep ^[fd]
	print " "
done

echo "--INFO: Ensuring $test_FMRI1 comes online"
service_wait_state $test_FMRI1 online
if [ $? -ne 0 ]; then
	echo "--DIAG: $assertion: Service $test_FMRI1 didn't come online"
	exit $STF_UNRESOLVED
fi

echo "--INFO: [ Phase 1 ] Transition dependency $test_FMRI2 to maintenance"
svcadm mark maintenance $test_FMRI2
if [ $? -ne 0 ]; then
	echo "--DIAG: $assertion: service $test_FMRI2 could not be set to"
	echo "  maintenance mode."
	exit $STF_UNRESOLVED
fi
service_wait_state $test_FMRI2 maintenance
if [ $? -ne 0 ]; then
	echo "--DIAG: $assertion: Service $test_FMRI2 is not in maintenance"
	echo "	It is in '$(svcprop -p restarter/state $test_FMRI2)' state,"
	exit $STF_UNRESOLVED
fi

echo "--INFO: Checking that $test_FMRI1 is online"
service_wait_state $test_FMRI1 online
if [ $? -ne 0 ]; then
	echo "--DIAG: $assertion: Service $test_FMRI1 is not online"
	echo "	It is in '$(svcprop -p restarter/state $test_FMRI1)' state,"
	echo "  $test_FMRI2 is in '$(svcprop -p restarter/state $test_FMRI2)'
	state."
	exit $STF_FAIL
fi

echo "--INFO: [ Phase 2 ] Transition dependency $test_FMRI3 to maintenance"
svcadm mark maintenance $test_FMRI3
if [ $? -ne 0 ]; then
	echo "--DIAG: $assertion: service $test_FMRI3 could not be set to"
	echo "  maintenance mode."
	exit $STF_UNRESOLVED
fi
service_wait_state $test_FMRI3 maintenance
if [ $? -ne 0 ]; then
	echo "--DIAG: $assertion: Service $test_FMRI3 is not in maintenance"
	echo "	It is in '$(svcprop -p restarter/state $test_FMRI3)' state,"
	exit $STF_UNRESOLVED
fi


echo "--INFO: Checking that $test_FMRI1 is online"
service_wait_state $test_FMRI1 online
if [ $? -ne 0 ]; then
	echo "--DIAG: $assertion: Service $test_FMRI1 is not online"
	echo "	It is in '$(svcprop -p restarter/state $test_FMRI1)' state,"
	echo "	$test_FMRI3 is in '$(svcprop -p restarter/state $test_FMRI3)'
	state."
	exit $STF_FAIL
fi

echo "--INFO: [ Phase 3 ] Clear maintenance state of $test_FMRI2"
svcadm clear $test_FMRI2
if [ $? -ne 0 ]; then
	echo "--DIAG: $assertion: could not clear maintenance of $test_FMRI2"
	exit $STF_UNRESOLVED
fi

echo "--INFO: wait for $test_FMRI2 to come online"
service_wait_state $test_FMRI2 online
if [ $? -ne 0 ]; then
	echo "--DIAG: $assertion: service $test_FMRI2 is not online"
	echo "	it is in '$(svcprop -p restarter/state $test_FMRI2)' state."
	exit $STF_UNRESOLVED
fi

echo "--INFO: Checking that $test_FMRI1 is online"
service_wait_state $test_FMRI1 online
if [ $? -ne 0 ]; then
	echo "--DIAG: $assertion: Service $test_FMRI1 is not online"
	echo "	It is in '$(svcprop -p restarter/state $test_FMRI1)' state,"
	echo "  $test_FMRI2 is in '$(svcprop -p restarter/state $test_FMRI2)'
	state."
	exit $STF_FAIL
fi

echo "--INFO: [Phase 4] clearing maintenance state $test_FMRI3"
svcadm clear $test_FMRI3
if [ $? -ne 0 ]; then
	echo "--DIAG: $assertion: service $test_FMRI3 would not clear state"
	exit $STF_UNRESOLVED
fi

echo "--INFO: check that $test_FMRI1 is online"
service_wait_state $test_FMRI1 online
if [ $? -ne 0 ]; then
	echo "--DIAG: $assertion: Service $test_FMRI1 didn't stay online"
	exit $STF_FAIL
fi

echo "--INFO: [ Phase 5 ] marking maintenance on $test_FMRI3"
svcadm mark maintenance $test_FMRI3
if [ $? -ne 0 ]; then
	echo "--DIAG: $assertion: $test_FMRI3 would not mark maintenance"
	exit $STF_UNRESOLVED
fi
service_wait_state $test_FMRI3 maintenance
if [ $? -ne 0 ]; then
	echo "--DIAG: $assertion: Service $test_FMRI3 is not in maintenance"
	echo "	It is in '$(svcprop -p restarter/state $test_FMRI3)' state,"
	exit $STF_UNRESOLVED
fi

echo "--INFO: wait for $test_FMRI1 to go online"
service_wait_state $test_FMRI1 online
if [ $? -ne 0 ]; then
	echo "--DIAG: $assertion: service $test_FMRI1 isn't online"
	echo "	It's in '$(svcprop -p restarter/state $test_FMRI1)' state.
	exit $STF_FAIL"
fi

echo "--INFO: [ Phase 6 ] Clear maintenance state of $test_FMRI3"
svcadm clear $test_FMRI3
if [ $? -ne 0 ]; then
	echo "--DIAG: $assertion: could not clear maintenance of $test_FMRI3"
	exit $STF_UNRESOLVED
fi

echo "--INFO: wait for $test_FMRI3 to come online"
service_wait_state $test_FMRI3 online
if [ $? -ne 0 ]; then
	echo "--DIAG: $assertion: service $test_FMRI3 is not online"
	echo "  it is in the '$(svcprop -p restarter/state $test_FMRI3)' state."
	exit $STF_UNRESOLVED
fi

echo "--INFO: check that $test_FMRI1 is still online"
service_wait_state $test_FMRI1 online
if [ $? -ne 0 ]; then
	echo "--DIAG: $assertion: service $test_FMRI1 is not online"
	echo "  it is in the '$(svcprop -p restarter/state $test_FMRI1)' state."
	exit $STF_FAIL
fi

echo "--INFO: [ Phase 7 ] disabling $test_FMRI2"
svcadm disable $test_FMRI2
if [ $? -ne 0 ]; then
	echo "--DIAG: $assertion: service $test_FMRI2 would not disable"
	exit $STF_UNRESOLVED
fi

echo "--INFO: check that $test_FMRI1 is still online"
service_wait_state $test_FMRI1 online
if [ $? -ne 0 ]; then
	echo "--DIAG: $assertion: Service $test_FMRI1 didn't stay online"
	exit $STF_FAIL
fi

echo "--INFO: [ Phase 8 ] disabling $test_FMRI3"
svcadm disable $test_FMRI3
if [ $? -ne 0 ]; then
	echo "--DIAG: $assertion: service $test_FMRI3 would not disable"
	exit $STF_UNRESOLVED
fi

echo "--INFO: check that $test_FMRI1 is still online"
service_wait_state $test_FMRI1 online
if [ $? -ne 0 ]; then
	echo "--DIAG: $assertion: Service $test_FMRI1 didn't stay online"
	exit $STF_FAIL
fi

echo "--INFO: Cleaning up service"
cleanup

exit $STF_PASS
