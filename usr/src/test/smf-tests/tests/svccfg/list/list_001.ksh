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
# ASSERTION: svccfg_list_001
#
# DESCRIPTION:
#	The 'list' subcommand lists all children of the current 
#	selection.  If the current selection is an entity the 
#	output also contains ":properties".
#
# STRATEGY:
#	Test the following cases:
#	- test at scope level
#	- test at service level
#	- test at instance level
#	
#	Note: to verify the assertion (at the scope level) we don't
#	do before/after comparison of the repository listing.  This
#	is because the repository may change, for whatever reason,
#	while the test is running and before/after comparison would
#	not be accurate.  We can't control what is going on in the
#	repository outside of this test.
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

# The number of service and service/instance entries that will be
# used in this test.  This number can be modified.
typeset -r num_entities=10


function cleanup {
	# Note that $TEST_SERVICE may or may not exist so don't check
	# results.  Just make sure the service is gone.
	typeset -i index=1
	while [ $index -le $num_entities ]
	do
		service_delete ${TEST_SERVICE}_${index}
		service_exists ${TEST_SERVICE}_${index}
		[[ $? -eq 0 ]] && {
			echo "--DIAG: [${assertion}, cleanup]
			service ${TEST_SERVICE} should not exist in 
			repository after being deleted, but does"

        		RESULT=$(update_result $STF_UNRESOLVED $RESULT)
		}
		(( index = index + 1 ))
	done

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

assertion=svccfg_list_001

#
# Setup - create $num_entities services and $num_entities service/instance
#
typeset -i index=1
while [ $index -le $num_entities ]
do
	SERVICE=${TEST_SERVICE}_${index}

	svccfg add $SERVICE > /dev/null 2>&1

	service_exists $SERVICE
	[[ $? -ne 0 ]] && {
		echo "--DIAG: [${assertion}, setup]
		EXPECTED: service ${SERVICE} exists
		OBSERVED: service ${SERVICE} does not exist"

		RESULT=$(update_result $STF_UNRESOLVED $RESULT)
		exit $RESULT
	}
	typeset -i i=1
	while [ $i -le $num_entities ]
	do
		cat <<EOF >> $CMDFILE
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
		EXPECTED: svccfg returned 0
		OBSERVED: svccfg returned $ret,
		error output is $(cat $ERRFILE)"

		RESULT=$(update_result $STF_UNRESOLVED $RESULT)
		exit $RESULT
	}

	(( index = index + 1 ))

	rm -f $ERRFILE $OUTFILE $CMDFILE
done

	
# 
# Test #1: verify list at the scope level
#

echo "--INFO: Verify list at the scope level"

typeset -i TEST_RESULT=$STF_PASS

svccfg list > $OUTFILE 2>$ERRFILE
ret=$?
[[ $ret -ne 0 ]] &&  {
	echo "--DIAG: [${assertion}, test 1]
	svccfg expected to return 0, got $ret"

	TEST_RESULT=$STF_FAIL 
}

# Check that something is in stdout - we can't check the output
# because there is no reliable way of knowing what else is in the 
# repository.
[[ ! -s $OUTFILE ]] &&  {
	echo "--DIAG: [${assertion}, test 1]
	stdout expected, but got nothing"

	TEST_RESULT=$STF_FAIL
}

# Verify that nothing in stderr - non-fatal error
[[ -s $ERRFILE ]] &&  {
	echo "--DIAG: [${assertion}, test 1]
	stdout not expected, but got $(cat $OUTFILE)"

	TEST_RESULT=$STF_FAIL
}

# Verify that :properties not showing up at scope level.  
if  egrep -s "^:properties$" $OUTFILE
then
	echo "--DIAG: [${assertion}, test 1]
	did not expect :properties at scope level"

	TEST_RESULT=$STF_FAIL
fi

# Verify that service that were setup are showing up . . .
typeset -i lines
index=1
while [ $index -le $num_entities ]
do
	SERVICE=${TEST_SERVICE}_${index}
	lines=$(egrep -c "^${SERVICE}$" $OUTFILE)
	[[ $lines -ne 1 ]]  &&  {
		echo "--DIAG: [${assertion}, test 1]
		expected list subcommand to show $SERVICE, but did not"

		TEST_RESULT=$STF_FAIL
	}

	((index = index + 1 ))
done

rm -f $ERRFILE $OUTFILE  $CMDFILE

print_result $TEST_RESULT
RESULT=$(update_result $TEST_RESULT $RESULT)

 
# Test #2: verify list at the service level
#

echo "--INFO: Verify list at the service level"

typeset -i TEST_RESULT=$STF_PASS

# Pick any test service
SERVICE=${TEST_SERVICE}_1

cat  << EOF > $CMDFILE
select ${SERVICE}
list
EOF

svccfg -f $CMDFILE > $OUTFILE 2>$ERRFILE
ret=$?
[[ $ret -ne 0 ]] &&  {
	echo "--DIAG: [${assertion}]
	svccfg expected to return 0, got $ret
	error output is $(cat $ERRFILE)"

	TEST_RESULT=$STF_FAIL
}


#
# Make sure that the instances in the service are showing up
#
index=1
while [ $index -le $num_entities ]
do
	pattern=${SERVICE}_${TEST_INSTANCE}_$index
	lines=$(egrep -c "^$pattern$" $OUTFILE)
        [[ $lines -ne 1 ]]  &&  {
		echo "--DIAG: [${assertion}, test 2]
		expected list subcommand to show $pattern, but did not"

		TEST_RESULT=$STF_FAIL
	}

	((index = index + 1 ))
done

# Make sure that :properties shows up
if  ! egrep -s "^:properties$" $OUTFILE > /dev/null 2>&1
then
	echo "--DIAG: [${assertion}, test 2]
	expected :properties at service level but did not get"

	TEST_RESULT=$STF_FAIL
fi

rm -f $ERRFILE $OUTFILE $CMDFILE

print_result $TEST_RESULT
RESULT=$(update_result $TEST_RESULT $RESULT)

# 
# Test #3: verify list at the instance level
#


echo "--INFO: Verify list at the instance level"

typeset -i TEST_RESULT=$STF_PASS

# Pick any test service & instance
SERVICE=${TEST_SERVICE}_1
INSTANCE=${TEST_INSTANCE}_1

cat  << EOF > $CMDFILE
select ${SERVICE}
select ${SERVICE}_${INSTANCE}
list
EOF

svccfg -f $CMDFILE > $OUTFILE 2>$ERRFILE
ret=$?
[[ $ret -ne 0 ]] &&  {
	echo "--DIAG: [${assertion}]
	svccfg expected to return 0, got $ret
	error output is $(cat $ERRFILE)"

	TEST_RESULT=$STF_FAIL
}

# Make sure that there is only one entry - ":properties"

lines=$(wc -l $OUTFILE | awk '{print $1}')
[[ $lines -ne 1 ]] &&  {
	echo "--DIAG: [${assertion}, test 3]
	expected only :properties entry, but got $(cat $OUTFILE)"

	TEST_RESULT=$STF_FAIL
}

if  ! egrep -s "^:properties$" $OUTFILE > /dev/null 2>&1
then
	echo "--DIAG: [${assertion}, test 3]
	expected :properties at service level but did not get"

	TEST_RESULT=$STF_FAIL
fi


rm -f $ERRFILE $OUTFILE $CMDFILE

print_result $TEST_RESULT
RESULT=$(update_result $TEST_RESULT $RESULT)

exit $RESULT

