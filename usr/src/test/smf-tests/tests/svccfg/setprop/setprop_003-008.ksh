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
# ASSERTION: svccfg_setprop_003
#
# DESCRIPTION:
#	The 'setprop pg/name = [type:] value'  subcommand expects
#    	the lhs of the argument to be a valid "pg/name" where pg 
#	is a valid property group and name is a valid property name.  
#	If "pg/name" is not a valid value then a diagnostic message 
#	will be sent to stderr and the subcommand will exit with a 
#	status of 1.
#
# STRATEGY:
#	The test is performed through an expect script which is
#	called from this script.
#
# end __stf_assertion__
###############################################################################

###############################################################################
# start __stf_assertion__
#
# ASSERTION: svccfg_setprop_004
#
# DESCRIPTION:
#	The 'setprop pg/name = [type:] *' subcommand expects
#	the rhs of the argument to be a valid "[type:] *.  If 
#	'type' is not valid then a diagnostic message will be sent 
#	to stderr and the subcommand will exit with a status of 1.
#
# STRATEGY:
#	The test is performed through an expect script which is
#	called from this script.
#
# end __stf_assertion__
###############################################################################

###############################################################################
# start __stf_assertion__
#
# ASSERTION: svccfg_setprop_005
#
# DESCRIPTION:
#	The 'setprop pg/name = [type:] value' subcommand 
#	expects the rhs of the argument to be a valid "[type:] 
#	value".  If 'values' is not valid for the corresponding 
#	'type' then a diagnostic message will be sent to stderr 
#	and the subcommand will exit with a status of 1.
#
# STRATEGY:
#	The test is performed through an expect script which is
#	called from this script.
#
# end __stf_assertion__
###############################################################################

###############################################################################
# start __stf_assertion__
#
# ASSERTION: svccfg_setprop_006
#
# DESCRIPTION:
#	The 'setprop pg/name = [type:] ([values . . . ])' 
#	subcommand expects the rhs of the argument to be a 
#	valid "type ([values . . .])".  If 'values' is not 
#	enclosed in parenthesis then a diagnostic message will 
#	be sent to stderr and the subcommand will exit with a 
#	status of 1.
#
# STRATEGY:
#	The test is performed through an expect script which is
#	called from this script.
#
# end __stf_assertion__
###############################################################################

###############################################################################
# start __stf_assertion__
#
# ASSERTION: svccfg_setprop_007
#
# DESCRIPTION:
#	The 'setprop pg/name = [type:] ([values . . . ])' 
#	subcommand expects an assignment 'lhs = rhs' as an 
#	argument.  If an invalid argument is passed to setprop 
#	then a diagnostic message will be sent to stderr and 
#	the subcommand will exit with a status of 1.  Invalid
#	argument should include: no arguments, valid assignments 
#	without the '=' character, valid arguments with trailing 
#	members (e.g.  'lhs = rhs extra')
#
# STRATEGY:
#	The test is performed through an expect script which is
#	called from this script.
#
# end __stf_assertion__
###############################################################################

###############################################################################
# start __stf_assertion__
#
# ASSERTION: svccfg_setprop_008
#
# DESCRIPTION:
#	If 'setprop pg/name = type ([values . . . ])' subcommand 
#	contains a pg/name value in which the property does not 
#	exist and the type specifier is not present will return 
#	a diagnostic message and a return value of 1.
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

assertion=svccfg_setprop_003-008


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

$MYLOC/test_003-008 $TEST_SERVICE
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

$MYLOC/setprop_003-008 $TEST_SERVICE $TEST_INSTANCE
ret=$?
[[ $ret -ne 0 ]] && TEST_RESULT=$STF_FAIL

RESULT=$(update_result $TEST_RESULT $RESULT)

# Test done

exit $RESULT
