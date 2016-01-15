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
# ASSERTION: svccfg_add_001
#
# DESCRIPTION:
#      	The 'add name' subcommand adds an entity with the given 
#	name as a child of the current selection.  The entity will 
#	be empty. If no errors have occurred during processing, 
#	there is nothing seen on stderr and the command exit status 
#	is 0.
#
# STRATEGY:
#	This assertion has two sub-tests associated:
#	1) add a service using the command-line
#	2) add an instance using an input file.
#
# end __stf_assertion__
###############################################################################

###############################################################################
# start __stf_assertion__
#
# ASSERTION: svccfg_add_002
#
# DESCRIPTION:
#	Calling the 'add name' subcommand with "name" being an entity 
#	that already exists will return with a diagnostic message 
#	displayed on stderr.  The exit status will be 1.
#
# STRATEGY:
#	This assertion has two sub-tests associated:
#	1) add an already existing service using the command-line
#	2) add an already existing instance using an input file.
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

# make sure that the environment is sane - svc.configd  is up and running
check_gl_env
[[ $? -ne 0 ]] && {
	echo "--DIAG: 
	     	Invalid test environment - svc.configd  not available"

        RESULT=$STF_UNRESOLVED
	exit $RESULT
}

# extract and print assertion information from this source script.
extract_assertion_info $ME

#
# Start assertion testing
#

assertion=svccfg_add_001

#
# Test #1: Add a service
#

echo "--INFO: Starting $assertion, test 1 (add a service)"

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


svccfg add $TEST_SERVICE > $OUTFILE 2>$ERRFILE
ret=$?

# Verify that the return value is as expected - this is a non-fatal error
[[ $ret -ne 0 ]] &&  {
	echo "--DIAG: [${assertion}, test 1]
	svccfg add expected to return 0, got $ret"

	TEST_RESULT=$STF_FAIL

}

# Verify that nothing in stdout - this is a non-fatal error
[[ -s $OUTFILE ]] &&  {
	echo "--DIAG: [${assertion}, test 1]
	stdout not expected, but got $(cat $OUTFILE)"

	TEST_RESULT=$STF_FAIL
}

# Verify that nothing in stderr - this is a non-fatal error
[[ -s $ERRFILE ]] &&  {
	echo "--DIAG: [${assertion}, test 1]
	stderr not expected, but got $(cat $ERRFILE)"

	TEST_RESULT=$STF_FAIL
}

service_exists $TEST_SERVICE
[[ $? -ne 0 ]] && {
	echo "--DIAG: [${assertion}, test 1]
	service $TEST_SERVICE should exist in repository but does not"


	exit $STF_FAIL
}

rm -f $ERRFILE $OUTFILE


print_result $TEST_RESULT
RESULT=$(update_result $TEST_RESULT $RESULT)


#
# Test #2: Add a service/service_instance
#
echo "--INFO: Starting $assertion, test 2 (add a service instance)" 
typeset -i TEST_RESULT=$STF_PASS

cat <<EOF >$CMDFILE
select ${TEST_SERVICE}
add ${TEST_INSTANCE}
EOF

svccfg -f $CMDFILE > $OUTFILE 2>$ERRFILE
ret=$?

# Verify that the return value is as expected - non-fatal error
[[ $ret -ne 0 ]] &&  {
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

# Verify that nothing in stderr - non-fatal error
[[ -s $ERRFILE ]] &&  {
	echo "--DIAG: [${assertion}, test 2]
	stderr not expected, but got $(cat $ERRFILE)"

	TEST_RESULT=$STF_FAIL
}

service_exists ${TEST_SERVICE}:${TEST_INSTANCE}
[[ $? -ne 0 ]] && {
	echo "--DIAG: [${assertion}, test 2]
	service ${TEST_SERVICE}:${TEST_INSTANCE} should exist in 
	repository after being added, but does not"


        RESULT=$(update_result $STF_UNRESOLVED $RESULT)
	exit $RESULT
}

	

rm -f $OUTFILE $ERRFILE $CMDFILE

print_result $TEST_RESULT
RESULT=$(update_result $TEST_RESULT $RESULT)


#
# Start assertion testing
#

assertion=svccfg_add_002

echo "--INFO: Starting $assertion, test 1 (add an existing service)"
typeset -i TEST_RESULT=$STF_PASS

#
# Test #1: Attempt to add an already existing service
#

svccfg add ${TEST_SERVICE}  > $OUTFILE 2>$ERRFILE
ret=$?

# Verify that the return value is as expected - non-fatal error
[[ $ret -ne 1 ]] &&  {
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
if ! egrep -s "$SERVICE_EXISTS_ERRMSG" $ERRFILE
then
	echo "--DIAG: [${assertion}, test 1]
	Expected error message \"$SERVICE_EXISTS_ERRMSG\"
	but got \"$(cat $ERRFILE)\""

	TEST_RESULT=$STF_FAIL
fi


print_result $TEST_RESULT
RESULT=$(update_result $TEST_RESULT $RESULT)

rm -f $ERRFILE $OUTFILE



#
# Test #2: Attempt to add the same service and service/instance that was 
# already added
#

echo "--INFO: Starting $assertion, test 2 (add an existing instance)"
typeset -i TEST_RESULT=$STF_PASS

cat <<EOF >$CMDFILE
select ${TEST_SERVICE}
add ${TEST_INSTANCE}
EOF

svccfg -f $CMDFILE > $OUTFILE 2>$ERRFILE
ret=$?
 
# Verify that the return value is as expected - non-fatal error
[[ $ret -ne 1 ]] &&  {
	echo "--DIAG: [${assertion}, test 2]
	svccfg expected to return 1, got $ret"

	TEST_RESULT=$STF_FAIL
}

# Verify that nothing in stdout - non-fatal error
[[ -s $OUTFILE ]] &&  {
	echo "--DIAG: [${assertion}, test 2]
	stdout not expected, but got $(cat $OUTFILE)"

	TEST_RESULT=$STF_FAIL
}

# Verify that a message is sent to stderr - non-fatal error
if ! egrep -s "$INSTANCE_EXISTS_ERRMSG" $ERRFILE 
then
	echo "--DIAG: [${assertion}, test 2]
	Expected error message \"$INSTANCE_EXISTS_ERRMSG\"
	but got \"$(cat $ERRFILE)\""

	TEST_RESULT=$STF_FAIL
fi

rm -f $ERRFILE $OUTFILE $CMDFILE


print_result $TEST_RESULT
RESULT=$(update_result $TEST_RESULT $RESULT)

exit $RESULT
