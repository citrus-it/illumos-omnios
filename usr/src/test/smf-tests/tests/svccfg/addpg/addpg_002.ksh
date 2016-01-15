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
# ASSERTION: svccfg_addpg_002
#
# DESCRIPTION:
#	The 'addpg name type [flags]' subcommand adds a property 
#	group of name 'name' and type 'type' to the currently 
#	selected entity.  If the flags argument is "P" then 
#	the SCF_PG_FLAG_NONPERSISTENT flag is set.  If the flags 
#	argument is "p" then the SCF_PG_FLAG_NONPERSISTENT flag is 
#	cleared.
#
# STRATEGY:
#	This assertion has two subtests:
#	1) add a property group to a service with the P flag
#	2) add a property group to a service with the p flag (this 
#	   should be the same as not adding a flag)
#	3) add a property group to a instance with the P flag
#	4) add a property group to a instance with the p flag (this 
#	   should be the same as not adding a flag)
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
	Invalid test environment - svc.configd not available"

	RESULT=$STF_UNRESOLVED
	exit $RESULT
}

# extract and print assertion information from this source script.
extract_assertion_info $ME

assertion=svccfg_addpg_002

typeset -i test_id=1


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


# 
# Test #1, #2: Add a pg to a service with a P and p flag
#

for pgflag in P p
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
	[[ $ret -ne 0 ]] &&  {
		echo "--DIAG: [${assertion}, test $test_id]
		svccfg expected to return 0, got $ret
		error output is $(cat $ERRFILE)"

		TEST_RESULT=$STF_FAIL
	}

	# Verify that nothing in stdout - non-fatal error
	[[ -s $OUTFILE ]] &&  {
		echo "--DIAG: [${assertion}, test $test_id]
		stdout not expected, but got $(cat $OUTFILE)"

		TEST_RESULT=$STF_FAIL
	}

	# Verify that nothing in stderr - non-fatal error
	[[ -s $ERRFILE ]] &&  {
		echo "--DIAG: [${assertion}, test $test_id]
		stderr not expected, but got $(cat $ERRFILE)"

		TEST_RESULT=$STF_FAIL
	}

	#
	# Check that the property group exists - fatal error
	#
	svcprop -p $THIS_PROPERTY $TEST_SERVICE > /dev/null 2>&1
	ret=$?
	[[ $ret -ne 0 ]] &&  {
		echo "--DIAG: [${assertion}, test $test_id]
		property group $THIS_PROPERTY does not exist"

		TEST_RESULT=$STF_FAIL
	}


	rm -f $ERRFILE $OUTFILE $CMDFILE

	print_result $TEST_RESULT
	RESULT=$(update_result $TEST_RESULT $RESULT)
done

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
	echo "--DIAG: [${assertion}]
	svccfg add expected to return 0, got $ret
	error output is $(cat $ERRFILE)"
	
	exit $STF_UNRESOLVED
}

# Make sure the service is there
service_exists ${TEST_SERVICE}:${TEST_INSTANCE}
[[ $? -ne 0 ]] && {
	echo "--DIAG: [${assertion}]
	service $TEST_SERVICE should exist in
	repository after being added, but does not"

	RESULT=$(update_result $STF_UNRESOLVED $RESULT)
	exit $RESULT
}

# 
# Test #3, #4: Add a pg to an instance with a P and p flag
#

for pgflag in P p
do

	typeset -i TEST_RESULT=$STF_PASS
	echo "--INFO: Starting $assertion, test $test_id ($pgflag flag instance)" 
	((test_id = test_id + 1))

	THIS_PROPERTY=${TEST_PROPERTY}_$pgflag

	#
	# Add a property group to the service
	#
	cat << EOF >$CMDFILE
	select ${TEST_SERVICE}
	select ${TEST_INSTANCE}
	addpg ${THIS_PROPERTY} astring $pgflag
EOF

	svccfg -f $CMDFILE > $OUTFILE 2>$ERRFILE
	ret=$?

	# Verify that the return value is as expected - non-fatal error
	[[ $ret -ne 0 ]] &&  {
		echo "--DIAG: [${assertion}, test $test_id]
		svccfg expected to return 0, got $ret
		error output is $(cat $ERRFILE)"

		TEST_RESULT=$STF_FAIL
	}

	# Verify that nothing in stdout - non-fatal error
	[[ -s $OUTFILE ]] &&  {
		echo "--DIAG: [${assertion}, test $test_id]
		stdout not expected, but got $(cat $OUTFILE)"

		TEST_RESULT=$STF_FAIL
	}

	# Verify that nothing in stderr - non-fatal error
	[[ -s $ERRFILE ]] &&  {
		echo "--DIAG: [${assertion}, test 1]
		stderr not expected, but got $(cat $ERRFILE)"

		TEST_RESULT=$STF_FAIL
	}

	#
	# Check that the property group exists - fatal error
	#
	svcprop -p $THIS_PROPERTY ${TEST_SERVICE}:${TEST_INSTANCE} > /dev/null 2>&1
	ret=$?
	[[ $ret -ne 0 ]] &&  {
		echo "--DIAG: [${assertion}, test $test_id]
		property group $THIS_PROPERTY does not exist"

		TEST_RESULT=$STF_FAIL
	}


	rm -f $ERRFILE $OUTFILE $CMDFILE

	print_result $TEST_RESULT
	RESULT=$(update_result $TEST_RESULT $RESULT)
done

exit $RESULT
