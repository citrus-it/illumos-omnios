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
# ASSERTION: svcadm_restart_008
#
# DESCRIPTION:
#	Calling 'svcadm -v restart <FMRI1> <FMRI2>' where FMRI1 and FMRI2 is a 
#	service instance that is in the online state will result in the 
#	service being stopped and restarted. The exit status will be 0 (zero)
#	and a verbose message will be printed indicating the transition.
#
# STRATEGY:
#	- Generate a manifest which starts and stops a process.
#	- Enable it using svccfg import <manifest>
#	- Make sure process is started, verify it whether it touched
#		a expected file say 'foo.start'.
#	- remove the file 'foo.start' now.
#	- Call svcadm restart FMRI1 and FMRI2 and make sure it exits 0.
#	- Also verify the verbose message printed during transition.
#	- Now the above step, should touch a file  say 'foo.stop when
#		trigerrring a stop event and touch a file say 'foo.start'
#		when trigerring a start event.
#	- Verify foo.start and foo.stop exists.
#	- That proves restarts, restarts a process.
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

# Define the test's cleanup function
function cleanup {
	cleanup_leftovers $test_service1 $start_file1 $stop_file1 \
		$start_process1 $stop_process1 $registration_file1
	cleanup_leftovers $test_service2 $start_file2 $stop_file2 \
		$start_process2 $stop_process2 $registration_file2
	print_result $RESULT
}

# Define Variables
readonly assertion=svcadm_restart_008
readonly ME=$(whence -p ${0})
readonly MYLOC=$(dirname ${ME})
readonly registration_template=${STF_SUITE}/tests/svcadm/restart/template.xml
readonly registration_file1=/var/tmp/svcadm_restart_008.$$1.xml
readonly registration_file2=/var/tmp/svcadm_restart_008.$$2.xml
readonly test_service1="smftest_svcadm_1"
readonly test_service2="smftest_svcadm_2"
readonly test_instance="$assertion"
readonly start_process1="/var/tmp/$assertion.$$.start1"
readonly start_process2="/var/tmp/$assertion.$$.start2"
readonly stop_process1="/var/tmp/$assertion.$$.stop1"
readonly stop_process2="/var/tmp/$assertion.$$.stop2"
readonly start_file1="/var/tmp/$assertion.$$.startfile1"
readonly start_file2="/var/tmp/$assertion.$$.startfile2"
readonly stop_file1="/var/tmp/$assertion.$$.stopfile1"
readonly stop_file2="/var/tmp/$assertion.$$.stopfile2"
readonly fmri1="svc:/$test_service1:$test_instance"
readonly fmri2="svc:/$test_service2:$test_instance"
readonly expected_output="/var/tmp/$assertion.$$.expected"
readonly actual_output="/var/tmp/$assertion.$$.actual"

# Make sure we run as root
if ! /usr/bin/id | grep "uid=0(root)" > /dev/null 2>&1
then
        RESULT=$(update_result $STF_UNRESOLVED $RESULT)
        echo "--DIAG: [$assertion]
        This test must be run from root."
        exit $RESULT
fi

# Extract and print assertion from this source script.
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

echo "--INFO: [$assertion]
        Verify that required manifest template exists"

if [ ! -s $registration_template ]; then
	echo "--DIAG: [$assertion]
		$registration_template not found"
	exit $RESULT
fi

echo "--INFO: [$assertion]
        Create test processes in /var/tmp"

echo "--INFO: [$assertion]
	(for $test_service1):
		$start_process1
		$stop_process1"

cat > $start_process1 << EOF
#!/bin/ksh -p
/bin/rm -f $start_file1
sleep 1
/bin/date +\%Y\%m\%d\%H\%M\%S > $start_file1
exit 0
EOF

cat > $stop_process1 << EOF
#!/bin/ksh -p
/bin/rm -f $stop_file1
/bin/date +\%Y\%m\%d\%H\%M\%S > $stop_file1
exit 0
EOF

# Create process scripts for test_service2
echo "--INFO: [$assertion]
	(for $test_service2): 
		$start_process2
		$stop_process2"

cat > $start_process2 << EOF
#!/bin/ksh -p
/bin/rm -f $start_file2
sleep 1
/bin/date +\%Y\%m\%d\%H\%M\%S > $start_file2
exit 0
EOF

cat > $stop_process2 << EOF
#!/bin/ksh -p
/bin/rm -f $stop_file2
/bin/date +\%Y\%m\%d\%H\%M\%S > $stop_file2
exit 0
EOF
echo "--INFO: [$assertion]
        chmod 755 $start_process1"

chmod 755 $start_process1
if [ $? -ne 0 ]; then
	echo "--DIAG: [$assertion]
		chmod 755 /var/tmp/$start_process1 failed"
	exit $RESULT
fi

echo "--INFO: [$assertion]
        chmod 755 $stop_process1"

chmod 755 $stop_process1
if [ $? -ne 0 ]; then
	echo "--DIAG: [$assertion]
		chmod 755 /var/tmp/$stop_process1 failed"
	exit $RESULT
fi

echo "--INFO: [$assertion]
        chmod 755 $start_process2"

chmod 755 $start_process2
if [ $? -ne 0 ]; then
	echo "--DIAG: [$assertion]
		chmod 755 /var/tmp/$start_process2 failed"
	exit $RESULT
fi

echo "--INFO: [$assertion]
        chmod 755 $stop_process2"

chmod 755 $stop_process2
if [ $? -ne 0 ]; then
	echo "--DIAG: [$assertion]
		chmod 755 /var/tmp/$stop_process2 failed"
	exit $RESULT
fi

# Generate the manifest for test_service1
echo "--INFO: [$assertion]
        Generate .xml required for $test_service1  using given template.xml"

manifest_generate $registration_template \
        TEST_SERVICE=$test_service1 \
        TEST_INSTANCE=$test_instance \
        START_NAME=$start_process1 \
	STOP_NAME=$stop_process1 \
        SERVICE_APP=$service_app \
        STATEFILE=$service_state > $registration_file1

# Generate the manifest for test_service2
echo "--INFO: [$assertion]
        Generate .xml required for $test_service2  using given template.xml"

manifest_generate $registration_template \
        TEST_SERVICE=$test_service2 \
        TEST_INSTANCE=$test_instance \
        START_NAME=$start_process2 \
	STOP_NAME=$stop_process2 \
        SERVICE_APP=$service_app \
        STATEFILE=$service_state > $registration_file2

# Verify that non-zero sized manifests were creatd
echo "--INFO: [$assertion]
        Verify that registration template exists and size > 0 bytes"

if [ ! -s $registration_file1 ]; then
    echo "--DIAG: [$assertion]
        $registration_file1 not found or size = 0 bytes"
    exit $RESULT
fi

if [ ! -s $registration_file2 ]; then
    echo "--DIAG: [$assertion]
        $registration_file2 not found or size = 0 bytes"
    exit $RESULT
fi

# Import the services into the repository
echo "--INFO: [$assertion]
        Import $test_service1 into the repository using svccfg"

svccfg import $registration_file1 > /dev/null 2>&1
if [ $? -ne 0 ]; then
        print "--DIAG: $assertion: Unable to import the service $test_service1"
        exit $RESULT
fi

echo "--INFO: [$assertion]
        Import $test_service2 into the repository using svccfg"

svccfg import $registration_file2 > /dev/null 2>&1
if [ $? -ne 0 ]; then
        print "--DIAG: $assertion: Unable to import the service $test_service2"
        exit $RESULT
fi

# Verify that the services were enabled on import
echo "--INFO: [$assertion]
        Services should be enabled on import"

echo "--INFO: [$assertion]
	Wait until $start_process1 is triggered"
wait_process $start_file1 2>/dev/null
if [ $? -ne 0 ]; then
	echo "--DIAG: [$assertion]
		$start_process1 was not started"
	exit $RESULT
fi

echo "--INFO: [$assertion]
        Wait until $start_process2 is triggered."

wait_process $start_file2 2>/dev/null
if [ $? -ne 0 ]; then
	echo "--DIAG: [$assertion]
		$start_process2 not started"
	exit $RESULT
fi

# Remove the $start_files and call svcadm restart. Expect that the stop
# and start methods are triggered, IN THAT ORDER.

/usr/bin/rm -f $start_file1 >/dev/null 2>&1
if [ -f $start_file1 ]; then
	echo "--DIAG: [$assertion]
	EXPECTED: Removed $start_file1
	OBSERVED: Could not remove $start_file1"
	exit $RESULT
fi

/usr/bin/rm -f $start_file2 >/dev/null 2>&1
if [ -f $start_file2 ]; then
	echo "--DIAG: [$assertion]
	EXPECTED: Removed $start_file2
	OBSERVED: Could not remove $start_file2"
	exit $RESULT
fi

cat > $expected_output << EOF
Action restart set for $fmri1.
Action restart set for $fmri2.
EOF

echo "--INFO: [${assertion}]
        restart <$fmri1> <$fmri2> using svcadm"

svcadm -v restart $fmri1 $fmri2 >$actual_output 2>/dev/null
ret=$?
if [ $ret -ne 0 ]; then
        RESULT=$(update_result $STF_FAIL $RESULT)
        echo "--DIAG: [$assertion]
		svcadm -v restart $fmri1 $fmri2 fails
        EXPECTED: ret = 0; output =
`cat $expected_output`
        OBSERVED: ret = $ret; output =
`cat $actual_output`"
        exit $RESULT
fi

diff -w $expected_output $actual_output >/dev/null 2>&1
if [ $? -ne 0 ]; then
        RESULT=$(update_result $STF_FAIL $RESULT)
        echo "--DIAG: [$assertion]
        EXPECTED: `cat $expected_output`
        OBSERVED: `cat $actual_output`"
        exit $RESULT
fi

# Verify that the stop events were triggered

echo "--INFO: [$assertion]
        Wait until $stop_process1 is triggered"

wait_process $stop_file1 2>/dev/null
if [ $? -ne 0 ]; then
	RESULT=$(update_result $STF_FAIL $RESULT)
	echo "--DIAG: [$assertion]
	$stop_process1 was not triggered"
	exit $RESULT
fi

echo "--INFO: [$assertion]
        Wait until $stop_process2 is triggered"

wait_process $stop_file2 2>/dev/null
if [ $? -ne 0 ]; then
	RESULT=$(update_result $STF_FAIL $RESULT)
	echo "--DIAG: [$assertion]
	$stop_process2 was not triggered"
	exit $RESULT
fi

# Verify that the start events were triggered.
echo "--INFO: [$assertion]
        Wait until $start_process1 is invoked"

wait_process $start_file1 2>/dev/null
if [ $? -ne 0 ]; then
	RESULT=$(update_result $STF_FAIL $RESULT)
	echo "--DIAG: [$assertion]
	$start_process1 not invoked"
	exit $RESULT
fi

echo "--INFO: [$assertion]
        Wait until $start_process2 is invoked"

wait_process $start_file2 2>/dev/null
if [ $? -ne 0 ]; then
	RESULT=$(update_result $STF_FAIL $RESULT)
	echo "--DIAG: [$assertion]
	$start_process2  not started"
	exit $RESULT
fi

# Wait until both services are online
service_wait_state $fmri1 online
if [ $? -ne 0 ]; then
	RESULT=$(update_result $STF_FAIL $RESULT)
	echo "--DIAG: [$assertion]
	<$fmri1> is not online"
	print_result $RESULT
	exit $RESULT
fi
service_wait_state $fmri2 online
if [ $? -ne 0 ]; then
	RESULT=$(update_result $STF_FAIL $RESULT)
	echo "--DIAG: [$assertion]
	<$fmri2> is not online"
	print_result $RESULT
	exit $RESULT
fi

# Verify that the start method was not triggered BEFORE the stop method.
typeset -i stop_runtime=`cat $stop_file1`
typeset -i start_runtime=`cat $start_file1`
if [ $start_runtime -le $stop_runtime ]; then
	RESULT=$(update_result $STF_FAIL $RESULT)
	echo "--DIAG: [$assertion]
	Restart error for \$fmri1 ($fmri1):
	start method was not rerun or was run before stop method
		stop method run time:  `cat $stop_file1`
		start method run time: `cat $start_file1`"
	exit $RESULT
fi

stop_runtime=`cat $stop_file2`
start_runtime=`cat $start_file2`
if [ $start_runtime -le $stop_runtime ]; then
	RESULT=$(update_result $STF_FAIL $RESULT)
	echo "--DIAG: [$assertion]
	Restart error on $fmri2:
	start method was not rerun or was run before stop method
		stop method run time:  `cat $stop_file2`
		start method run time: `cat $start_file2`"
	exit $RESULT
fi

# Disable the test instance
svcadm disable $fmri1 $fmri2 >/dev/null 2>&1
ret=$?
if [ $ret -ne 0 ]; then
	echo "--DIAG: [$assertion]
	svcadm disable $fmri1 $fmri2 failed
	EXPECTED: ret = 0
	OBSERVED: ret = $ret"
	exit $RESULT
fi

# exit, trap set to call cleanup
RESULT=$STF_PASS
print_result $RESULT
exit $RESULT

#
### END
#
