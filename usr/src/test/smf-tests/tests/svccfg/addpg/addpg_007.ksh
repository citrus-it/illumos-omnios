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
# ASSERTION: svccfg_addpg_007
#
# DESCRIPTION:
#	The 'addpg' subcommand expects at least two arguments 
#	and no more than three  - name, type and flags.  If less 
#	than two arguments are given or more than three then a 
#	diagnostic message is sent to stderr and an exit status 
#	of 1 is returned.
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
	Invalid test environment - svc.configd not available"

	RESULT=$STF_UNRESOLVED
	exit $RESULT
}

# extract and print assertion information from this source script.
extract_assertion_info $ME

assertion=svccfg_addpg_007



# Before starting make sure that the test service doesn't already exist.
# If it does then consider it a fatal error.
service_exists $TEST_SERVICE
[[ $? -eq 0 ]] && {
	echo "--DIAG: [${assertion}]
	service $TEST_SERVICE should not exist in repository but does"

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
	echo "--DIAG: [${assertion}]
	service $TEST_SERVICE should exist in
	repository after being added, but does not"

	RESULT=$(update_result $STF_UNRESOLVED $RESULT)
	exit $RESULT
}


#
# Test #1: call addpg with one argument
#
echo "--INFO: Starting $assertion, test 1 (one argument)"

typeset -i TEST_RESULT=$STF_PASS

#
# Add a property group to the service
#
cat <<EOF>$CMDFILE
select ${TEST_SERVICE}
addpg ${TEST_PROPERTY}
EOF

svccfg -f $CMDFILE > $OUTFILE 2>$ERRFILE
ret=$?

# Verify that the return value is as expected - non-fatal error
[[ $ret -ne 1 ]] &&  {
	echo "--DIAG: [${assertion}, test 1]
	svccfg addpg expected to return 1, got $ret"

 	TEST_RESULT=$STF_FAIL
}

# Verify that nothing in stdout - non-fatal error
[[ -s $OUTFILE ]] &&  {
	echo "--DIAG: [${assertion}, test 1]
        stdout not expected, but got $(cat $OUTFILE)"

 	TEST_RESULT=$STF_FAIL
}


# Verify that message in stderr - non-fatal error
if ! egrep -s "$SYNTAX_ERRMSG" $ERRFILE
then
	echo "--DIAG: [${assertion}, test 1]
	Expected error message \"$SYNTAX_ERRMSG\"
	but got \"$(cat $ERRFILE)\""

	TEST_RESULT=$STF_FAIL
fi

rm -f $ERRFILE $OUTFILE $CMDFILE
print_result $TEST_RESULT
RESULT=$(update_result $TEST_RESULT $RESULT)


#
# Test #2: call addpg with four arguments
#
echo "--INFO: Starting $assertion, test 2 (4 arguments)"

typeset -i TEST_RESULT=$STF_PASS

#
# Add a property group to the service
#
cat <<EOF>$CMDFILE
select ${TEST_SERVICE}
addpg ${TEST_PROPERTY} astring h extra_option
EOF

svccfg -f $CMDFILE > $OUTFILE 2>$ERRFILE
ret=$?

# Verify that the return value is as expected - non-fatal error
[[ $ret -eq 0 ]] &&  {
	echo "--DIAG: [${assertion}, test 2]
	svccfg addpg expected to return 1, got $ret"

	TEST_RESULT=$STF_FAIL
}

# Verify that nothing in stdout - non-fatal error
[[ -s $OUTFILE ]] &&  {
	echo "--DIAG: [${assertion}, test 2]
        stdout not expected, but got $(cat $OUTFILE)"

	TEST_RESULT=$STF_FAIL
}

# Verify that message in stderr - non-fatal error
if ! egrep -s "$SYNTAX_ERRMSG" $ERRFILE
then
	echo "--DIAG: [${assertion}, test 2]
	Expected error message \"$SYNTAX_ERRMSG\"
	but got \"$(cat $ERRFILE)\""

	TEST_RESULT=$STF_FAIL
fi


print_result $TEST_RESULT
RESULT=$(update_result $TEST_RESULT $RESULT)

#
# Test #3: call addpg with no arguments
#
echo "--INFO: Starting $assertion, test 3 (no arguments)"

typeset -i TEST_RESULT=$STF_PASS

#
# Add a property group to the service
#
cat <<EOF>$CMDFILE
select ${TEST_SERVICE}
addpg
EOF

svccfg -f $CMDFILE > $OUTFILE 2>$ERRFILE
ret=$?

# Verify that the return value is as expected - non-fatal error
[[ $ret -eq 0 ]] &&  {
	echo "--DIAG: [${assertion}, test 3]
	svccfg expected to return 1, got $ret"

	RESULT=$STF_FAIL
}

# Verify that nothing in stdout - non-fatal error
[[ -s $OUTFILE ]] &&  {
	echo "--DIAG: [${assertion}, test 3]
	stdout not expected, but got $(cat $OUTFILE)"

	RESULT=$STF_FAIL
}

# Verify message in stderr - non-fatal error
[[ ! -s $ERRFILE ]] &&  {
	echo "--DIAG: [${assertion}, test 3]
	Expected message in stderr but got nothing"

	RESULT=$STF_FAIL
}


print_result $TEST_RESULT
RESULT=$(update_result $TEST_RESULT $RESULT)

exit $RESULT
