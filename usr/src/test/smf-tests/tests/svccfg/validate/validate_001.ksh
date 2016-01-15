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
# ASSERTION: svccfg_validate_001
#
# DESCRIPTION:
#	Calling the "validate file" subcommand where the file 
#	contains a valid manifest will validate the file.  No 
#	changes will be made to the repository, nothing seen on 
#	stderr and the command exit status is 0.
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

assertion=svccfg_validate_001

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
pgrep $(basename $SERVICE_APP) > /dev/null 2>&1
ret=$?
[[ $ret -eq 0 ]] && {
       	echo "--DIAG: [${assertion}]
	an instance of $(basename SERVICE_APP) is running but should not be
	to ensure correct validation of this test"

	RESULT=$(update_result $STF_UNRESOLVED $RESULT)
	exit $RESULT
}

#
#  Start assertion testing
#

# Test #1: validate a good xml file

typeset -i TEST_RESULT=$STF_PASS

echo "--INFO: Validate a good service bundle"

registration_template=$MYLOC/validate_001_good.xml
registration_file=/tmp/validate_001_good.xml

readonly LOGFILE=/tmp/log.$$
readonly STATEFILE=/tmp/state.$$

manifest_generate $registration_template \
	TEST_SERVICE=$TEST_SERVICE \
	TEST_INSTANCE=$TEST_INSTANCE \
	SERVICE_APP=$SERVICE_APP  \
 	LOGFILE=$LOGFILE \
	STATEFILE=$STATEFILE >$registration_file


manifest_purgemd5 $registration_file

svccfg  validate $registration_file > $OUTFILE 2>$ERRFILE
ret=$?

[[ $ret -ne 0 ]] &&  {
	echo "--DIAG: [${assertion}]
	svccfg validate expected to return 0, got $ret"

	TEST_RESULT=$STF_FAIL
}

# Verify that nothing in stdout - this is a non-fatal error
[[ -s $OUTFILE ]] &&  {
	echo "--DIAG: [${assertion}]
	stdout not expected, but got $(cat $OUTFILE)"

	TEST_RESULT=$STF_FAIL
}

# Verify that nothing in stderr - this is a non-fatal error
[[ -s $ERRFILE ]] &&  {
	echo "--DIAG: [${assertion}]
	stderr not expected, but got $(cat $ERRFILE)"

	TEST_RESULT=$STF_FAIL
}

service_exists $TEST_SERVICE
[[ $? -eq 0 ]] && {
       	echo "--DIAG: [${assertion}]
	service $TEST_SERVICE should not exist in repository but does not"

	TEST_RESULT=$STF_FAIL
}


pgrep $(basename $SERVICE_APP) > /dev/null 2>&1
ret=$?
[[ $ret -eq 0 ]] && {
       	echo "--DIAG: [${assertion}]
	app $(basename SERVICE_APP) is running but should not be"

	TEST_RESULT=$STF_FAIL
}

print_result $TEST_RESULT
RESULT=$(update_result $TEST_RESULT $RESULT)

# Test #2: validate a bad xml file

typeset -i TEST_RESULT=$STF_PASS

echo "--INFO: Validate a bad service bundle"

registration_template=$MYLOC/validate_001_bad.xml
registration_file=/tmp/validate_001_bad.xml

manifest_generate $registration_template \
	TEST_SERVICE=$TEST_SERVICE \
	TEST_INSTANCE=$TEST_INSTANCE \
	SERVICE_APP=$SERVICE_APP  \
 	LOGFILE=$LOGFILE \
	STATEFILE=$STATEFILE >$registration_file


manifest_purgemd5 $registration_file

svccfg  validate $registration_file > $OUTFILE 2>$ERRFILE
ret=$?
[[ $ret -ne 1 ]] &&  {
	echo "--DIAG: [${assertion}]
	svccfg validate expected to return 1, got $ret"

	TEST_RESULT=$STF_FAIL
}

# Verify that nothing in stdout - this is a non-fatal error
[[ -s $OUTFILE ]] &&  {
	echo "--DIAG: [${assertion}]
	stdout not expected, but got $(cat $OUTFILE)"

	TEST_RESULT=$STF_FAIL
}

# Verify that nothing in stderr - this is a non-fatal error
if ! egrep -s "$UNPARSEABLE_ERRMSG" $ERRFILE
then
	echo "--DIAG: [${assertion}]
	Expected error message \"$UNPARSEABLE_ERRMSG\"
	but got \"$(cat $ERRFILE)\""

	RESULT=$STF_FAIL
fi

service_exists $TEST_SERVICE
[[ $? -eq 0 ]] && {
       	echo "--DIAG: [${assertion}]
	service $TEST_SERVICE should not exist in repository but does not"

	TEST_RESULT=$STF_FAIL
}


pgrep $(basename $SERVICE_APP) > /dev/null 2>&1
ret=$?
[[ $ret -eq 0 ]] && {
       	echo "--DIAG: [${assertion}]
	app $(basename SERVICE_APP) is running but should not be"

	TEST_RESULT=$STF_FAIL
}

print_result $TEST_RESULT
RESULT=$(update_result $TEST_RESULT $RESULT)



exit $RESULT
