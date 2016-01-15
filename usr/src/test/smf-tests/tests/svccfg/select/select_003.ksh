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
# ASSERTION: svccfg_select_003
#
# DESCRIPTION:
#	If the (valid) scope or service the name specified in the
#	subcommand "select name" does not exist in the repository,
#	the diagnostic message "Not found." is sent to stderr.  The
#	command exit status will be 1.
#
# STRATEGY:
#
# end __stf_assertion__
###############################################################################


# First STF library
. ${STF_TOOLS}/include/stf.kshlib

# Load GL library
. ${STF_SUITE}/include/gltest.kshlib

# Assertion ID
readonly assertion=svccfg_select_003

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
	print_err "$assertion: invalid test environment"
	exit $STF_UNRESOLVED
}

# extract and print assertion information from this source script.
extract_assertion_info $ME


# 
# Test #1: Non-existent service
#
echo "--INFO: Starting $assertion, test 1 (select non-existent service)"

typeset -i TEST_RESULT=$STF_PASS

service_exists ${TEST_SERVICE}
[[ $? -eq 0 ]] && {
	echo "--DIAG: [${assertion}, test 1]
	service ${TEST_SERVICE} should not exist in
	repository for test to run."

	RESULT=$(update_result $STF_UNRESOLVED $RESULT)
	exit $RESULT
}

svccfg select ${TEST_SERVICE} > $OUTFILE 2>$ERRFILE
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

EMSG="svccfg: Pattern '$TEST_SERVICE' doesn't match any instances or services"

# Verify message to stderr - non-fatal error
if ! egrep -s "$EMSG" $ERRFILE
then
	echo "--DIAG: [${assertion}, test 1]
	Expected error message \"$EMSG\"
	but got \"$(cat $ERRFILE)\""

	TEST_RESULT=$STF_FAIL
fi

rm -f $ERRFILE $OUTFILE 

print_result $TEST_RESULT
RESULT=$(update_result $TEST_RESULT $RESULT)



#
# Test #2: Non-existent instance
#

echo "--INFO: Starting $assertion, test 2 select non-existent instance)"

typeset -i TEST_RESULT=$STF_PASS

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

#  Create svccfg file that select an non-existent instance
cat <<EOF >$CMDFILE
select ${TEST_SERVICE}
select ${TEST_INSTANCE}
end
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

NO_MATCH="doesn't match any instances or services"
EMSG="svccfg ($CMDFILE, line 2): Pattern '$TEST_INSTANCE' $NO_MATCH"

# Verify message to stderr - non-fatal error
if ! grep -sl "$EMSG" $ERRFILE >/dev/null 2>&1
then
	echo "--DIAG: [${assertion}, test 1]
	Expected error message:
$EMSG
	but got
$(cat $ERRFILE)"

	TEST_RESULT=$STF_FAIL
fi

rm -f $ERRFILE $OUTFILE $CMDFILE

print_result $TEST_RESULT
RESULT=$(update_result $TEST_RESULT $RESULT)

exit $RESULT
