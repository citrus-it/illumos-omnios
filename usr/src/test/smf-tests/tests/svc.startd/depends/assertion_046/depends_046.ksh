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
# ASSERTION: depends_046
# DESCRIPTION:
#  Pair of dependent services.
#  service_A has multiple dependencies on other services. It will stay
#  online if any of its optional_all dependent services are offline.
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
	rm -f $service_state1 $service_state2 $service_state3 $service_state4 \
		$service_state5
}

trap cleanup 0 1 2 15

readonly ME=$(whence -p ${0})
readonly MYLOC=$(dirname ${ME})

DATA=$MYLOC

readonly registration_template=$DATA/service_046.xml

extract_assertion_info $ME

# make sure that the svc.startd is running
verify_daemon
if [ $? -ne 0 ]; then
	print -- "--DIAG: $assertion: svc.startd is not executing. Cannot "
	print -- "  continue"
	exit $STF_UNRESOLVED
fi

# Make sure the environment is clean - the test service isn't running
print -- "--INFO: Cleanup any old $test_service state"
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
	TEST_INSTANCE4=$test_instance4 \
	TEST_INSTANCE5=$test_instance5 \
	SERVICE_APP=$service_app \
	LOGFILE=$service_log \
	STATEFILE1=$service_state1 \
	STATEFILE2=$service_state2 \
	STATEFILE3=$service_state3 \
	STATEFILE4=$service_state4 \
	STATEFILE5=$service_state5 \
	> $registration_file

print -- "--INFO: Importing service into repository"
manifest_purgemd5 $registration_file
svccfg -v import $registration_file >$svccfg_errfile 2>&1

if [ $? -ne 0 ]; then
	echo "--DIAG: $assertion: Unable to import the service $test_service"
        echo "  and $test_FMRI2 error messages from svccfg: "
        echo "  \"$(cat $svccfg_errfile)\""
	exit $STF_UNRESOLVED
fi
service_setup=1

print -- "--INFO: List service dependencies"
for svc in $test_FMRI1 $test_FMRI2 $test_FMRI3; do
	svcs -l $svc | grep ^[fd]
	print " "
done

# phase 1 ... all dependents disabled
print -- "--INFO: [Phase 1] Wait for $test_FMRI1 to come online"
service_wait_state $test_FMRI1 online
if [ $? -ne 0 ]; then
	print -- "--DIAG: $assertion: Service $test_FMRI1 dodn't come online"
	exit $STF_FAIL
fi

thresh=0

# phase 2 ... enable one of the dependencies, should stay offline
print -- "--INFO: [ Phase 2 ] Enable $test_FMRI2 (currently offline)"
print "  $test_FMRI2 should stay offline, $test_FMRI1 should stay online"
print " "

print -- "--INFO: enabling $test_FMRI2, should stay offline"
svcadm enable $test_FMRI2
if [ $? -ne 0 ]; then
	echo "--DIAG: Could not enable $test_FMRI2"
	exit $STF_UNRESOLVED
fi

service_wait_state $test_FMRI2 offline
if [ $? -ne 0 ]; then
	echo "--DIAG: Service $test_FMRI2 did not enter the offline state."
	echo "  it is in '$(svcprop -p restarter/state $test_FMRI2)' state"
	exit $STF_INRESOLVED
fi

service_wait_state $test_FMRI1 online
if [ $? -ne 0 ]; then
	echo "--DIAG: $assertion: $test_FMRI1 did not stay offline"
	echo "  it is in the '$(svcprop -p restarter/state $test_FMRI1)' state"
	exit $STF_FAIL
fi

# enable dependency of $test_FMRI2, it should cause test_FMRI{1,2} to be online
echo "--INFO: [ Phase 3 ] online the required dependency of ($test_FMRI2)"
echo "--INFO: enable $test_FMRI4.
	$test_FMRI2 should go online and $test_FMRI1 should stay online."
svcadm enable $test_FMRI4
if [ $? -ne 0 ]; then
	echo "--DIAG: $assertion: $test_FMRI4 did not enable."
	exit $STF_UNRESOLVED
fi

service_wait_state $test_FMRI4 online
if [ $? -ne 0 ]; then
	echo "--DIAG: $assertion: $test_FMRI4 did not go online."
	echo "  it's in '$(svcprop -p restarter/state $test_FMRI4)' state."
	exit $STF_UNRESOLVED
fi

service_wait_state $test_FMRI2 online
if [ $? -ne 0 ]; then
	echo "--DIAG: $assertion: $test_FMRI2 did not go online."
	echo "  it's in '$(svcprop -p restarter/state $test_FMRI2)' state."
	exit $STF_UNRESOLVED
fi

service_wait_state $test_FMRI1 online
if [ $? -ne 0 ]; then
	echo "--DIAG: $assertion: $test_FMRI1 did not stay online."
	echo "  it's in '$(svcprop -p restarter/state $test_FMRI1)' state."
	exit $STF_FAIL
fi

echo "--INFO: [ Phase 4 ] enable dependency $test_FMRI3
	$test_FMRI1 should stay online"

echo "--INFO: Enabling $test_FMRI3"
svcadm enable $test_FMRI3
if [ $? -ne 0 ]; then
	echo "--DIAG: $assertion: $test_FMRI3 would not enable."
	exit $STF_UNRESOLVED
fi

service_wait_state $test_FMRI3 offline
if [ $? -ne 0 ]; then
	echo "--DIAG: $assertion: $test_FMRI3 is not offline."
	echo "  it's in '$(svcprop -p restarter/state $test_FMRI3)' state."
	exit $STF_UNRESOLVED
fi
service_wait_state $test_FMRI1 online
if [ $? -ne 0 ]; then
	echo "--DIAG: $assertion: $test_FMRI1 is not online."
	echo "  it's in '$(svcprop -p restarter/state $test_FMRI1)' state."
	exit $STF_FAIL
fi

echo "--INFO: [ Phase 5 ] Enable $test_FMRI5.
	$test_FMRI3 should go online; $test_FMRI1 should stay online."
svcadm enable $test_FMRI5
if [ $? -ne 0 ]; then
	echo "--DIAG: $assertion: $test_FMRI5 would not enable"
	exit $STF_UNRESOLVED
fi

service_wait_state $test_FMRI3 online
if [ $? -ne 0 ]; then
	echo "--DIAG: $assertion: $test_FMRI3 is not online."
	echo "  it's in '$(svcprop -p restarter/state $test_FMRI3)' state."
	exit $STF_UNRESOLVED
fi

service_wait_state $test_FMRI1 online
if [ $? -ne 0 ]; then
	echo "--DIAG: $assertion: $test_FMRI1 is not online."
	echo "  it's in '$(svcprop -p restarter/state $test_FMRI1)' state."
	exit $STF_FAIL
fi

# redisable a dependency of one of the services (this should cause an offline)
echo "--INFO: [ Phase 6 ] Disable $test_FMRI4.
	$test_FMRI2 should go offline, $test_FMRI1 should stay online"
svcadm disable $test_FMRI4
if [ $? -ne 0 ]; then
	echo "--DIAG: $assertion: could not disable $test_FMRI4"
	exit $STF_UNRESOLVED
fi

service_wait_state $test_FMRI2 offline
if [ $? -ne 0 ]; then
	echo "--DIAG: $assertion: $test_FMRI2 is not offline."
	echo "  it's in '$(svcprop -p restarter/state $test_FMRI2)' state."
	exit $STF_UNRESOLVED
fi

service_wait_state $test_FMRI1 online
if [ $? -ne 0 ]; then
	echo "--DIAG: $assertion: $test_FMRI1 is not online."
	echo "  it's in '$(svcprop -p restarter/state $test_FMRI1)' state."
	exit $STF_FAIL
fi

# disable $test_FMRI2 ... this should restart $test_FMRI1
echo "--INFO: [ Phase 7 ] Disable $test_FMRI2.
	$test_FMRI1 should stay online"
svcadm disable $test_FMRI2
if [ $? -ne 0 ]; then
	echo "--DIAG: $assertion: $test_FMRI2 would not disable"
	exit $STF_UNRESOLVED
fi

service_wait_state $test_FMRI1 online
if [ $? -ne 0 ]; then
	echo "--DIAG: $assertion: $test_FMRI1 did not stay online"
	echo "  it's in '$(svcprop -p restarter/state $test_FMRI1)' state."
	exit $STF_FAIL
fi

# Disable dependency of $test_FMRI3 - this will offline $test_FMRI1
echo "--INFO: [ Phase 8 ] disable $test_FMRI5.
	$test_FMRI3 should go offline, $test_FMRI1 should stay offline."
svcadm disable $test_FMRI5
if [ $? -ne 0 ]; then
	echo "--DIAG: $assertion: could not disable $test_FMRI5"
	exit $STF_UNRESOLVED
fi

service_wait_state $test_FMRI3 offline
if [ $? -ne 0 ]; then
	echo "--DIAG: $assertion: $test_FMRI3 is not offline."
	echo "  it's in '$(svcprop -p restarter/state $test_FMRI3)' state."
	exit $STF_UNRESOLVED
fi

service_wait_state $test_FMRI1 online
if [ $? -ne 0 ]; then
	echo "--DIAG: $assertion: $test_FMRI1 is not online."
	echo "  it's in '$(svcprop -p restarter/state $test_FMRI1)' state."
	exit $STF_FAIL
fi

echo "--INFO: [ Phase 9 ] Disable $test_FMRI3. $test_FMRI1 should stay online"
svcadm disable $test_FMRI3
if [ $? -ne 0 ]; then
	echo "--DIAG: $assertion: $test_FMRI3 would not disable"
	exit $STF_UNRESOLVED
fi

service_wait_state $test_FMRI1 online
if [ $? -ne 0 ]; then
	echo "--DIAG: $assertion: $test_FMRI1 is not online"
	echo "  it's in '$(svcprop -p restarter/state $test_FMRI1)' state."
	exit $STF_FAIL
fi

print -- "--INFO: Cleaning up service"
cleanup

exit $STF_PASS
