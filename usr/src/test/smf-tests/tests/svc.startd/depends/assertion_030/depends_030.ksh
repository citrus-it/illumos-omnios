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
# ASSERTION: depends_030
# DESCRIPTION:
#  If a service, service_A, has a dependency on another service,
#  service_B, and service_B has no dependencies. Both are put
#  online. If service_B has a dependency added that causes it to
#  now depend on service_A then service_B will be transitioned to
#  maintenance mode immediately. Any effort to move it out of
#  maintenance mode will fail until it has the dependency removed.
#  (single link circular dependency)
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

readonly registration_template=$DATA/service_030.xml

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
	STATEFILE2=$service_state2 \
	> $registration_file

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

print -- "--INFO: Wait for $test_FMRI2 to go online"
service_wait_state $test_FMRI2 online
if [ $? -ne 0 ]; then
	print -- "--DIAG: $assertion: Service $test_FMRI2 didn't go"
	print -- "  online. It is in " \
		"\"$(svcs -H -o STATE $test_FMRI2)\" state."
	exit $STF_FAIL
fi

print -- "--INFO: Verifying that $test_FMRI1 is in the online state"
service_wait_state $test_FMRI1 online
if [ $? -ne 0 ]; then
        print -- "--DIAG: $assertion: Service $test_FMRI1 did not go to the"
	print -- "  online state. It is in the " \
		"\"$(svcs -H -o STATE $test_FMRI1)\" state."
        exit $STF_FAIL
fi

print -- "--INFO: Adding the dependency from $test_FMRI2 -> $test_FMRI1"
service_dependency_add $test_FMRI2 btoa require_all error $test_FMRI1
if [ $? -ne 0 ]; then
	print -- "--DIAG: Could not add dependency"
	exit $STF_FAIL
fi

print -- "--INFO: Refreshing $test_FMRI2"
svcadm refresh $test_FMRI2
if [ $? -ne 0 ]; then
	print -- "--DIAG: Could not refresh service $test_FMRI2"
	exit $STF_FAIL
fi

typeset inmaint=
typeset notinmaint=

print -- "--INFO: Either $test_FMRI1 or $test_FMRI2 will enter maintenance"
print -- "--INFO: Wait for $test_FMRI1 to enter maintenance state"
service_wait_state $test_FMRI1 maintenance
typeset maint=$?

if [ $maint -ne 0 ]; then
	print -- "--INFO: Wait for $test_FMRI2 to enter maintenance state"
	service_wait_state $test_FMRI2 maintenance
	if [ $? -eq 0 ]; then
		inmaint=$test_FMRI2
		notinmaint=$test_FMRI1
	fi
else
	inmaint=$test_FMRI1
	notimaint=$test_FMRI2
fi

if [ -z "$inmaint" ]; then
	print -- "--DIAG: neither $test_FMRI1 or $test_FMRI2 entered the"
	print -- "  maintenance state"
	print -- "  $test_FMRI1 has the '$(svcs -H -o state $test_FMRI1)' state"
	print -- "  $test_FMRI2 has the '$(svcs -H -o state $test_FMRI2)' state"
	exit $STF_FAIL
fi

print -- "--INFO: Trying to remove maintenance mode while dependency exists"
svcadm clear $inmaint

print -- "--INFO: wait for service to re-enter maintenance mode"
service_wait_state $inmaint maintenance
if [ $? -ne 0 ]; then
	print -- "--DIAG: service $inmaint did not re-enter maintenance state"
	exit $STF_UNRESOLVED
fi

print -- "--INFO: Removing dependency service_b to service_a"
service_dependency_remove $test_FMRI2 btoa
if [ $? -ne 0 ]; then
	print -- "--DIAG: Could not remove dependency"
	exit $STF_FAIL
fi

print -- "--INFO: Refreshing $inmaint"
svcadm refresh $inmaint
if [ $? -ne 0 ]; then
	print -- "--DIAG: Could not refresh service $inmaint"
	exit $STF_UNRESOLVED
fi

echo "--INFO: Clearing maintenance mode in $inmaint"
svcadm clear $inmaint
if [ $? -ne 0 ]; then
	echo "--DIAG: $assertion: Could not clear $inmaint"
	exit $STF_UNRESOLVED
fi

print -- "--INFO: Wait for $inmaint to enter online state"
service_wait_state $inmaint online
if [ $? -ne 0 ]; then
	print -- "--DIAG: service $inmaint did not enter online state"
	exit $STF_FAIL
fi

print -- "--INFO: Cleaning up service"
cleanup

exit $STF_PASS
