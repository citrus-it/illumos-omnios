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
# ASSERTION: svccfg_addpg_001
#
# DESCRIPTION:
#	The 'addpg name type [flags]' subcommand adds a property group
#	of name 'name' and type 'type' to the currently selected entity.
#	If no flags argument is given then no flag is associated with
#	the property group.
#
# STRATEGY:
#	This assertion has two subtests:
#	1) add a property group to a service
#	2) add a property group to an instance
#
#	Both of these subtests is performed from a svccfg command
#	file.
#
# end __stf_assertion__
###############################################################################

###############################################################################
# start __stf_assertion__
#
# ASSERTION: svccfg_delpg_001
#
# DESCRIPTION:
#       The 'delpg name' subcommand deletes the property group of the
#       currently selected entity as named by 'name'.  How does this
#       affect running services?
#
# STRATEGY:
#       This assetion has two subtests:
#       1) delete a property group from a service
#       1) delete a property group from an service
#
# end __stf_assertion__
###############################################################################


# First STF library
. ${STF_TOOLS}/include/stf.kshlib

# Load GL library
. ${STF_SUITE}/include/gltest.kshlib


readonly ME=$(whence -p ${0})
readonly MYLOC=$(dirname ${ME})

readonly NUM_PROPS=50

typeset -i index


# Initialize test result 
typeset -i RESULT=$STF_PASS


function cleanup {
	
	# Note that $TEST_SERVICE may or may not exist so don't check
	# results.  Just make sure the service is gone.
	service_delete ${TEST_SERVICE} > /dev/null 2>&1

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


function check_pg {

	entity=$1
	exist=$2

	typeset -i ret_value=0

	index=0
	while [ $index -lt ${NUM_PROPS} ]
	do
		svcprop -p ${TEST_PROPERTY}_${index} ${entity} > /dev/null 2>&1
		ret=$?
		if [ "$exist" = "TRUE" ] 
		then
			[[ $ret -ne 0 ]] &&   {
				echo "--DIAG: [${assertion}]
		property group ${TEST_PROPERTY}_${index} does not exist"
				ret_value=1
			}
		
		else
			[[ $ret -eq 0 ]] &&   {
				echo "--DIAG: [${assertion}]
		property group ${TEST_PROPERTY}_${index} should not 
		exist, but does"
				ret_value=1
			}
			
		fi
		(( index = index + 1 ))
	done

	return $ret_value
}

trap cleanup 0 1 2 15

# make sure that the environment is sane - svc.configd is up and running
check_gl_env
[[ $? -ne 0 ]] && {
	echo "--DIAG: 
	Invalid test environment -svc.configd not available"

	RESULT=$STF_UNRESOLVED
	exit $RESULT
}

# extract and print assertion information from this source script.
extract_assertion_info $ME

assertion=svccfg_addpg_001

# 
# Test #1: Add a property group to a service
#

echo "--INFO: Starting $assertion, test 1 (add a pg to a service)"

typeset -i TEST_RESULT=$STF_PASS


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

# Make sure the service is there
service_exists $TEST_SERVICE
[[ $? -ne 0 ]] && {
	echo "--DIAG: [${assertion}, test 1]
	service $TEST_SERVICE should exist in
	repository after being added, but does not"

	RESULT=$(update_result $STF_UNRESOLVED $RESULT)
	exit $RESULT
}


#
# Add property groups to the service
#
cat <<EOF>$CMDFILE
select ${TEST_SERVICE}
EOF

index=0
while [ $index -lt ${NUM_PROPS} ]
do
	cat <<EOF >> $CMDFILE
	addpg ${TEST_PROPERTY}_$index astring
EOF
	(( index = index + 1 ))
done

svccfg -f $CMDFILE > $OUTFILE 2>$ERRFILE
ret=$?

# Verify that the return value is as expected - non-fatal error
[[ $ret -ne 0 ]] &&  {
	echo "--DIAG: [${assertion}, test 1]
	svccfg expected to return 0, got $ret
	error output is $(cat $ERRFILE)"

	TEST_RESULT=$STF_FAIL
}

# Verify that nothing in stdout - non-fatal error
[[ -s $OUTFILE ]] &&  {
	echo "--DIAG: [${assertion}, test 1]
	stdout not expected, but got $(cat $OUTFILE)"

	TEST_RESULT=$STF_FAIL
}

# Verify that nothing in stderr - non-fatal error
[[ -s $ERRFILE ]] &&  {
	echo "--DIAG: [${assertion}, test 1]
	stderr not expected, but got $(cat $ERRFILE)
	error output is $(cat $ERRFILE)"

	TEST_RESULT=$STF_FAIL
}

#
# Check that the property groups exists - fatal error
#
check_pg ${TEST_SERVICE} TRUE
[[ $? -ne 0 ]] && TEST_RESULT=$STF_FAIL

rm -f $ERRFILE $OUTFILE $CMDFILE

print_result $TEST_RESULT
RESULT=$(update_result $TEST_RESULT $RESULT)

# 
# Test #2: Add a property group to a service instance
#

echo "--INFO: Starting $assertion, test 2 (add a pg to an instance)"

typeset -i TEST_RESULT=$STF_PASS

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
	echo "--DIAG: [${assertion}, test 2]
	svccfg expected to return 0, got $ret
	error output is $(cat $ERRFILE)"
	
	RESULT=$(update_result $STF_UNRESOLVED $RESULT)
	exit $RESULT
}
				       
#
# Add a property group to the service instance
#

cat <<EOF>$CMDFILE
select ${TEST_SERVICE}
select ${TEST_INSTANCE}
EOF

index=0
while [ $index -lt ${NUM_PROPS} ]
do
	cat <<EOF >> $CMDFILE
	addpg ${TEST_PROPERTY}_$index astring
EOF
	(( index = index + 1 ))
done

svccfg -f $CMDFILE > $OUTFILE 2>$ERRFILE
ret=$?

# Verify that the return value is as expected - non-fatal error
[[ $ret -ne 0 ]] &&  {
	echo "--DIAG: [${assertion}, test 2]
	svccfg expected to return 0, got $ret
	error output is $(cat $ERRFILE)"

	TEST_RESULT=$STF_FAIL
}

# Verify that nothing in stdout - non-fatal error
[[ -s $OUTFILE ]] &&  {
	echo "--DIAG: [${assertion}, test 2]
	stdout not expected, but got $(cat $OUTFILE)"

	TEST_RESULT=$STF_FAIL
}

# Verify that nothing in stderr - non-fatal error
[[ -s $ERRFILE ]] &&  {
	echo "--DIAG: [${assertion}, test 2]
	stderr not expected, but got $(cat $ERRFILE)"

	TEST_RESULT=$STF_FAIL
}

#
# Check that the property group exists - fatal error
#
check_pg ${TEST_SERVICE}:${TEST_INSTANCE} TRUE
[[ $? -ne 0 ]] && 
	TEST_RESULT=$STF_FAIL



rm -f $ERRFILE $OUTFILE $CMDFILE

print_result $TEST_RESULT
RESULT=$(update_result $TEST_RESULT $RESULT)

# 
# BEGIN delpg testing
#

assertion=svccfg_delpg_001

# 
# Test #1 for delpg 
#

echo "--INFO: Starting $assertion, test 1 (delete a pg from a service)"
typeset -i TEST_RESULT=$STF_PASS

cat << EOF > $CMDFILE
select ${TEST_SERVICE}
EOF

index=0
while [ $index -lt ${NUM_PROPS} ]
do
	cat <<EOF >> $CMDFILE
	delpg ${TEST_PROPERTY}_$index 
EOF
	(( index = index + 1 ))
done

svccfg -f $CMDFILE > $OUTFILE 2>$ERRFILE
ret=$?

# Verify that the return value is as expected - non-fatal error
[[ $ret -ne 0 ]] &&  {
	echo "--DIAG: [${assertion}, test 1]
	svccfg expected to return 0, got $ret
	error output is $(cat $ERRFILE)"

	TEST_RESULT=$STF_FAIL
}

# Verify that nothing in stdout - non-fatal error
[[ -s $OUTFILE ]] &&  {
	echo "--DIAG: [${assertion}, test 1]
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
# Check that the property groups do not exist - fatal error
#
check_pg ${TEST_SERVICE} FALSE
[[ $? -ne 0 ]] && TEST_RESULT=$STF_FAIL

rm -f $ERRFILE $OUTFILE $CMDFILE

print_result $TEST_RESULT
RESULT=$(update_result $TEST_RESULT $RESULT)

# 
# Test #2 for delpg 
#

echo "--INFO: Starting $assertion, test 2 (delete a pg from an instance )"
typeset -i TEST_RESULT=$STF_PASS

cat << EOF > $CMDFILE
select ${TEST_SERVICE}
select ${TEST_INSTANCE}
EOF

index=0
while [ $index -lt ${NUM_PROPS} ]
do
	cat <<EOF >> $CMDFILE
	delpg ${TEST_PROPERTY}_$index 
EOF
	(( index = index + 1 ))
done

svccfg -f $CMDFILE > $OUTFILE 2>$ERRFILE
ret=$?

# Verify that the return value is as expected - non-fatal error
[[ $ret -ne 0 ]] &&  {
	echo "--DIAG: [${assertion}, test 2]
	svccfg expected to return 0, got $ret
	error output is $(cat $ERRFILE)"

	TEST_RESULT=$STF_FAIL
}

# Verify that nothing in stdout - non-fatal error
[[ -s $OUTFILE ]] &&  {
	echo "--DIAG: [${assertion}, test 2]
	stdout not expected, but got $(cat $OUTFILE)"

	TEST_RESULT=$STF_FAIL
}

# Verify that nothing in stderr - non-fatal error
[[ -s $ERRFILE ]] &&  {
	echo "--DIAG: [${assertion}, test 2]
	stderr not expected, but got $(cat $ERRFILE)"

	TEST_RESULT=$STF_FAIL
}

#
# Check that the property groups do not exist - fatal error
#
check_pg ${TEST_SERVICE} FALSE
[[ $? -ne 0 ]] && TEST_RESULT=$STF_FAIL

rm -f $ERRFILE $OUTFILE $CMDFILE

print_result $TEST_RESULT
RESULT=$(update_result $TEST_RESULT $RESULT)

exit $RESULT
