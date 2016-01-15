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
# ASSERTION: svccfg_select_008
#
# DESCRIPTION:
#	The entity name passed to the select command is first treated
#	as a relative name to the current selection.  If this can not
#	be satisfied it will be treated as an absolute name.
#
# STRATEGY:
#	Create a instance name which is the same as a service name,
#	say foo. Test the following conditions:
#	- Call 'select foo' from the command line.  Should select
#	  the service foo.
#	- Call 'select foo' with the current selection being the service
#	  in which the foo instance is defined.  The selection should
#	  be the foo instance.
#	- Call 'select foo' from within "foo" instance.  Should 
#	  select the foo service.
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


typeset COMMON_NAME=foo_$$
typeset OTHER_NAME=dummy
set -A TEST_SERVICE_ARRAY ${TEST_SERVICE} ${COMMON_NAME}
set -A TEST_INSTANCE_ARRAY ${COMMON_NAME} ${OTHER_NAME}

function cleanup {
	
	for index in 0 1
	do
		TEST_SERVICE=${TEST_SERVICE_ARRAY[$index]}

		# Note that $TEST_SERVICE may or may not exist so don't check
		# results.  Just make sure the service is gone.
		service_delete $TEST_SERVICE> /dev/null 2>&1

		service_exists svc://${TEST_SERVICE}
		[[ $? -eq 0 ]] && {
			echo "--DIAG: [${assertion}, cleanup]
		service ${TEST_SERVICE} should not exist in 
		repository after being deleted, but does"

			RESULT=$(update_result $STF_UNRESOLVED $RESULT)
		}
	done

	rm -f $OUTFILE $ERRFILE $CMDFILE

	exit $RESULT
}

trap cleanup 0 1 2 15

# make sure that the environment is sane - svc.configd is up and running
check_gl_env
[[ $? -ne 0 ]] && {
	echo "--DIAG: 
	Invalid test environment -svc.configd not available"

	RESULT=$(update_result $STF_UNRESOLVED $RESULT)
	exit $RESULT
}

# extract and print assertion information from this source script.
extract_assertion_info $ME

assertion=svccfg_select_008

#
# Setup: setup two test services (one called 'foo') and one instance
#	of the first test service (called 'foo').
#

for index in 0 1
do
	TEST_SERVICE=${TEST_SERVICE_ARRAY[$index]}
	TEST_INSTANCE=${TEST_INSTANCE_ARRAY[$index]}

	service_exists svc://$TEST_SERVICE
	[[ $? -eq 0 ]] && {
		echo "--DIAG: [${assertion}, setup]
		service ${TEST_SERVICE} should not exist in
		repository for test to run."

		svccfg list
		RESULT=$(update_result $STF_UNRESOLVED $RESULT)
		exit $RESULT
	}

	cat <<EOF>$CMDFILE
	add ${TEST_SERVICE}
	select ${TEST_SERVICE}
	add ${TEST_INSTANCE}
	end
EOF

	svccfg -f $CMDFILE > $OUTFILE 2>$ERRFILE
	ret=$?
	[[ $ret -ne 0 ]] && {
		echo "--DIAG: [${assertion}, setup] 
		error adding service ${TEST_SERVICE} and service
		instance ${TEST_INSTANCE}, can not continue test
		error output is $(cat $ERRFILE)"

		RESULT=$(update_result $STF_UNRESOLVED $RESULT)
		exit $RESULT
	}
done

# 
# Test #1: Calling 'foo' from comand line
#

echo "--INFO: Starting $assertion, calling \"common\" service"

typeset -i TEST_RESULT=$STF_PASS

cat <<EOF>$CMDFILE
select ${COMMON_NAME}
list
end
EOF

svccfg -f $CMDFILE > $OUTFILE 2>$ERRFILE
ret=$?

# Verify that the return value is as expected - non-fatal error
[[ $ret -ne 0 ]] &&  {
	echo "--DIAG: [${assertion}, test 1]
	svccfg list expected to return 0, got $ret"

	TEST_RESULT=$STF_FAIL
}

# Verify that nothing in stderr - non-fatal error
[[ -s $ERRFILE ]] &&  {
	echo "--DIAG: [${assertion}, test 1]
	stderr not expected, but got $(cat $ERRFILE)"

	TEST_RESULT=$STF_FAIL
}


# Need to verify that the stdout is correct
if ! egrep -s ${OTHER_NAME} $OUTFILE
then
	echo "--DIAG: [${assertion}, test 1]
	Expected ${OTHER_NAME} to be part of output
	but got \"$(cat $OUTFILE)\""

	TEST_RESULT=$STF_FAIL
fi


rm -f $OUTFILE $ERRFILE $CMDFILE
print_result $TEST_RESULT
RESULT=$(update_result $TEST_RESULT $RESULT)

# 
# Test #2: Calling 'foo' instance
#

echo "--INFO: Starting $assertion, calling \"common\" instance"

typeset -i TEST_RESULT=$STF_PASS

cat <<EOF >$CMDFILE
select ${TEST_SERVICE_ARRAY[0]}
select ${COMMON_NAME}
list
end
EOF

svccfg -f $CMDFILE > $OUTFILE 2>$ERRFILE
ret=$?

# Verify that the return value is as expected - non-fatal error
[[ $ret -ne 0 ]] &&  {
	echo "--DIAG: [${assertion}, test 2]
	svccfg list expected to return 0, got $ret"

	TEST_RESULT=$STF_FAIL
}

# Verify that nothing in stdout - non-fatal error
[[ -s $ERRFILE ]] &&  {
	echo "--DIAG: [${assertion}, test 2]
	stderr not expected, but got $(cat $ERRFILE)"

	TEST_RESULT=$STF_FAIL
}


# Need to verify that the stdout is correct
if  egrep -s ${OTHER_NAME} $OUTFILE
then
	echo "--DIAG: [${assertion}, test 2]
	Did not expect ${OTHER_NAME} to be part of output
	but got \"$(cat $OUTFILE)\""

	TEST_RESULT=$STF_FAIL
fi


rm -f $OUTFILE $ERRFILE $CMDFILE
print_result $TEST_RESULT
RESULT=$(update_result $TEST_RESULT $RESULT)


# 
# Test #3: Calling 'foo' service from 'foo' instance
#

echo "--INFO: Starting $assertion, calling \"common\" service (again)"

typeset -i TEST_RESULT=$STF_PASS

cat <<EOF >$CMDFILE
select ${TEST_SERVICE_ARRAY[0]}
select ${COMMON_NAME}
select ${TEST_SERVICE_ARRAY[1]}
list
end
EOF

svccfg -f $CMDFILE > $OUTFILE 2>$ERRFILE
ret=$?
# Verify that the return value is as expected - non-fatal error
[[ $ret -ne 0 ]] &&  {
	echo "--DIAG: [${assertion}, test 3]
	svccfg select expected to return 0, got $ret"

	TEST_RESULT=$STF_FAIL
}

# Verify that nothing in stdout - non-fatal error
[[ -s $ERRFILE ]] &&  {
	echo "--DIAG: [${assertion}, test 3]
	stderr not expected, but got $(cat $ERRFILE)"

	TEST_RESULT=$STF_FAIL
}


# Need to verify that the stdout is correct
if  ! egrep -s ${OTHER_NAME} $OUTFILE
then
	echo "--DIAG: [${assertion}, test 3]
	Expected ${OTHER_NAME} to be part of output
	but got \"$(cat $OUTFILE)\""

	TEST_RESULT=$STF_FAIL
fi


rm -f $OUTFILE $ERRFILE $CMDFILE
print_result $TEST_RESULT
RESULT=$(update_result $TEST_RESULT $RESULT)

