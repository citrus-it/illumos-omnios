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
# ASSERTION: svccfg_import_001
#
# DESCRIPTION:
#	Calling the "import file" subcommand where the file contains a
#	valid service manifest will result in the service and instances
#	specified within the file being enabled, if the services are
#	specified to be enabled.  If no errors have occurred during
#	processing, there is nothing seen on stderr and the
#	command exit status is 0.
#
# STRATEGY:
#	This is a simple test - import a manifest file with a enabled
#	service.  The service used is the standard test service.
#	To verify make sure that the service is in the repository
#	and the test service is running.
#
#	Note: the DTD tests do testing of the import command.  This
#	  test is a simple positive test.  For addition import
#	  testing run the DTD tests.
#	
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

	manifest_purgemd5 $registration_file

	service_cleanup ${TEST_SERVICE}

	service_exists ${TEST_SERVICE}
	[[ $? -eq 0 ]] && {
		echo "--DIAG: [${assertion}, cleanup]
		service ${TEST_SERVICE} should not exist in 
		repository after being deleted, but does"

		RESULT=$(update_result $STF_UNRESOLVED $RESULT)
	}

	rm -f $OUTFILE $ERRFILE $LOGFILE $STATEFILE $registration_file

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

assertion=svccfg_import_001

# extract and print assertion information from this source script.
extract_assertion_info $ME

# Before starting make sure that the test service doesn't already exist.
# If it does then consider it a fatal error.

service_exists $TEST_SERVICE
[[ $? -eq 0 ]] && {
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

readonly registration_template=$MYLOC/import_001.xml
readonly registration_file=/tmp/import_001.xml

readonly LOGFILE=/tmp/log.$$
readonly STATEFILE=/tmp/state.$$

manifest_generate $registration_template \
	TEST_SERVICE=$TEST_SERVICE \
	TEST_INSTANCE=$TEST_INSTANCE \
	SERVICE_APP=$SERVICE_APP  \
 	LOGFILE=$LOGFILE \
	STATEFILE=$STATEFILE >$registration_file


manifest_purgemd5 $registration_file


svccfg  import $registration_file > $OUTFILE 2>$ERRFILE
ret=$?

[[ $ret -ne 0 ]] &&  {
	echo "--DIAG: [${assertion}]
	svccfg import expected to return 0, got $ret"

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

service_wait_state $TEST_SERVICE:$TEST_INSTANCE online 60
[[ $? -ne 0 ]] && {
       	echo "--DIAG: [${assertion}]
	service $TEST_SERVICE should exist in repository but does not"

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
