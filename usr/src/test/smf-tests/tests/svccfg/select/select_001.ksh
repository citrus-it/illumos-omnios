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
# ASSERTION: svccfg_select_001
#
# DESCRIPTION:
#      	The subcommand "select name" selects the scope or service
#	specified by the name parameter.  On success no error is
#	seen and the exit status is 0.
#
# STRATEGY:
#	This assertion is verified in many other svccfg test cases.
#	For this test case we'll verify scope by selecting with
#	absolute names vs. relative names.
#
# end __stf_assertion__
###############################################################################


# First STF library
. ${STF_TOOLS}/include/stf.kshlib

# Load GL library
. ${STF_SUITE}/include/gltest.kshlib

# Assertion ID
readonly assertion=svccfg_select_001

readonly ME=$(whence -p ${0})
readonly MYLOC=$(dirname ${ME})

# Initialize test result 
typeset -i RESULT=$STF_PASS

typeset -r  num_entities=10

function cleanup {
	
        # Note that $TEST_SERVICE may or may not exist so don't check
	# results.  Just make sure the service is gone.
	typeset i=1
	while [[ $i -le $num_entities ]]  
	do
		service_delete ${TEST_SERVICE}_$i > /dev/null 2>&1

		service_exists ${TEST_SERVICE}_$i
		[[ $? -eq 0 ]] && {
			echo "--DIAG: [${assertion}, cleanup] 
			service ${TEST_SERVICE} should not exist in
			repository after being deleted, but does"
	
			RESULT=$(update_result $STF_UNRESOLVED $RESULT)
		}
		(( i = i + 1 ))
	done

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

typeset -i index=1
# create the test services/instance for testing
while [ $index -le $num_entities ]
do
	SERVICE=${TEST_SERVICE}_${index}

	service_exists $SERVICE
	[[ $? -eq 0 ]] && {
		echo "--DIAG: [${assertion}, setup]
		service ${SERVICE} should not exist in
		repository for test to run."

		RESULT=$(update_result $STF_UNRESOLVED $RESULT)
		exit $RESULT
	}

	svccfg add $SERVICE > $OUTFILE 2>$ERRFILE
	[[ $? -ne 0 ]] && {
		echo "--DIAG: [${assertion}, setup]
		error adding service ${SERVICE}, can not continue test
		error output is $(cat $ERRFILE)"

		RESULT=$(update_result $STF_UNRESOLVED $RESULT)
		exit $RESULT
	}

	#  Create svccfg file that select an non-existent instance
	i=1
	while [ $i -le $num_entities ] 
	do
		cat <<EOF >>$CMDFILE
		select ${SERVICE}
		add ${SERVICE}_${TEST_INSTANCE}_$i
EOF
		(( i = i + 1 ))
	done

	svccfg -f $CMDFILE > $OUTFILE 2>$ERRFILE
	ret=$?

	# Verify that the return value is as expected - fatal error
	[[ $ret -ne 0 ]] &&  {
		echo "--DIAG: [${assertion}, setup]
		svccfg expected to return 0, got $ret
		error output is $(cat $ERRFILE)"

		RESULT=$(update_result $STF_UNRESOLVED $RESULT)

		exit $RESULT
		
	}

	(( index = index + 1 ))

	rm -f $OUTFILE $ERRFILE $CMDFILE
done


# 
# Now execute the test
#

typeset -i index=1
# create the test services/instance for testing
while [ $index -le $num_entities ]
do

	echo "--INFO: Starting $assertion, test $index"
	typeset -i TEST_RESULT=$STF_PASS

	((nindex = (index % num_entities) + 1))

	SERVICE=${TEST_SERVICE}_$index	
	NSERVICE=${TEST_SERVICE}_${nindex}

	#  Create svccfg file that select an non-existent instance
	cat <<EOF >$CMDFILE
	select ${SERVICE}
	select ${NSERVICE}
	list ${NSERVICE}*
	select ${SERVICE}:${SERVICE}_${TEST_INSTANCE}_${index}
	list 
EOF

	svccfg -f $CMDFILE > $OUTFILE 2>$ERRFILE
	ret=$?
	
	# Verify that the return value is as expected - non-fatal error
	[[ $ret -ne 0 ]] &&  {
		echo "--DIAG: [${assertion}, test $index]
		svccfg select expected to return 0, got $ret"

		TEST_RESULT=$STF_FAIL
	}

	# Verify that nothing in stderr - non-fatal error
	[[ -s $ERRFILE ]] &&  {
		echo "--DIAG: [${assertion}, test $index]
		stderr not expected, but got $(cat $ERRFILE)"

		TEST_RESULT=$STF_FAIL
	}

	# Verify the output of the file 
	num_lines=$(egrep -c ":properties" $OUTFILE)
	[[ $num_lines -ne 2 ]] && {
		echo "--DIAG: [${assertion}, test $index]
		expected 2 \":properties\" lines"

		TEST_RESULT=$STF_FAIL
	}

	typeset i=1
	while [ $i -le $num_entities ] 
	do
		num_lines=$(egrep -c "^${NSERVICE}_${TEST_INSTANCE}_${i}$" $OUTFILE)
		[[ $num_lines -ne 1 ]] && {
			echo "--DIAG: [${assertion}, test $index]
	expected list to show ${NSERVICE}_${TEST_INSTANCE}_${i} but did not"
			
			TEST_RESULT=$STF_FAIL
		}
		(( i = i + 1 ))
	done

	num_lines=$(wc -l $OUTFILE | awk '{print $1}')

	(( total_lines = num_entities + 2 ))

	[[ $num_lines -ne $total_lines ]]  && {
		echo "--DIAG: [${assertion}, test $index]
	expected $total_lines, lines but got $num_lines"

		TEST_RESULT=$STF_FAIL
	}
	[[ $TEST_RESULT -eq $STF_FAIL ]]  && {
		echo "--DIAG: Here is the output:
		$(cat $OUTFILE)"
	}
		
	(( index = index + 1 ))

	rm -f $OUTFILE $ERRFILE $CMDFILE

	print_result $TEST_RESULT

	RESULT=$(update_result $TEST_RESULT $RESULT)
done

exit $RESULT
