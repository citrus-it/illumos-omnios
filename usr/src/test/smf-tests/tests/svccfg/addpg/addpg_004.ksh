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
# ASSERTION: svccfg_addpg_004
#
# DESCRIPTION:
#	If an invalid name is given to the 'addpg' subcommand
#	then a diagnostic message is sent to stderr and the return
#	status is set to 1.
#
# STRATEGY:
#	This assertion has two subtests:
#	1) add an invalid property group to a service
#	2) add an invalid property group to an instance
#
#	Both of these subtests is performed from a svccfg command
#	file.
#
# end __stf_assertion__
###############################################################################


# First STF library
. ${STF_TOOLS}/include/stf.kshlib

# Load GL library
. ${STF_SUITE}/include/gltest.kshlib


readonly ME=$(whence -p ${0})
readonly MYLOC=$(dirname ${ME})

# Initialize test result 
typeset -i RESULT=$STF_PASS


function cleanup {
	
	# Note that $TEST_SERVICE may or may not exist so don't check
	# results.  Just make sure the service is gone.
	service_delete $TEST_SERVICE > /dev/null 2>&1

	service_exists ${TEST_SERVICE}
	[[ $? -eq 0 ]] && {
		echo "--DIAG: [${assertion}, cleanup]
		service ${TEST_SERVICE} should not exist in 
		repository after being deleted, but does"

		RESULT=$(update_result $STF_UNRESOLVED $RESULT)
	}

	rm -f $OUTFILE $ERRFILE $CMDFILE

	exit $RESULT
}

trap cleanup 0 1 2 15

# make sure that the environment is sane - svc.configd is up and running
check_gl_env
[[ $? -ne 0 ]] && {
	echo "--DIAG: 
	Invalid test environment - svc.configd is not available"

	RESULT=$STF_UNRESOLVED
	exit $RESULT
}

# extract and print assertion information from this source script.
extract_assertion_info $ME

assertion=svccfg_addpg_004

# 
# Test #1: Add a property group to a service
#

echo "--INFO: Starting $assertion, test 1 (add an invalid pg to a service)"

typeset -i TEST_RESULT=$STF_PASS


# Before starting make sure that the test service doesn't already exist.
# If it does then consider it a fatal error.
service_exists $TEST_SERVICE
[[ $? -eq 0 ]] && {
	echo "--DIAG: [${assertion}, test 1]
	service $TEST_SERVICE should not exist in 
	repository but does"

	RESULT=$(update_result $STF_UNRESOLVED $RESULT)
	exit $RESULT
}


#
# Add the service.  If this fails consider it a fatal error
#
svccfg add $TEST_SERVICE > $OUTFILE 2>$ERRFILE
ret=$?
[[ $ret -ne 0 ]] && {
	echo "--DIAG: [${assertion}]
	error adding service $TEST_SERVICE needed for test
	error output is $(cat $ERRFILE)"

	RESULT=$(update_result $STF_UNRESOLVED $RESULT)
	exit $RESULT
}

# Make sure the service is there
service_exists $TEST_SERVICE
[[ $? -ne 0 ]] && {
	echo "--DIAG: [${assertion}, test 1]
	service $TEST_SERVICE should exist in
	repository after being added, but does not"

	RESULT=$(update_result $STF_UNRESOLVED $RESULT)
	exit $RESULT
}


#
# Add an invalid  property group to the service
#
cat <<EOF>$CMDFILE
select ${TEST_SERVICE}
addpg ${INVALID_NAME} astring
EOF

svccfg -f $CMDFILE > $OUTFILE 2>$ERRFILE
ret=$?

# Verify that the return value is as expected - non-fatal error
[[ $ret -ne 1 ]] &&  {
	echo "--DIAG: [${assertion}, test 1]
	svccfg expected to return 0, got $ret"

	TEST_RESULT=$STF_FAIL
}

# Verify that nothing in stdout - non-fatal error
[[ -s $OUTFILE ]] &&  {
	echo "--DIAG: [${assertion}, test 1]
        stdout not expected, but got $(cat $OUTFILE)"

	TEST_RESULT=$STF_FAIL
}

# Verify message to stderr - non-fatal error
if ! egrep -s "$INVALID_ERRMSG" $ERRFILE
then
	echo "--DIAG: [${assertion}, test 1]
	Expected error message \"$INVALID_ERRMSG\"
	but got \"$(cat $ERRFILE)\""

	TEST_RESULT=$STF_FAIL
fi


#
# Check that the property group does not exist - fatal error
#
svcprop -p $INVALID_NAME $TEST_SERVICE > /dev/null 2>&1
ret=$?
[[ $ret -eq 0 ]] &&  {
	echo "--DIAG: [${assertion}, test 1]
	property group $INVALID_NAME does not exist"

	TEST_RESULT=$STF_FAIL
}


rm -f $ERRFILE $OUTFILE $CMDFILE

print_result $TEST_RESULT
RESULT=$(update_result $TEST_RESULT $RESULT)

# 
# Test #2: Add a invalid property group to a service instance
#

echo "--INFO: Starting $assertion, test 2 (add an invalid pg to an instance)"

typeset -i TEST_RESULT=$STF_PASS

# 
# Create the instance - if this fails it's a fatal error
#
cat <<EOF >$CMDFILE
select ${TEST_SERVICE}
add ${TEST_INSTANCE}
EOF

svccfg -f $CMDFILE > $OUTFILE 2>$ERRFILE
ret=$?

[[ $ret -ne 0 ]] && {
	echo "--DIAG: [${assertion}, test 2]
	svccfg add expected to return 0, got $ret
	error output is $(cat $ERRFILE)"
	
	RESULT=$(update_result $STF_UNRESOLVED $RESULT)
	exit $RESULT
}
				       
#
# Add an invalid property group to the service instance
#
cat <<EOF>$CMDFILE
select ${TEST_SERVICE}
select ${TEST_INSTANCE}
addpg ${INVALID_NAME} astring
EOF

svccfg -f $CMDFILE > $OUTFILE 2>$ERRFILE
ret=$?

# Verify that the return value is as expected - non-fatal error
[[ $ret -eq 0 ]] &&  {
	echo "--DIAG: [${assertion}, test 2]
	svccfg add expected to return 0, got $ret"

	TEST_RESULT=$STF_FAIL
}

# Verify that nothing in stdout - non-fatal error
[[ -s $OUTFILE ]] &&  {
	echo "--DIAG: [${assertion}, test 2]
        stdout not expected, but got $(cat $OUTFILE)"

	TEST_RESULT=$STF_FAIL
}

# Verify that message in stderr - non-fatal error
if ! egrep -s "$INVALID_ERRMSG" $ERRFILE
then
	echo "--DIAG: [${assertion}, test 1]
	Expected error message \"$INVALID_ERRMSG\"
	but got \"$(cat $ERRFILE)\""

	TEST_RESULT=$STF_FAIL
fi

#
# Check that the property group does not exist - fatal error
#
svcprop -p $INVALID_NAME $TEST_SERVICE > /dev/null 2>&1
ret=$?
[[ $ret -eq 0 ]] &&  {
	echo "--DIAG: [${assertion}, test 2]
	property group $INVALID_NAME does not exist"

	TEST_RESULT=$STF_FAIL
}


rm -f $ERRFILE $OUTFILE $CMDFILE

print_result $TEST_RESULT
RESULT=$(update_result $TEST_RESULT $RESULT)

exit $RESULT
