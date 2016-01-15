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

function cleanup {
	service_cleanup $test_service
	/usr/bin/rm -f $svccfg_add_script
}

###############################################################################
# start __stf_assertion__
#
# ASSERTION: svcadm_degraded_001
#
# DESCRIPTION:
#	Calling 'svcadm mark degraded FMRI' where FMRI is a service 
#	instance that is in the disabled state should fail with exit 1.
# STRATEGY:
#	- Create a service instance configuration.
#	- Enable it using svcadm enable
#	- Disable the service instance using svcadm disable.
#	- Call svcadm mark degraded FMRI and make sure it exits 1
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

# Define Variables
readonly assertion=svcadm_degraded_001
readonly ME=$(whence -p ${0})
readonly MYLOC=$(dirname ${ME})
readonly test_service="degraded_001$$"
readonly test_instance="degraded_001$$"
readonly svccfg_add_script=/var/tmp/degraded_001$$.cfg
readonly test_fmri="svc:/$test_service:$test_instance"

# Make sure we run as root
if ! /usr/bin/id | grep "uid=0(root)" > /dev/null 2>&1
then
        RESULT=$(update_result $STF_UNRESOLVED $RESULT)
        echo "--DIAG: [$assertion]
        This test must be run from root."
        print_result $RESULT
        exit $RESULT
fi

# Extract and print assertion information from this source script.
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
        configure test service $test_service using svccfg"

cat > $svccfg_add_script <<EOF
add $test_service
select $test_service
add $test_instance
quit
EOF

echo "/usr/sbin/svccfg 
`cat $svccfg_add_script`"

#
# Add objects to repository
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
        Enable $test_fmri using svcadm enable"

svcadm enable $test_fmri >/dev/null 2>&1
ret=$?
if [ $ret -ne 0 ]; then
        echo "--DIAG: [$assertion]
		svcadm enable $test_fmri failed
        EXPECTED: ret = 0
        OBSERVED: ret = $ret"
        print_result $RESULT
        exit $RESULT
fi


# Wait until state transition is completed
target_state="online"

echo "--INFO: [${assertion}]
        Wait until transition to state $target_state is completed"

service_wait_state $test_fmri $target_state
if [ $? -ne 0 ]; then
        echo "--DIAG: [$assertion]
        <$test_fmri> is not $target_state"
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
        EXPECTED: ret = 0
        OBSERVED: ret = $ret"
        print_result $RESULT
        exit $RESULT
fi

# Wait until transition to disabled is completed
target_state="disabled"

echo "--INFO: [${assertion}]
        Wait until transition to $target_state is completed"

service_wait_state $test_fmri $target_state
if [ $? -ne 0 ]; then
        echo "--DIAG: [$assertion]
        <$test_fmri> is not $target_state"
        print_result $RESULT
        exit $RESULT
fi


echo "--INFO: [${assertion}]
        Verify that state = disabled"

state=`svcprop -p restarter/state $test_fmri 2>/dev/null`
ret=$?
if [[ $ret -ne 0 || "$state" != "disabled" ]]; then
        RESULT=$(update_result $STF_FAIL $RESULT)
        echo "--DIAG: [$assertion]
        EXPECTED: ret = 0; STATE= disabled
        OBSERVED: ret = $ret; STATE = $state"
        print_result $RESULT
        exit $RESULT
fi

svcadm mark degraded $test_fmri >/dev/null 2>&1
ret=$?
if [ $ret -ne 1 ]; then
        RESULT=$(update_result $STF_FAIL $RESULT)
        echo "--DIAG: [$assertion]
		<svcadm mark degraded $test_fmri> fails
        EXPECTED: ret = 1
        OBSERVED: ret = $ret"
        print_result $RESULT
        exit $RESULT
fi

echo "--INFO: [${assertion}]
        Verify that state = disabled"

state=`svcprop -p restarter/state $test_fmri 2>/dev/null`
ret=$?
if [[ $ret -ne 0 || "$state" != "disabled" ]]; then
        RESULT=$(update_result $STF_FAIL $RESULT)
        echo "--DIAG: [$assertion]
        EXPECTED: ret = 0; STATE= disabled
        OBSERVED: ret = $ret; STATE = $state"
        print_result $RESULT
        exit $RESULT
fi

RESULT=$STF_PASS
print_result $RESULT
exit $RESULT
