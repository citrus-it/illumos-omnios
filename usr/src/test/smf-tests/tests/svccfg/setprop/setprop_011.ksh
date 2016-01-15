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
# ASSERTION: svccfg_setprop_011
#
# DESCRIPTION:
#	If 'setprop pg/name = [type:] *' subcommand specifies
#	a property that already exists and the type disagrees 
#	with the existing type on the property, the request 
#	will return a diagnostic message and a return value of 1.
#
# STRATEGY:
#	The test is performed through an expect script which is
#	called from this script.
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

# make sure that the environment is sane - svc.configd is up and running
check_gl_env
[[ $? -ne 0 ]] && {
	echo "--DIAG: 
	Invalid test environment - svc.configd is not available"

        RESULT=$STF_UNRESOLVED 
	exit $RESULT
}

# extract and print assertion information from this source script.
extract_assertion_info $ME

assertion=svccfg_setprop_011


# Before starting make sure that the test service doesn't already exist.
# If it does then consider it a fatal error.
service_exists $TEST_SERVICE
[[ $? -eq 0 ]] && {
	echo "--DIAG: [${assertion}]
	service $TEST_SERVICE should not exist in 
	repository but does"

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
	error adding service $TEST_SERVICE needed for test"

        RESULT=$(update_result $STF_UNRESOLVED $RESULT)
	exit $RESULT
}


echo "--INFO: [${assertion}] 
	Running error tests on service entities"

# Run the error test on service entities

$MYLOC/setprop_011 $TEST_SERVICE
ret=$?
[[ $ret -ne 0 ]] && TEST_RESULT=$STF_FAIL


cat <<EOF>$CMDFILE
select ${TEST_SERVICE}
add ${TEST_INSTANCE}
EOF

svccfg -f $CMDFILE > $OUTFILE 2>$ERRFILE
ret=$?

# Verify that the return value is as expected - non-fatal error
[[ $ret -ne 0 ]] &&  {
	echo "--DIAG: [${assertion}]
	svccfg add expected to return 0, got $ret"

	RESULT=$(update_result $STF_UNRESOLVED $RESULT)
	exit $RESULT
}


# Run the error test on instance entities

echo "--INFO: [${assertion}] 
	Running error tests on instance entities"

$MYLOC/test_011 $TEST_SERVICE $TEST_INSTANCE
ret=$?
[[ $ret -ne 0 ]] && TEST_RESULT=$STF_FAIL

RESULT=$(update_result $TEST_RESULT $RESULT)

# Test done

exit $RESULT
