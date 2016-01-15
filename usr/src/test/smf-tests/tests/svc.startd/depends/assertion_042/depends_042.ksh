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
# ASSERTION: depends_042
# DESCRIPTION:
#  Pair of dependent services.
#  If an online service, service_A, has a dependency on another service,
#  service_B The dependency is of type reset_on="error".
#   service_B encounters an error and is transitioned
#  into maintenance mode, then service_A is transitioned offline.
#  Once service_B transitions to online then service_A will transition 
#  to online as well.
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
	rm -f $service_state1 $service_state2
}

trap cleanup 0 1 2 15

readonly ME=$(whence -p ${0})
readonly MYLOC=$(dirname ${ME})

DATA=$MYLOC

readonly registration_template=$DATA/service_042.xml

extract_assertion_info $ME

# make sure that the svc.startd is running
verify_daemon
if [ $? -ne 0 ]; then
	print -- "--DIAG: $assertion: svc.startd is not executing. Cannot "
	print -- "  continue"
	exit $STF_UNRESOLVED
fi

# Make sure the environment is clean - the test service isn't running
print -- "--INFO: Cleanup any old $test_FMRI1, $test_FMRI2 state"
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
	SERVICE_APP=$service_app \
	LOGFILE=$service_log \
	STATEFILE1=$service_state1 \
	STATEFILE2=$service_state2 > $registration_file

print -- "--INFO: Importing service into repository"
manifest_purgemd5 $registration_file
svccfg -v import $registration_file >$svccfg_errfile 2>&1

if [ $? -ne 0 ]; then
	print -- "--DIAG: $assertion: Unable to import the services $test_FMRI1"
        print -- "  and $test_FMRI2 error messages from svccfg: "
        print -- "  \"$(cat $svccfg_errfile)\""
	exit $STF_UNRESOLVED
fi
service_setup=1

print -- "--INFO: Wait for $test_FMRI2 to come online"
service_wait_state $test_FMRI2 online
if [ $? -ne 0 ]; then
	print -- "--DIAG: $assertion: Service $test_FMRI2 didn't come online
	Current state: $(svcprop -p restarter/state $test_FMRI2)"
	exit $STF_UNRESOLVED
fi

thresh=0

print -- "--INFO: Waiting for $test_FMRI1 to come online"
service_wait_state $test_FMRI1 online
if [ $? -ne 0 ]; then
	print -- "--DIAG: $assertion: $test_FMRI1 did not come online
	Current state: $(svcprop -p restarter/state $test_FMRI1)"
	exit $STF_UNRESOLVED
fi

typeset NFAIL=10
print -- "--INFO: Make start method of $test_FMRI1 repeatedly fail $NFAIL times"
while [ $thresh -le $NFAIL ]; do
	# failures are triggered by asking the start method to coredump,
	# as well as by killing the service methods externally
	service_app -s $test_service -i $test_instance1 -m blip \
		-r triggerservicesegv
	for m in start stop; do
		for proc in `echo $(svcs -Hp $test_instance1 | \
			awk '/service_app/ { print $2 }' 2>/dev/null)`; do
			kill -9 $proc 2>/dev/null
		done
	done
	thresh=$((thresh + 1))
done

echo "--INFO: Wait for service to enter maintenance mode"
service_wait_state $test_FMRI1 maintenance
if [ $? -ne 0 ]; then
	echo "--DIAG: $assertion: Service $test_FMRI1 didn't enter "
	echo "  maintenance mode
	Current state: $(svcprop -p restarter/state $test_FMRI1)"
	exit $STF_UNRESOLVED
fi

echo "--INFO: Checking that $test_FMRI2 goes offline"
service_wait_state $test_FMRI2 offline
if [ $? -ne 0 ]; then
	echo "--DIAG: $assertion: Service $test_FMRI2 didn't go offline
	Current state: $(svcprop -p restarter/state $test_FMRI2)"
	exit $STF_FAIL
fi

print -- "--INFO: Cleaning up service"
cleanup

exit $STF_PASS
