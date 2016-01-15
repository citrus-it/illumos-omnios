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
# ASSERTION: svccfg_apply_008
#	Calling the 'apply' subcommand with a valid service profile that
#	was previously applied will yield no changes.  This is not considered
#	an error.
#
# DESCRIPTION:
#
# STRATEGY:
#	
# end __stf_assertion__
###############################################################################


# First STF library
. ${STF_TOOLS}/include/stf.kshlib

# Load GL library
. ${STF_SUITE}/include/gltest.kshlib

# Load svc.startd library for manifest_generate
. ${STF_SUITE}/include/svc.startd_config.kshlib

readonly ME=$(whence -p ${0})
readonly MYLOC=$(dirname ${ME})

# Initialize test result 
typeset -i RESULT=$STF_PASS


function cleanup {
	
	# Note that $TEST_SERVICE may or may not exist so don't check
	# results.  Just make sure the service is gone.

	manifest_purgemd5 $manifest_file

	service_cleanup ${TEST_SERVICE}

	service_exists ${TEST_SERVICE}
	[[ $? -eq 0 ]] && {
		echo "--DIAG: [${assertion}, cleanup]
		service ${TEST_SERVICE} should not exist in 
		repository after being deleted, but does"

		RESULT=$(update_result $STF_UNRESOLVED $RESULT)
	}

	rm -f $OUTFILE $ERRFILE $LOGFILE $STATEFILE $manifest_file $profile_file

	exit $RESULT
}

trap cleanup 0 1 2 15

# make sure that the environment is sane - svc.configd  is up and running
check_gl_env
[[ $? -ne 0 ]] && {
	echo "--DIAG: 
	     	Invalid test environment - svc.configd  not available"

        RESULT=$STF_UNRESOLVED
	exit $RESULT
}

assertion=svccfg_apply_008

# extract and print assertion information from this source script.
extract_assertion_info $ME

# Before starting make sure that the test service doesn't already exist.
# If it does then consider it a fatal error.

service_exists $TEST_SERVICE
ret=$?
[[ $ret -eq 0 ]] && {
	echo "--DIAG: [${assertion}]
	service $TEST_SERVICE should not exist in
	repository but does"

	RESULT=$(update_result $STF_UNRESOLVED $RESULT)
	exit $RESULT
}

# Make sure that no instances of the test service are running.  This
# is to ensure that the subsequent pgrep used to verify the assertion
# does not falsely fail.
#
pgrep -z $(zonename) $(basename $SERVICE_APP) > /dev/null 2>&1
ret=$?
[[ $ret -eq 0 ]] && {
       	echo "--DIAG: [${assertion}]
	an instance of $(basename $SERVICE_APP) is running but should not be
	to ensure correct validation of this test"

	RESULT=$(update_result $STF_UNRESOLVED $RESULT)
	exit $RESULT
}


# # Start assertion testing
#

readonly manifest_template=$MYLOC/apply_001_manifest.xml
readonly manifest_file=/tmp/apply_001_manifest.xml

readonly LOGFILE=/tmp/log.$$
readonly STATEFILE=/tmp/state.$$

manifest_generate $manifest_template \
	TEST_SERVICE=$TEST_SERVICE \
	TEST_INSTANCE=$TEST_INSTANCE \
	SERVICE_APP=$SERVICE_APP  \
 	LOGFILE=$LOGFILE \
	STATEFILE=$STATEFILE | sed 's/ENABLE_VALUE/true/' >$manifest_file

manifest_purgemd5 $manifest_file

svccfg  import $manifest_file > $OUTFILE 2>$ERRFILE
ret=$?
[[ $ret -ne 0 ]] &&  {
	echo "--DIAG: [${assertion}]
	svccfg import failed unexpectedly
	error output is $(cat $ERRFILE)"

	RESULT=$(update_result $STF_UNRESOLVED $RESULT)
	exit $RESULT
}

service_wait_state $TEST_SERVICE:$TEST_INSTANCE online 
ret=$?
[[ $ret -ne 0 ]] && {
       	echo "--DIAG: [${assertion}]
	$TEST_SERVICE:$TEST_INSTANCE did not transition to online as expected" 

	RESULT=$(update_result $STF_UNRESOLVED $RESULT)
	exit $RESULT
}


pgrep -z $(zonename) $(basename $SERVICE_APP) > /dev/null 2>&1
ret=$?
[[ $ret -ne 0 ]] && {
       	echo "--DIAG: [${assertion}]
	app $(basename $SERVICE_APP) is not running but should be"

	RESULT=$(update_result $STF_UNRESOLVED $RESULT)
	exit $RESULT
}

readonly profile_template=$MYLOC/apply_001_profile.xml
readonly profile_file=/tmp/apply_001_profile.xml

manifest_generate $profile_template \
	TEST_SERVICE=$TEST_SERVICE \
	TEST_INSTANCE=$TEST_INSTANCE  | sed 's/ENABLE_VALUE/true/' >$profile_file

svccfg apply $profile_file > $OUTFILE 2>$ERRFILE
ret=$?
[[ $ret -ne 0 ]] &&  {
	echo "--DIAG: [${assertion}]
	svccfg apply expected to return 0, got $ret
	error output is $(cat $ERRFILE)"

	RESULT=$(update_result $STF_UNRESOLVED $RESULT)
	exit $RESULT
}

svccfg apply $profile_file > $OUTFILE 2>$ERRFILE
ret=$?
[[ $ret -ne 0 ]] &&  {
	echo "--DIAG: [${assertion}]
	svccfg apply expected to return 0, got $ret"

	RESULT=$STF_FAIL
}

# Verify that nothing in stdout - this is a non-fatal error
[[ -s $OUTFILE ]] &&  {
	echo "--DIAG: [${assertion}]
	stdout not expected, but got $(cat $OUTFILE)"

	RESULT=$STF_FAIL
}

# Verify that nothing in stderr - this is a non-fatal error
[[ -s $ERRFILE ]] &&  {
	echo "--DIAG: [${assertion}]
	stderr not expected, but got $(cat $ERRFILE)"

	RESULT=$STF_FAIL
}


service_wait_state $TEST_SERVICE:$TEST_INSTANCE online 
ret=$?
[[ $ret -ne 0 ]] && {
       	echo "--DIAG: [${assertion}]
	$TEST_SERVICE:$TEST_INSTANCE did not transition to online as expected" 
	RESULT=$STF_FAIL
}

pgrep -z $(zonename) $(basename $SERVICE_APP) > /dev/null 2>&1
ret=$?
[[ $ret -ne 0 ]] && {
       	echo "--DIAG: [${assertion}]
	app $(basename $SERVICE_APP) is not running but should be"

	RESULT=$STF_FAIL
}

exit $RESULT
