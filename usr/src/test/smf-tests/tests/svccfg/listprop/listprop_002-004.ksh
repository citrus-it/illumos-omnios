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
# ASSERTION: svccfg_listprop_002
#
# DESCRIPTION:
#	The 'listprop [pattern]' subcommand lists all the 
#	names of the property groups and properties of the 
#	current select entity that match the glob pattern 
#	provided. Types and flags of property groups and 
#	types and values of properties are also displayed.
#
# end __stf_assertion__
###############################################################################

###############################################################################
# start __stf_assertion__
#
# ASSERTION: svccfg_listprop_003
#
# DESCRIPTION:
#	If the glob pattern provided to the 'listprop pattern' 
#	subcommand does not match any property groups/properties 
#	of the currently selected entity then no property 
#	groups/properties are listed.  A return status of 0 is 
#	returned.
#
# STRATEGY:
#
# end __stf_assertion__
###############################################################################

###############################################################################
# start __stf_assertion__
#
# ASSERTION: svccfg_listprop_004
#
# DESCRIPTION:
#	The 'listprop *' subcommand produces output identical 
#	to the 'listprop' subcommand.  If no errors have occurred 
#	during processing, then command exit status is 0.
#
# STRATEGY:
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
	service_delete ${TEST_SERVICE}

	service_exists ${TEST_SERVICE}
	[[ $? -eq 0 ]] && {
		echo "--DIAG: [${assertion}, cleanup]
		service ${TEST_SERVICE} should not exist in 
		repository after being deleted, but does"

        	RESULT=$(update_result $STF_UNRESOLVED $RESULT)
	}

	rm -f $OUTFILE $ERRFILE $CMDFILE $OUTFILE2

	exit $RESULT
}

function verify_outfile
{
	total_lines=$1

	num_lines=$(wc -l $OUTFILE | awk '{print $1}')
	[[ $num_lines != $total_lines ]] && {
		echo "--DIAG: [${assertion}]
		expected $total_lines lines after listprop, got $num_lines"
		
		TEST_RESULT=$STF_FAIL
	}


	cat $OUTFILE | 
	while read prop type value
	do
		prop_name=$(basename $prop)
		num=$(echo $prop_name | awk -F_ '{print $2}')
		[[ $type != "astring" ]]  && {
			echo "--DIAG: [${assertion}]
			expected type to be astring, got $type"
	
			TEST_RESULT=$STF_FAIL
		}
		[[ ${data_array[$num]} != "$value" ]] && {
			echo "--DIAG: [${assertion}, test 1]
			expected value to be ${data_array[$num]}, not $value"
	
			TEST_RESULT=$STF_FAIL
		}
	done

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

assertion=svccfg_listprop_002

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
# Add the service, a property group and define properties within
# the property group.
#

set -A data_array "0123456789abcdefghijklmnopqrstuvwxyz01234567890abcdefghijklmnopqrstuvwxyz" \
		  '"one two three four"' \
		  100 \
		  2147483647 \
		  a \
		  one \
		  two  \
		  three  \
		  four \
	 	  five \
	 	  six

typeset -i total=${#data_array[*]}
typeset -i index=0


cat << EOF > $CMDFILE
add $TEST_SERVICE
select $TEST_SERVICE
addpg ${TEST_PROPERTY} framework
EOF

while [ $index -lt $total ]
do
	echo "setprop ${TEST_PROPERTY}/prop_$index = astring: ${data_array[$index]}" >> $CMDFILE
	(( index = index + 1 ))
done

svccfg -f $CMDFILE > $OUTFILE 2>$ERRFILE
ret=$?
[[ $ret -ne 0 ]] && {
	echo "--DIAG: [${assertion}]
	svccfg expected to return 0, got $ret
	error output is $(cat $ERRFILE)"

        RESULT=$(update_result $STF_UNRESOLVED $RESULT)

	exit $RESULT
}

# 
# Test #1: Simple '*' 
#

echo "--INFO: Starting $assertion, test 1 (use of '*')"
TEST_RESULT=$STF_PASS

typeset -i TEST_RESULT=$STF_PASS

cat << EOF > $CMDFILE
select ${TEST_SERVICE}
listprop ${TEST_PROPERTY}/*
end
EOF

svccfg -f $CMDFILE > $OUTFILE 2>$ERRFILE
ret=$?

# Verify that the return value is as expected - non-fatal error
[[ $ret -ne 0 ]] &&  {
	echo "--DIAG: [${assertion}, test 1]
	svccfg expected to return 0, got $ret"

	TEST_RESULT=$STF_FAIL
}


# Verify that nothing in stderr - non-fatal error
[[ -s $ERRFILE ]] &&  {
	echo "--DIAG: [${assertion}, test 1]
	stderr not expected, but got $(cat $ERRFILE)"

	TEST_RESULT=$STF_FAIL
}

verify_outfile $total

rm -f $OUTFILE $ERRFILE $CMDFILE

print_result $TEST_RESULT
RESULT=$(update_result $TEST_RESULT $RESULT)

# 
# Test #2: Glob pattern with []
#

echo "--INFO: Starting $assertion, test 2 (use of [])"
TEST_RESULT=$STF_PASS

cat << EOF > $CMDFILE
select ${TEST_SERVICE}
listprop */prop_[1-4]*
end
EOF

svccfg -f $CMDFILE > $OUTFILE 2>$ERRFILE
ret=$?

# Verify that the return value is as expected - non-fatal error
[[ $ret -ne 0 ]] &&  {
	echo "--DIAG: [${assertion}, test 2]
	svccfg expected to return 0, got $ret"
	
	TEST_RESULT=$STF_FAIL
}


# Verify that nothing in stderr - non-fatal error
[[ -s $ERRFILE ]] &&  {
	echo "--DIAG: [${assertion}, test 2]
	stderr not expected, but got $(cat $ERRFILE)"

	TEST_RESULT=$STF_FAIL
}

verify_outfile 5

print_result $TEST_RESULT
RESULT=$(update_result $TEST_RESULT $RESULT)

rm -f $ERRFILE $OUTFILE $CMDFILE

# 
# Test #3: Glob pattern with ?
#

echo "--INFO: Starting $assertion, test 3 (use of ?)"
TEST_RESULT=$STF_PASS


cat << EOF > $CMDFILE
select ${TEST_SERVICE}
listprop */prop_??
end
EOF

svccfg -f $CMDFILE > $OUTFILE 2>$ERRFILE
ret=$?

# Verify that the return value is as expected - non-fatal error
[[ $ret -ne 0 ]] &&  {
	echo "--DIAG: [${assertion}, test 3]
	svccfg expected to return 0, got $ret"
	
	TEST_RESULT=$STF_FAIL
}


# Verify that nothing in stderr - non-fatal error
[[ -s $ERRFILE ]] &&  {
	echo "--DIAG: [${assertion}, test 3]
	stderr not expected, but got $(cat $ERRFILE)"

	TEST_RESULT=$STF_FAIL
}

# Check the output - only one line is expected

verify_outfile 1


rm -f $ERRFILE $OUTFILE $CMDFILE
RESULT=$(update_result $TEST_RESULT $RESULT)

print_result $TEST_RESULT


# 
# Test listprop_003
#

assertion=svccfg_listprop_003

echo "--INFO: Starting $assertion"
TEST_RESULT=$STF_PASS


cat << EOF > $CMDFILE
select ${TEST_SERVICE}
listprop */xyz*
end
EOF

svccfg -f $CMDFILE > $OUTFILE 2>$ERRFILE
ret=$?

# Verify that the return value is as expected - non-fatal error
[[ $ret -ne 0 ]] &&  {
	echo "--DIAG: [${assertion}]
	svccfg expected to return 0, got $ret"
	
	TEST_RESULT=$STF_FAIL
}


# Verify that nothing in stderr - non-fatal error
[[ -s $ERRFILE ]] &&  {
	echo "--DIAG: [${assertion}]
	stderr not expected, but got $(cat $ERRFILE)"

	TEST_RESULT=$STF_FAIL
}

# Verify that nothing in stderr - non-fatal error
[[ -s $OUTFILE ]] &&  {
	echo "--DIAG: [${assertion}]
	stderr not expected, but got $(cat $OUTFILE)"

	TEST_RESULT=$STF_FAIL
}

rm -f $ERRFILE $OUTFILE $CMDFILE
RESULT=$(update_result $TEST_RESULT $RESULT)

print_result $TEST_RESULT


# 
# Test listprop_004
#

assertion=svccfg_listprop_004

echo "--INFO: Starting $assertion"
TEST_RESULT=$STF_PASS


cat << EOF > $CMDFILE
select ${TEST_SERVICE}
listprop *
end
EOF

svccfg -f $CMDFILE > $OUTFILE 2>/dev/null
ret=$?

# Verify that the return value is as expected - non-fatal error
[[ $ret -ne 0 ]] &&  {
	echo "--DIAG: [${assertion}]
	svccfg expected to return 0, got $ret"
	
	TEST_RESULT=$STF_FAIL
}

cat << EOF > $CMDFILE
select ${TEST_SERVICE}
listprop 
end
EOF

OUTFILE2=${OUTFILE}_2

svccfg -f $CMDFILE > $OUTFILE2 2>/dev/null
ret=$?

# Verify that the return value is as expected - non-fatal error
[[ $ret -ne 0 ]] &&  {
	echo "--DIAG: [${assertion}]
	svccfg expected to return 0, got $ret"
	
	TEST_RESULT=$STF_FAIL
}

cmp -s $OUTFILE2 $OUTFILE
[[ $? -ne 0 ]] && {
	echo "--DIAG: [${assertion}]
	output of two listprop calls different . . ."

	echo "Output of 'listprop': $(cat $OUTFILE)"
	echo "Output of 'listprop *': $(cat $OUTFILE2)"

	TEST_RESULT=$STF_FAIL
}




rm -f $ERRFILE $OUTFILE $CMDFILE $OUTFILE2

print_result $TEST_RESULT

RESULT=$(update_result $TEST_RESULT $RESULT)

exit $RESULT
