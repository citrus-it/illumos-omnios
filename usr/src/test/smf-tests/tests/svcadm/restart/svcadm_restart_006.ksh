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
# 
# Copyright 2008 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#

###############################################################################
# start __stf_assertion__
#
# ASSERTION: svcadm_restart_006
#
# DESCRIPTION:
#	Calling 'svcadm -v restart FMRI' where FMRI is a service instance that 
#	is in the disabled state will have no effect on the service.
#	The exit status will be 0.
# STRATEGY:
#	- Create a service instance configuration.
#	- Enable it using svcadm enable
#	- Disable the service instance using svcadm disable.
#	- Call svcadm restart FMRI and make sure it exits 0.
#	- Also verify it prints the message "Action restart set for $FMRI"
#	- Verify state of "service instance" is unchanged from disable.
#
# COMMANDS: svcadm(1)
#
# end __stf_assertion__
################################################################################

# First load up definitions of STF result variables like STF_PASS etc.
. ${STF_TOOLS}/include/stf.kshlib

# Load up definitions of shell functionality common to all smf sub-suites.
. ${STF_SUITE}/include/gltest.kshlib
. ${STF_SUITE}/include/svc.startd_config.kshlib

# Load up common functions for tests in this directory
. ${STF_SUITE}/tests/svcadm/restart/functions.kshlib

# Define this test's cleanup function
function cleanup {
	cleanup_leftovers $test_service $svccfg_add_script
}

# Define Variables
readonly assertion=svcadm_restart_006
readonly ME=$(whence -p ${0})
readonly MYLOC=$(dirname ${ME})
readonly test_service="smftest_svcadm"
readonly test_instance="$assertion"
readonly test_fmri="svc:/$test_service:$test_instance"
readonly svccfg_add_script=/var/tmp/restart_006$$.cfg

# Make sure we run as root
if ! /usr/bin/id | grep "uid=0(root)" > /dev/null 2>&1
then
        RESULT=$(update_result $STF_UNRESOLVED $RESULT)
        echo "--DIAG: [$assertion]
        This test must be run from root."
        print_result $RESULT
        exit $RESULT
fi

# gltest.kshlib functions to extract and print assertion information
# from this source script.
extract_assertion_info $ME

# Initialize test result to pass.
typeset -i RESULT=${STF_UNRESOLVED}

# Set a trap to execute the cleanup function
trap cleanup 0 1 2 15

# Exit code for individual commands.
typeset -i tmp_rc=0

# Execute environmental sanity checks.
check_gl_env
tmp_rc=$?
if [[ $tmp_rc -ne 0 ]]
then
	echo "--DIAG: [$assertion]
		Invalid smf environment, quitting."
	print_result $RESULT
	exit $RESULT
fi

# Create the svccfg add script which will add entities to the
# repository for this test case.

echo "--INFO: [${assertion}]
        configure $test_service using svccfg"

cat > $svccfg_add_script <<EOF
add $test_service
select $test_service
add $test_instance
EOF

echo "--INFO: [$assertion]
	The name of service is $test_service"

#Add objects to repository

echo "--INFO: [${assertion}]
        Adding entities <$test_service> to repository using svccfg"

/usr/sbin/svccfg -f $svccfg_add_script >/dev/null 2>&1
if [ $? -ne 0 ]; then
        echo "--DIAG: [$assertion]
        Adding entities using svccfg failed."
        print_result $RESULT
        exit $RESULT
fi


echo "--INFO: [${assertion}]
        Enable svc:/$test_service:$test_instance using svcadm enable"

svcadm enable svc:/$test_service:$test_instance >/dev/null 2>&1
ret=$?
if [ $ret -ne 0 ]; then
        echo "--DIAG: [$assertion]
		svcadm enable svc:/$test_service:$test_instance fails
        EXPECTED: output = ret 0
        ACTUAL: output ret = $ret"
        print_result $RESULT
        exit $RESULT
fi

#Wait just for sometime, let the service STATE gets updated

echo "--INFO: [${assertion}]
        Wait until state transition gets completed"

service_wait_state $test_fmri online
if [ $? -ne 0 ]; then
        echo "--DIAG: [$assertion]
        <$test_service> is not online"
        print_result $RESULT
        exit $RESULT
fi


echo "--INFO: [${assertion}]
        Disable $test_fmri using svcadm disable"

svcadm disable $test_fmri >/dev/null 2>&1
ret=$?
if [ $ret -ne 0 ]; then
        echo "--DIAG: [$assertion]
		svcadm disable $test_fmri fails
        EXPECTED: output = ret 0
        ACTUAL: output ret = $ret"
        print_result $RESULT
        exit $RESULT
fi

#Wait just for sometime, let the service STATE gets updated

echo "--INFO: [${assertion}]
        Wait until state transition gets completed"

service_wait_state $test_fmri disabled
if [ $? -ne 0 ]; then
        echo "--DIAG: [$assertion]
        <$test_service> is not disabled"
        print_result $RESULT
        exit $RESULT
fi

# Restart the test instance
echo "--INFO: [$assertion]
	Restart $test_fmri"

output=`svcadm -v restart svc:/$test_service:$test_instance 2>/dev/null`
ret=$?
if [[ "$output" != "Action restart set for $test_fmri." || $ret -ne 0 ]]; then
        RESULT=$(update_result $STF_FAIL $RESULT)
        echo "--DIAG: [$assertion]
		svcadm restart svc:/$test_service:$test_instance fails
        EXPECTED: output = ret 0
        ACTUAL: output ret = $ret"
        print_result $RESULT
        exit $RESULT
fi

echo "--INFO: [${assertion}]
	svcadm -v restart output = $output"

echo "--INFO: [${assertion}]
        Verify that state is still disabled"

state=`svcprop -p restarter/state $test_fmri 2>/dev/null`
ret=$?
if [[ $ret -ne 0 || "$state" != "disabled" ]]; then
        RESULT=$(update_result $STF_FAIL $RESULT)
        echo "--DIAG: [$assertion]
        EXPECTED: ret = 0; STATE= disabled
        ACTUAL: ret = $ret; STATE = $state"
        print_result $RESULT
        exit $RESULT
fi

RESULT=$STF_PASS
print_result $RESULT
exit $RESULT

#
### END
#
