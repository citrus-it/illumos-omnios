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
# ASSERTION: svccfg_unselect_001
#
# DESCRIPTION:
#	The 'unselect' subcommand causes the parent of the 
#	current selection to become the currently selected 
#	entity.  Upon success, no error is printed to stderr.  
#	In the absence of any other errors during processing, 
#	the exit status will be 0.
#
# STRATEGY:
#	Test unselect from the service and service instance level.
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

typeset readonly SERVICE_LIST=/tmp/sl.$$
typeset readonly SERVICE_LIST_2=/tmp/sl_2.$$
typeset readonly INSTANCE_LIST=/tmp/il.$$
typeset readonly INSTANCE_LIST_2=/tmp/il_2.$$

assertion=svccfg_unselect_001


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


	rm -f $SERVICE_LIST $SERVICE_LIST_2 $INSTANCE_LIST $INSTANCE_LIST_2
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


# Before starting make sure that the test service doesn't already exist.
# If it does then consider it a fatal error.
service_exists $TEST_SERVICE
[[ $? -eq 0 ]] && {
	echo "--DIAG: [${assertion}]
	service $TEST_SERVICE should not exist in repository but does"

	RESULT=$(update_result $STF_UNRESOLVED $RESULT)
	exit $RESULT
}


# Add the service and a service instance
cat << EOF > $CMDFILE
add ${TEST_SERVICE}
select ${TEST_SERVICE}
add ${TEST_INSTANCE}
EOF

svccfg -f $CMDFILE > $OUTFILE 2>$ERRFILE
ret=$?
[[ $? -ne 0 ]] && {
	echo "--DIAG: [${assertion}]
	error setting up service $TEST_SERVICE and 
	$instance TEST_INSTANCE; here is error output:
	$(cat $ERRFILE)"

	RESULT=$STF_UNRESOLVED 
	exit $RESULT
}

# Capture the listing of the service
svccfg list > $SERVICE_LIST 2>$ERRFILE
ret=$?
[[ $? -ne 0 ]] && {
	echo "--DIAG: [${assertion}]
	error getting listing from service $TEST_SERVICE;
	here is error output:
	$(cat $ERRFILE)"

	RESULT=$STF_UNRESOLVED 
	exit $RESULT
}
		

# Capture the listing of the service instance
cat << EOF > $CMDFILE
select ${TEST_SERVICE}
list 
EOF

svccfg -f $CMDFILE > $INSTANCE_LIST 2>$ERRFILE
ret=$?
[[ $? -ne 0 ]] && {
	echo "--DIAG: [${assertion}]
	error getting listing from instance $TEST_INSTANCE;
	here is error output:
	$(cat $ERRFILE)"

	RESULT=$STF_UNRESOLVED 
	exit $RESULT
}
		
echo "--INFO: Starting $assertion, test 1 (unselect from instance)"

typeset -i TEST_RESULT=$STF_PASS

# Call unselect from the instance level
cat << EOF > $CMDFILE
select ${TEST_SERVICE}
select ${TEST_INSTANCE}
unselect 
list 
EOF

svccfg -f $CMDFILE > ${INSTANCE_LIST_2} 2>$ERRFILE
ret=$?
[[ $? -ne 0 ]] && {
	echo "--DIAG: [${assertion}, test 1]
	error getting listing from instance $TEST_INSTANCE;
	here is error output:
	$(cat $ERRFILE)"

	TEST_RESULT=$STF_FAIL
}

# Verify that nothing in stderr - this is a non-fatal error
[[ -s $ERRFILE ]] &&  {
	echo "--DIAG: [${assertion}, test 1]
	stderr not expected, but got $(cat $ERRFILE)"

	TEST_RESULT=$STF_FAIL
}


diff ${INSTANCE_LIST} ${INSTANCE_LIST_2}
[[ $? -ne 0 ]] && {
	echo "--DIAG: [${assertion}, test 1]
	error in instance unselect"

	TEST_RESULT=$STF_FAIL
}

print_result $TEST_RESULT
RESULT=$(update_result $TEST_RESULT $RESULT)


echo "--INFO: Starting $assertion, test 2 (unselect from service)"

typeset -i TEST_RESULT=$STF_PASS


# Call unselect from the service level
cat << EOF > $CMDFILE
select ${TEST_SERVICE}
unselect 
list 
EOF

svccfg -f $CMDFILE > ${SERVICE_LIST_2} 2>$ERRFILE
ret=$?
[[ $? -ne 0 ]] && {
	echo "--DIAG: [${assertion}, test 2]
	error getting listing from instance $TEST_SERVICE;
	here is error output:
	cat $(ERRFILE)"

	TEST_RESULT=$STF_FAIL
}

# Verify that nothing in stderr - this is a non-fatal error
[[ -s $ERRFILE ]] &&  {
	echo "--DIAG: [${assertion}]
	stderr not expected, but got $(cat $ERRFILE)"

	TEST_RESULT=$STF_FAIL
}

diff ${SERVICE_LIST} ${SERVICE_LIST_2}
[[ $? -ne 0 ]] && {
	echo "--DIAG: [${assertion}]
	error in service unselect"

	TEST_RESULT=$STF_FAIL
}

print_result $TEST_RESULT
RESULT=$(update_result $TEST_RESULT $RESULT)

exit $RESULT
