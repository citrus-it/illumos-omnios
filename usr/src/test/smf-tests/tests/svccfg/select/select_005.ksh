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
# ASSERTION: svccfg_select_005
#
# DESCRIPTION:
#	If the name supplied to the 'select' subcommand is invalid,
#	the diagnostic message "Invalid FMRI." is sent to stderr.
#	The command exit status will be 1.
#
# STRATEGY:
#	1. Supply various invalid names to the 'select' subcommand.
#	   Examples of invalid names: "12345", "@#$%#%", etc.
#	2. In each case, expect the error "Invalid FMRI." to be
#	   displayed on stderr.
#	3. Verify that the command exit status is 1.
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

	rm -f $OUTFILE $ERRFILE $CMDFILE

	exit $RESULT
}

trap cleanup 0 1 2 15

# make sure that the environment is sane - svc.configd is up and running
check_gl_env
[[ $? -ne 0 ]] && {
	echo "--DIAG: 
	Invalid test environment - recache not available"


	RESULT=$(update_result $STF_UNRESOLVED $RESULT)
	exit $RESULT
}

# extract and print assertion information from this source script.
extract_assertion_info $ME

assertion=svccfg_select_005

typeset -i test_id=0

for invalid_name in $INVALID_NAME_LIST
do

	typeset -i TEST_RESULT=$STF_PASS
	echo "--INFO: Starting $assertion, test $test_id (name is \"$invalid_name\")" 
	((test_id = test_id + 1))

	svccfg select $invalid_name > $OUTFILE 2>$ERRFILE
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

	NO_MATCH="doesn't match any instances or service"
	EMSG="svccfg: Pattern '$invalid_name' $NO_MATCH"

	# Verify that message in stderr - non-fatal error
	if ! grep -sl "$EMSG" $ERRFILE >/dev/null 2>&1
	then
		echo "--DIAG: [${assertion}, test 1]
		Expected error message:
$EMSG
		but got: 
$(cat $ERRFILE)"

		TEST_RESULT=$STF_FAIL
	fi

	rm -f $ERRFILE $OUTFILE 

	print_result $TEST_RESULT
	RESULT=$(update_result $TEST_RESULT $RESULT)
done

exit $RESULT
