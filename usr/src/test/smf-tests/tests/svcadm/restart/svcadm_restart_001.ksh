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
# ASSERTION: svcadm_restart_001
#
# DESCRIPTION:
#	Calling 'svcadm restart FMRI' where FMRI is a service instance that is
#	in the online state will result in the service being stopped and
#	restarted.  The exit status shall be 0.
#
# STRATEGY:
#	- Generate a manifest for a test service.  The start and stop methods
#	  each touch (distinct) files to indicate that they were triggered.
#	- Import the service using svccfg import; this should enable the service
#	- Verify the start method was triggered by confirming that the
#	  start file was touched
#	- remove the start_file
#	- Call svcadm restart FMRI and verify that it exits 0.
#	- Verify that due to svcadm restart, the stop method and start method
#	  were triggered, IN THAT ORDER.  That is, verify that
#		Both the stop file and start file exist, and
#		The start file was created after the stop file
#
# COMMANDS: svcadm(1M)
#
# end __stf_assertion__
################################################################################

# First load up definitions of STF result variables like STF_PASS etc.
. ${STF_TOOLS}/include/stf.kshlib

# Load up definitions of shell functionality common to all smf sub-suites.
. ${STF_SUITE}/include/gltest.kshlib
. ${STF_SUITE}/include/svc.startd_config.kshlib

# Load up common functions for tests in this directory
. ${STF_SUITE}/${STF_EXEC}/functions.kshlib

# Define this test's cleanup function
function cleanup {
	cleanup_leftovers $test_service $start_file $stop_file \
		$start_process $stop_process $registration_file
	print_result $RESULT
}

# Define Variables
readonly assertion=svcadm_restart_001
readonly ME=$(whence -p ${0})
readonly MYLOC=$(dirname ${ME})
readonly registration_template=${STF_SUITE}/tests/svcadm/restart/template.xml
readonly registration_file=/var/tmp/svcadm_restart_001.$$.xml
readonly test_service="smftest_svcadm"
readonly test_instance="$assertion"
readonly start_process="/var/tmp/$assertion.$$.start"
readonly stop_process="/var/tmp/$assertion.$$.stop"
readonly start_file="/var/tmp/$assertion.$$.startfile"
readonly stop_file="/var/tmp/$assertion.$$.stopfile"
readonly fmri="svc:/$test_service:$test_instance"

# Make sure we run as root
if ! /usr/bin/id | grep "uid=0(root)" > /dev/null 2>&1
then
        RESULT=$(update_result $STF_UNRESOLVED $RESULT)
        echo "--DIAG: [$assertion]
        This test must be run from root."
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
	Invalid environment, quitting."
	exit $RESULT
fi

# Verify that the test service template exists
echo "--INFO: [$assertion]
        Verify that required template exists"

if [ ! -s $registration_template ]; then
	echo "--DIAG: [$assertion]
	$registration_template was not found"
	exit $RESULT
fi

# Create the start/stop method scripts for the test service
echo "--INFO: [$assertion]
        Create test processes: $start_process, $stop_process"

cat > $start_process << EOF
#!/bin/ksh -p
/bin/rm -f $start_file >/dev/null 2>&1
sleep 1
/bin/date +\%Y\%m\%d\%H\%M\%S > $start_file
exit 0
EOF

cat > $stop_process << EOF
#!/bin/ksh -p
/bin/rm -f $stop_file >/dev/null 2>&1
/bin/date +\%Y\%m\%d\%H\%M\%S > $stop_file
exit 0
EOF

echo "--INFO: [$assertion]
        chmod a+x $start_process"

chmod a+x $start_process
if [ $? -ne 0 ]; then
	echo "--DIAG: [$assertion]
	chmod a+x $start_process failed"
	exit $RESULT
fi

echo "--INFO: [$assertion]
        chmod a+x $stop_process"

chmod a+x $stop_process
if [ $? -ne 0 ]; then
	echo "--DIAG: [$assertion]
	chmod a+x $stop_process failed"
	exit $RESULT
fi

#
# Generate the customized registration template for the test service
echo "--INFO: [$assertion]
        Generate the registration template (manifest) for the test service"

echo "--INFO: [$assertion]
	The name of service is $test_service"

manifest_generate $registration_template \
        TEST_SERVICE=$test_service \
        TEST_INSTANCE=$test_instance \
        START_NAME=$start_process \
	STOP_NAME="$stop_process" \
        SERVICE_APP=$service_app \
        STATEFILE=$service_state > $registration_file

echo "--INFO: [$assertion]
        Verify the registration template was created and size > 0 bytes"

if [ ! -s $registration_file ]; then
	echo "--DIAG: [$assertion]
	$registration_file does not exist or has size <= 0 bytes"
	exit $RESULT
fi

# Import the test service manifest into the repository
echo "--INFO: [$assertion]
        Import the service to repository using 'svccfg import'"

svccfg import $registration_file > $svccfg_errfile 2>&1
if [ $? -ne 0 ]; then
	echo "--DIAG: [$assertion]
	Unable to import service $test_service
	Error messages from svccfg: \"$(cat $svccfg_errfile)\""
        exit $RESULT
fi

# Import should automatically enable the service
echo "--INFO: [$assertion]
        Wait until $start_process is triggered"


wait_process_start 2>/dev/null
if [ $? -ne 0 ]; then
	echo "--DIAG: [$assertion]
	$start_process not triggered upon import"
	exit $RESULT
fi

# Remove the start_file and call svcadm restart <service> and expect
# that the stop and start event are triggered.  The stop_file should
# touched first, and then the start_file should be touched.

/bin/rm -f $start_file >/dev/null 2>&1
if [ -f $start_file ]; then
	echo "--DIAG: [$assertion]
	Could not remove $start_file"
	exit $RESULT
fi

#
# VERIFY ASSERTION
#
echo "--INFO: [${assertion}]
        restart <$fmri> using svcadm"

svcadm restart $fmri >/dev/null 2>&1
ret=$?
if [ $ret -ne 0 ]; then
        RESULT=$(update_result $STF_FAIL $RESULT)
        echo "--DIAG: [$assertion]
	\"svcadm restart $fmri\" failed
        EXPECTED: ret = 0
        OBSERVED: ret = $ret"
        exit $RESULT
fi

# Verify stop event is triggered.
echo "--INFO: [$assertion]
        Verify that stop method is triggered on restart"

wait_process_stop 2>/dev/null
if [ $? -ne 0 ]; then
	RESULT=$(update_result $STF_FAIL $RESULT)
	echo "--DIAG: [$assertion]
	$stop_process not triggered"
	exit $RESULT
fi

# Verify start event is triggered.
echo "--INFO: [$assertion]
	Verify that start method is triggered on restart" 

wait_process_start 2>/dev/null
if [ $? -ne 0 ]; then
	RESULT=$(update_result $STF_FAIL $RESULT)
	echo "--DIAG: [$assertion]
	$start_process not triggered"
	exit $RESULT
fi

# Wait until the test instance is online
service_wait_state $fmri online
if [ $? -ne 0 ]; then
	RESULT=$(update_result $STF_FAIL $RESULT)
	echo "--DIAG: [$assertion]
	Restart failed for fmri <$fmri>:
		stop and start methods executed, but
		instance did not transition to online state
	EXPECTED: state = online
	OBSERVED: state = `svcprop -p restarter/state $fmri`"
	exit $RESULT
fi

# Verify that the start method was not triggered BEFORE the stop method.
echo "--INFO: [$assertion]
	Verify that on restart stop method is executed before start method"

typeset -i stop_runtime=`cat $stop_file`
typeset -i start_runtime=`cat $start_file`

if [ $start_runtime -le $stop_runtime ]; then
	RESULT=$(update_result $STF_FAIL $RESULT)
	echo "--DIAG: [$assertion]
	Restart error for fmri '$fmri':
	start method was not rerun or was run before stop method
		stop method run time:  $stop_runtime
		start method run time: $start_runtime"
	exit $RESULT
fi

# Disable the test instance
svcadm disable $fmri >/dev/null 2>&1
ret=$?
if [ $ret -ne 0 ]; then
	echo "--DIAG: [$assertion]
		svcadm disable $fmri failed
	EXPECTED: ret = 0
	OBSERVED: ret = $ret"
	print_result $RESULT
	exit $RESULT
fi

# exit, trap calls cleanup
RESULT=$STF_PASS
print_result $RESULT
exit $RESULT

#
### END
#
