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
# ASSERTION: svccfg_addpg_003
#
# DESCRIPTION:
#	The 'addpg name type [flags]' subcommand adds a property 
#	group of name 'name' and type 'type' to the currently 
#	selected entity.  If the flags argument is set to some 
#	value other than "H" or "h" then it is considered invalid 
#	and a diagnostic message is sent to stderr and the return 
#	status is set to 1.  Invalid values of "flags" should 
#	include: character value, string values, non-terminated 
#	strings (e.g. "abc), integer values.
#
#
# STRATEGY:
#	This assertion tests that invalid flags are handled correctly.
#	A series of invalid flags are passed and the result from each
#	invocation of svccfg are inspected.
#
#	This assertion is verified on a service pg only (not a service
#	instance pg also)
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
	Invalid test environment - recache not available"


	RESULT=$STF_UNRESOLVED
	exit $RESULT
}

# extract and print assertion information from this source script.
extract_assertion_info $ME

assertion=svccfg_addpg_003

typeset -i test_id=0

# Before starting make sure that the test service doesn't already exist.
# If it does then consider it a fatal error.
service_exists $TEST_SERVICE
[[ $? -eq 0 ]] && {
	echo "--DIAG: [${assertion}, test 1]
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


for pgflag in abc 123 \"\" \"abc HI J
do

	typeset -i TEST_RESULT=$STF_PASS
	echo "--INFO: Starting $assertion, test $test_id ($pgflag flag)" 
	((test_id = test_id + 1))

	THIS_PROPERTY=${TEST_PROPERTY}_$pgflag

	#
	# Add a property group to the service
	#
	cat << EOF >$CMDFILE
	select ${TEST_SERVICE}
	addpg ${THIS_PROPERTY} astring $pgflag
EOF

	svccfg -f $CMDFILE > $OUTFILE 2>$ERRFILE
	ret=$?

	# Verify that the return value is as expected - non-fatal error
	[[ $ret -eq 0 ]] &&  {
		echo "--DIAG: [${assertion}, test $test_id]
		svccfg expected to return 1, got $ret"

		TEST_RESULT=$STF_FAIL
	}

	# Verify that nothing in stdout - non-fatal error
	[[ -s $OUTFILE ]] &&  {
		echo "--DIAG: [${assertion}, test $test_id]
		stdout not expected, but got $(cat $OUTFILE)"

		TEST_RESULT=$STF_FAIL
	}

	# Verify that message in stderr - non-fatal error
	[[ ! -s $ERRFILE ]] &&  {
		echo "--DIAG: [${assertion}, test $test_id]
		message in stderr expected, but got nothing"

		TEST_RESULT=$STF_FAIL
	}

	#
	# Check that the property group exists - fatal error
	#
	svcprop -p $THIS_PROPERTY $TEST_SERVICE > /dev/null 2>&1
	ret=$?
	[[ $ret -eq 0 ]] &&  {
		echo "--DIAG: [${assertion}, test $test_id]
		property group $THIS_PROPERTY does not exist"

		TEST_RESULT=$STF_FAIL
	}


	rm -f $ERRFILE $OUTFILE $CMDFILE

	print_result $TEST_RESULT
	RESULT=$(update_result $TEST_RESULT $RESULT)
done

exit $RESULT
