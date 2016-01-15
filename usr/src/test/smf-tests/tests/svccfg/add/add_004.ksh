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
# ASSERTION: svccfg_add_004
#
# DESCRIPTION:
#	Calling the 'add name' subcommand with 'name' being invalid
#	will return with a diagnostic message displayed on stderr.
#	The exit status will be 1.
#
# STRATEGY:
#	This assertion has two sub-tests:
#	1) test with an invalid service name
#	2) test with an invalid instance name
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
	
	service_delete $TEST_SERVICE 

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
	Invalid test environment - svc.configd not available"

	RESULT=$STF_UNRESOLVED 
	exit $RESULT
}

# extract and print assertion information from this source script.
extract_assertion_info $ME

assertion=svccfg_add_004


#
# Test #1: Attempt to add a service with an invalid name.  This should fail.
#

echo "--INFO: Starting $assertion, test 1 (add invalid service)"

typeset -i TEST_RESULT=$STF_PASS

svccfg add $INVALID_NAME > $OUTFILE 2>$ERRFILE
ret=$?
[[ $ret -ne 1 ]] && {
	echo "--DIAG: [${assertion}, test 1]
	svccfg expected to return 1, got $ret"

	TEST_RESULT=$STF_FAIL
}


# Verify that nothing in stdout - non-fatal error
[[ -s $OUTFILE ]] &&  {
	echo "--DIAG: [${assertion}, test 1]
	stdout not expected, but got $(cat $OUTFILE)"

	TEST_RESULT=$STF_FAIL
}


# Verify that a message is sent to stderr - non-fatal error
if ! egrep -s "$INVALID_NAME_ERRMSG" $ERRFILE
then
	echo "--DIAG: [${assertion}, test 1]
	Expected error message \"$INVALID_NAME_ERRMSG\"
	but got \"$(cat $ERRFILE)\""

	TEST_RESULT=$STF_FAIL
fi


# Verify that the service was not added
service_exists ${INVALID_NAME}
[[ $? -eq 0 ]] && {
	echo "--DIAG: [${assertion}, test 1]
	Service ${INVALID_NAME} should not exist but it does"

	TEST_RESULT=$STF_FAIL
}

rm -f $ERRFILE $OUTFILE

print_result $TEST_RESULT
RESULT=$(update_result $TEST_RESULT $RESULT)

#
# Test #2: Attempt to add an instance with an invalid name.  This should fail.
#

echo "--INFO: Starting $assertion, test 2 (add invalid instance)"

typeset -i TEST_RESULT=$STF_PASS

cat <<EOF >$CMDFILE
add ${TEST_SERVICE}
select ${TEST_SERVICE}
add ${INVALID_NAME}
EOF

svccfg -f $CMDFILE > $OUTFILE 2>$ERRFILE
ret=$?

[[ $ret -ne 1 ]] && {
	echo "--DIAG: [${assertion}, test 2]
	svccfg expected to return 1, got $ret"

	TEST_RESULT=$STF_FAIL
}


# Verify that nothing in stdout - non-fatal error
[[ -s $OUTFILE ]] &&  {
	echo "--DIAG: [${assertion}, test 2]
	stdout not expected, but got $(cat $OUTFILE)"

	TEST_RESULT==$STF_FAIL
}


# Verify that a message is sent to stderr - non-fatal error
if ! egrep -s "$INVALID_NAME_ERRMSG" $ERRFILE
then
	echo "--DIAG: [${assertion}, test 2]
	Expected error message \"$INVALID_NAME_ERRMSG\"
	but got \"$(cat $ERRFILE)\""

	TEST_RESULT=$STF_FAIL
fi


# Verify that the instance was not added
service_exists ${TEST_SERVICE}:${INVALID_NAME}
[[ $? -eq 0 ]] && {
	echo "--DIAG: [${assertion}, test 2]
	Instance ${TEST_SERVICE}:${INVALID_NAME} should not exist but it does"

	TEST_RESULT=$STF_FAIL
}

rm -f $ERRFILE $OUTFILE

print_result $TEST_RESULT
RESULT=$(update_result $TEST_RESULT $RESULT)


exit $RESULT
