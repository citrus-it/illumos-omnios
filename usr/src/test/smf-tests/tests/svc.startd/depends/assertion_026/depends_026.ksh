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
# ASSERTION: depends_026
# DESCRIPTION:
#  If an online service, service_A, has a dependency on another service,
#  service_B, and service_B is transitioned into offline mode, then 
#  service_A is transitioned offline.  Once service_B transitions to 
#  online then service_A will transition to online as well.
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

readonly registration_template=$DATA/service_026.xml

extract_assertion_info $ME

# make sure that the svc.startd is running
verify_daemon
if [ $? -ne 0 ]; then
	print -- "--DIAG: $assertion: svc.startd is not executing. Cannot "
	print -- "  continue"
	exit $STF_UNRESOLVED
fi

typeset startdpid=$(pgrep -z `zonename` svc.startd)
typeset -i count=$(set -- $startdpid; echo $#)

if [ $count -ne 1 ]; then
	print -- "--DIAG: there are $count instances of svc.startd running"
	[ $count -ne 0 ] && print -- "  pids are: $startdpid"
	print -- "  There should be only one(1) svc.startd instance"
	exit $STF_UNRESOLVED
fi

# Make sure the environment is clean - the test service isn't running
print -- "--INFO: Cleanup any old $test_FMRI1, $test_FMRI2, $test_FMRI3 state"
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
	SERVICE_APP=$service_app \
	LOGFILE=$service_log \
	STATEFILE1=$service_state1 \
	STATEFILE2=$service_state2 \
	STATEFILE3=$service_state3 \
	> $registration_file

print -- "--INFO: Importing service into repository"
manifest_purgemd5 $registration_file
svccfg -v import $registration_file >$svccfg_errfile 2>&1

if [ $? -ne 0 ]; then
	print -- "--DIAG: $assertion: Unable to import the services $test_FMRI1"
        print -- "  $test_FMRI2 and $test_FMRI3 error messages from svccfg: "
        print -- "  \"$(cat $svccfg_errfile)\""
	exit $STF_UNRESOLVED
fi
service_setup=1

print -- "--INFO: Wait for $test_FMRI3 to come online"
service_wait_state $test_FMRI3 online
if [ $? -ne 0 ]; then
	print -- "--DIAG: $assertion: Service $test_FMRI3 didn't come online"
	exit $STF_FAIL
fi

print -- "--INFO: Wait for $test_FMRI2 to come online"
service_wait_state $test_FMRI2 online
if [ $? -ne 0 ]; then
	print -- "--DIAG: $assertion: Service $test_FMRI2 didn't come online"
	exit $STF_FAIL
fi

print -- "--INFO: Wait for $test_FMRI1 to come online"
service_wait_state $test_FMRI1 online
if [ $? -ne 0 ]; then
	print -- "--DIAG: $assertion: Service $test_FMRI1 didn't come online"
	exit $STF_FAIL
fi

echo "--INFO: Sending service $test_FMRI1 to maintenance"
svcadm mark maintenance $test_FMRI1
if [ $? -ne 0 ]; then
	echo "--DIAG: $assertion: Service $test_FMRI1 didn't set"
	echo "  maintenance mode"
	exit $STF_FAIL
fi

echo "--INFO: Verify maintenance mode for $test_FMRI1"
service_wait_state $test_FMRI1 maintenance
if [ $? -ne 0 ]; then
	echo "--DIAG: $assertion: service $test_FMRI1 didn't enter"
	echo "  maintenance"
	exit $STF_FAIL
fi

echo "--INFO: Verify offline for $test_FMRI2"
service_wait_state $test_FMRI2 offline
if [ $? -ne 0 ]; then
	echo "--DIAG: $assertion: service $test_FMRI2 didn't enter offline"
	exit $STF_FAIL
fi

echo "--INFO: Verify offline for $test_FMRI3"
service_wait_state $test_FMRI3 offline
if [ $? -ne 0 ]; then
	echo "--DIAG: $assertion: service $test_FMRI3 didn't enter offline"
	exit $STF_FAIL
fi

echo "--INFO: clear maintenance of service $test_FMRI1"
svcadm clear $test_FMRI1
if [ $? -ne 0 ]; then
	echo "--DIAG: $assertion: service $test_FMRI1 would not clear state"
	exit $STF_UNRESOLVED
fi

echo "--INFO: Waiting for $test_FMRI1 to enter online"
service_wait_state $test_FMRI1 online
if [ $? -ne 0 ]; then
	echo "--DIAG: $assertion: service $test_FMRI1 didn't go online"
	exit $STF_FAIL
fi

echo "--INFO: Waiting for $test_FMRI2 to enter online"
service_wait_state $test_FMRI2 online
if [ $? -ne 0 ]; then
	echo "--DIAG: $assertion: service $test_FMRI2 didn't go online"
	exit $STF_FAIL
fi

echo "--INFO: Waiting for $test_FMRI3 to enter online"
service_wait_state $test_FMRI3 online
if [ $? -ne 0 ]; then
	echo "--DIAG: $assertion: service $test_FMRI3 didn't go online"
	exit $STF_FAIL
fi

print -- "--INFO: Checking that startd didn't crash during this test"
typeset endpid=$(pgrep -z `zonename` svc.startd)
typeset -i count=$(set -- $endpid; echo $#)

if [ $count -ne 1 ]; then
	print -- "--DIAG: there are $count instances of svc.startd running"
	[ $count -ne 0 ] && print -- "  pids are: $startdpid"
	print -- "  There should be only one(1) svc.startd instance"
	exit $STF_UNRESOLVED
fi
if [ $startdpid != $endpid ]; then
	print -- "--DIAG: svc.startd changed pid during execution"
	print "  it was $startpid, it is now $endpid"
	exit $STF_UNRESOLVED
fi

print -- "--INFO: Cleaning up service"
cleanup

exit $STF_PASS
