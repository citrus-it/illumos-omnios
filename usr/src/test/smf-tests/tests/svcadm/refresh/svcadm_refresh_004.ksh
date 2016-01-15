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

###############################################################################
# start __stf_assertion__
#
# ASSERTION: svcadm_refresh_004
#
# DESCRIPTION:
#	Calling 'svcadm -v refresh FMRI' where FMRI is a service instance that 
#	is in the disabled state will have no effect on the service.
#	The exit status will be 0.
# STRATEGY:
#	- Create a service instance configuration.
#	- Enable it using svcadm enable
#	- Disable the service instance using svcadm disable.
#	- Call svcadm refresh FMRI and make sure it exits 0.
#	- Also verify it prints the message "Action refresh set for $FMRI"
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

# Load up the common utility functions for tests in this directory
. ${STF_SUITE}/${STF_EXEC}/functions.kshlib

# Define the cleanup function for this test
function cleanup {
	cleanup_leftovers $refresh_test_service $svccfg_add_script
        print_result $RESULT
}

# Define Variables
readonly assertion=svcadm_refresh_004
readonly ME=$(whence -p ${0})
readonly MYLOC=$(dirname ${ME})
readonly refresh_test_service="refresh_004$$"
readonly refresh_test_instance="refresh_004$$"
readonly refresh_test_fmri="svc:/$refresh_test_service:$refresh_test_instance"
readonly svccfg_add_script=/var/tmp/refresh_004$$.cfg

# Make sure we run as root
if ! /usr/bin/id | grep "uid=0(root)" > /dev/null 2>&1
then
        RESULT=$(update_result $STF_UNRESOLVED $RESULT)
        echo "--DIAG: [$assertion]
        This test must be run from root."
        exit $RESULT
fi

# Extract and print the assertion from this source file
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
	exit $RESULT
fi

# Create the svccfg add script which will add entities to the
# repository for this test case.
echo "--INFO: [${assertion}]
        configure $refresh_test_service using svccfg"

cat > $svccfg_add_script <<EOF
add $refresh_test_service
select $refresh_test_service
add $refresh_test_instance
EOF

# Add objects to repository
echo "--INFO: [${assertion}]
        Adding entities <$refresh_test_service> to repository using svccfg"

/usr/sbin/svccfg -f $svccfg_add_script >/dev/null 2>&1
ret=$?
if [ $ret -ne 0 ]; then
        echo "--DIAG: [$assertion]
        Adding entities using svccfg failed.
	EXPECTED: svccfg ret = 0
	OBSERVED: svccfg ret = $ret"
        exit $RESULT
fi

# Enable the test instance using svcadm
echo "--INFO: [${assertion}]
        Enable $refresh_test_fmri"

svcadm enable $refresh_test_fmri >/dev/null 2>&1
ret=$?
if [ $ret -ne 0 ]; then
        echo "--DIAG: [$assertion]
		'svcadm enable $refresh_test_fmri' failed
        EXPECTED: ret = 0
	OBSERVED: ret = $ret"
        exit $RESULT
fi

# Wait for the start method to be triggered
echo "--INFO: [${assertion}]
        Wait until state transition is complete"

service_wait_state $refresh_test_fmri online
if [ $? -ne 0 ]; then
        echo "--DIAG: [$assertion]
        <$refresh_test_fmri> is not online"
        exit $RESULT
fi

# Disable the test instance using svcadm
echo "--INFO: [${assertion}]
        Disable $refresh_test_fmri"

svcadm disable $refresh_test_fmri >/dev/null 2>&1
ret=$?
if [ $ret -ne 0 ]; then
        echo "--DIAG: [$assertion]
	'svcadm disable $refresh_test_fmri' failed
        EXPECTED: ret = 0
	OBSERVED: ret = $ret"
        exit $RESULT
fi

# Wait until state transition is complete
echo "--INFO: [${assertion}]
        Wait until state transition is complete"

service_wait_state $refresh_test_fmri disabled
if [ $? -ne 0 ]; then
        echo "--DIAG: [$assertion]
        <$refresh_test_fmri> is not disabled"
        exit $RESULT
fi

#
# VERIFY ASSERTION
#
output=`svcadm -v refresh $refresh_test_fmri 2>/dev/null`
ret=$?
exp_out="Action refresh set for $refresh_test_fmri."
if [[ "$output" != "$exp_out" || $ret -ne 0 ]]; then
        RESULT=$(update_result $STF_FAIL $RESULT)
        echo "--DIAG: [$assertion]
	'svcadm refresh $refresh_test_fmri' failed
        EXPECTED: ret = 0, output = $exp_out
	OBSERVED: ret = $ret, output = $output"
        exit $RESULT
fi

echo "--INFO: [${assertion}]
        Verify $refresh_test_fmri is still disabled"

state=`svcprop -p restarter/state $refresh_test_fmri 2>/dev/null`
ret=$?
if [[ $ret -ne 0 || "$state" != "disabled" ]]; then
        RESULT=$(update_result $STF_FAIL $RESULT)
        echo "--DIAG: [$assertion]
        EXPECTED: svcprop ret = 0; STATE= disabled
        OBSERVED: svcprop ret = $ret; STATE = $state"
        exit $RESULT
fi

RESULT=$STF_PASS
print_result $RESULT
exit $RESULT

#
### END
#
