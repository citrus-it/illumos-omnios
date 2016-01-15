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
# ASSERTION: svccfg_add_005
#
# DESCRIPTION:
#	The 'add' subcommand accepts one argument - the name of a new
#	entity.  If no arguments or more than one argument are
#	given than a diagnostic message is sent to stderr and an exit
#	status of 1 is returned.
#
# STRATEGY:
#	Check the two tests of this assertions:
#	
#	1) calling 'add' with no arguments.  
#
#	2) calling 'add' with more than one argument. 
#
#	For both tests check that nothng printed on stdout, message on
#	stderr and a return value of 1.
#
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
	
	rm -f $OUTFILE $ERRFILE $CMDFILE
}

trap cleanup 0 1 2 15

# make sure that the environment is sane - svc.configd is up and running
check_gl_env
[[ $? -ne 0 ]] && {
	echo "--DIAG: [$assertion]
	Invalid test environment - svc.configd not available"

        RESULT=$STF_UNRESOLVED 
	exit $RESULT
}

# extract and print assertion information from this source script.
extract_assertion_info $ME

# Assertion ID
readonly assertion=svccfg_add_005

#
# Test #1: "svccfg add" no arguments 
#

echo "--INFO: Starting $assertion, test 1 (svccfg add)"

typeset -i TEST_RESULT=$STF_PASS

svccfg add > $OUTFILE 2>$ERRFILE

ret=$?

# Verify that the return value is as expected - this is a non-fatal error
[[ $ret -ne 1 ]] &&  {
	echo "--DIAG: [${assertion}, test 1]
	svccfg expected to return 1, got $ret"

	TEST_RESULT=$STF_FAIL
}

# Verify that nothing in stdout - this is a non-fatal error
[[ -s $OUTFILE ]] &&  {
	echo "--DIAG: [${assertion}, test 1]
        stdout not expected, but got $(cat $OUTFILE)"

	TEST_RESULT=$STF_FAIL
}

# Verify that a message is sent to stderr
if ! egrep -s "$SYNTAX_ERRMSG" $ERRFILE
then
	echo "--DIAG: [${assertion}, test 1]
	Expected error message \"$SYNTAX_ERRMSG\"
	but got \"$(cat $ERRFILE)\""

	TEST_RESULT=$STF_FAIL
fi
	
rm -f $ERRFILE $OUTFILE

print_result $TEST_RESULT
RESULT=$(update_result $TEST_RESULT $RESULT)

#
# Test #2: more than one argument
#

echo "--INFO: Starting $assertion, test 2 (more than one arg)"
typeset -i TEST_RESULT=$STF_PASS

# Verify that service is not in the repository
service_exists ${NEW_ENTITY}
[[ $? -eq 0 ]] && {
	echo "--DIAG: [${assertion}, test 2]
	service ${NEW_ENTITY} should not be in the 
	repository as a pre-test of the test"

        RESULT=$(update_result $STF_UNRESOLVED $RESULT)
	exit $RESULT

}

cat <<EOF >$CMDFILE
add ${NEW_ENTITY} extra_option 
EOF

svccfg -f $CMDFILE > $OUTFILE 2>$ERRFILE
ret=$?

# Verify that the return value is as expected
[[ $ret -eq 0 ]] &&  {
	echo "--DIAG: [${assertion}, test 2]
	svccfg expected to return 1, got $ret"

	RESULT=$STF_FAIL
}

# Verify that nothing in stdout
[[ -s $OUTFILE ]] &&  {
	echo "--DIAG: [${assertion}, test 2]
        stdout not expected, but got $(cat $OUTFILE)"

	TEST_RESULT=$STF_FAIL
}


# Verify that a message is sent to stderr
if ! egrep -s "$SYNTAX_ERRMSG" $ERRFILE
then
	echo "--DIAG: [${assertion}, test 2]
	Expected error message \"$SYNTAX_ERRMSG\"
	but got \"$(cat $ERRFILE)\""

	TEST_RESULT=$STF_FAIL
fi

# Verify that entity was not added
service_exists ${NEW_ENTITY}
[[ $? -eq 0 ]] && {
	echo "--DIAG: [${assertion}, test 2]
	service ${NEW_ENTITY} was unexpectedly added to repository."

	TEST_RESULT=$STF_FAIL
}


print_result $TEST_RESULT
RESULT=$(update_result $TEST_RESULT $RESULT)

exit $RESULT
