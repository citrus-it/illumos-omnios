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
# ASSERTION: svccfg_list_002
#
# DESCRIPTION:
#	The 'list [pattern]' subcommand lists all children of the 
#	current selection which match the glob pattern.
#
# end __stf_assertion__
###############################################################################

###############################################################################
# start __stf_assertion__
#
# ASSERTION: svccfg_list_003
#
# DESCRIPTION:
#	If the glob pattern passed through the 'list [pattern]' 
#	subcommand does not match any child of the current selection 
#	then nothing is displayed to stdout and the command will exit 
#	with a status of 0.
#
# end __stf_assertion__
###############################################################################

###############################################################################
# start __stf_assertion__
#
# ASSERTION: svccfg_list_004
#
# DESCRIPTION:
#	The 'list *' subcommand produces output identical to the 
#	'list' subcommand.  If no errors have occurred during 
#	processing, the command exit status is 0.
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

# Set data 
ENTITY_DATA="entity_one entity-two entity_3 entity_45 entity-6"

OUTFILE2=${OUTFILE}_2

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


	rm -f $OUTFILE $ERRFILE $CMDFILE $OUTFILE2

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

assertion=svccfg_list_002


# Before starting make sure that the test service doesn't already exist.
# If it does then consider it a fatal error.
service_exists $TEST_SERVICE
[[ $? -eq 0 ]] && {
	echo "--DIAG: [${assertion}]
	service $TEST_SERVICE should not exist in
	repository but does"

	RESULT=$STF_UNRESOLVED
	exit $RESULT
}


cat << EOF > $CMDFILE
add $TEST_SERVICE
select $TEST_SERVICE
EOF

echo $ENTITY_DATA | tr " " "\n" | 
while read name 
do
	echo add $name >> $CMDFILE
done

svccfg -f $CMDFILE > $OUTFILE 2>$ERRFILE
ret=$?
# Verify that the return value is as expected - non-fatal error
[[ $ret -ne 0 ]] &&  {
	echo "--DIAG: [${assertion}]
	svccfg expected to return 0, got $ret
	error output is $(cat $ERRFILE)"

	RESULT=$(update_result $STF_UNRESOLVED $RESULT)
	exit $RESULT
}

	
# 
# Test #1: Simple '*'
#

echo "--INFO: Starting $assertion, test 1 (use of \'*\')"

typeset -i TEST_RESULT=$STF_PASS


cat << EOF > $CMDFILE
select ${TEST_SERVICE}
list entity*
EOF

svccfg -f $CMDFILE > $OUTFILE 2>$ERRFILE
ret=$?
# Verify that the return value is as expected - non-fatal error
[[ $ret -ne 0 ]] &&  {
	echo "--DIAG: [${assertion}, test 1]
	svccfg expected to return 0, got $ret"

	RESULT=$STF_FAIL
}

# Verify that nothing in stderr - non-fatal error
[[ -s $ERRFILE ]] &&  {
	echo "--DIAG: [${assertion}, test 1]
	stdout not expected, but got $(cat $OUTFILE)"

	TEST_RESULT=$STF_FAIL
}

for i in :properties $ENTITY_DATA 
do
	echo $i >> ${OUTFILE2} 
done

diff ${OUTFILE} ${OUTFILE2} > /dev/null
[[ $? -ne 0 ]] && {
	echo "--DIAG: [ ${assertion}, test 1]
	error in list, expected: $(cat $OUTFILE2)
	got: $(cat $OUTFILE)"
	
	TEST_RESULT=$STF_FAIL
}

print_result $TEST_RESULT
RESULT=$(update_result $TEST_RESULT $RESULT)

rm -f $OUTFILE $ERRFILE $CMDFILE $OUTFILE2


#
# Test #2: Use [a-z] and ? character
#

echo "--INFO: Starting $assertion, test 2 (use of [a-z] and ?)"

typeset -i TEST_RESULT=$STF_PASS

cat << EOF > $CMDFILE
select ${TEST_SERVICE}
list ?ntity_[a-z]*
end 
EOF

svccfg -f $CMDFILE > $OUTFILE 2>$ERRFILE
ret=$?

# Verify that the return value is as expected - non-fatal error
[[ $ret -ne 0 ]] &&  {
	echo "--DIAG: [${assertion}, test 2]
	svccfg expected to return 0, got $ret
	error output is $(cat $ERRFILE)"

	RESULT=$STF_FAIL
}


for i in :properties entity_one
do
	echo $i >> ${OUTFILE2} 
done

diff ${OUTFILE} ${OUTFILE2} > /dev/null
[[ $? -ne 0 ]] && {
	echo "--DIAG: [ ${assertion}, test 2]
	error in list, expected: $(cat $OUTFILE2)
	got: $(cat $OUTFILE)"
	
	TEST_RESULT=$STF_FAIL
}


print_result $TEST_RESULT
RESULT=$(update_result $TEST_RESULT $RESULT)

rm -f $OUTFILE $ERRFILE $CMDFILE $OUTFILE2


#
# Test #3: Use [0-9] and ? character
#

echo "--INFO: Starting $assertion, test 3 (use of [0-9] and ?)"

typeset -i TEST_RESULT=$STF_PASS

cat << EOF > $CMDFILE
select ${TEST_SERVICE}
list ent*-[0-9]
end 
EOF

svccfg -f $CMDFILE > $OUTFILE 2>$ERRFILE
ret=$?

# Verify that the return value is as expected - non-fatal error
[[ $ret -ne 0 ]] &&  {
	echo "--DIAG: [${assertion}, test 3]
	svccfg expected to return 0, got $ret
	error output is $(cat $ERRFILE)"

	RESULT=$STF_FAIL
}


for i in :properties entity-6
do
	echo $i >> ${OUTFILE2} 
done

diff ${OUTFILE} ${OUTFILE2} > /dev/null
[[ $? -ne 0 ]] && {
	echo "--DIAG: [ ${assertion}, test 3]
	error in list, expected: $(cat $OUTFILE2)
	got: $(cat $OUTFILE)"
	
	TEST_RESULT=$STF_FAIL
}


print_result $TEST_RESULT
RESULT=$(update_result $TEST_RESULT $RESULT)

rm -f $OUTFILE $ERRFILE $OUTFILE2 $CMDFILE


#
# Test #4: Test search which yields :properties
#

echo "--INFO: Starting $assertion, test 4 (test :properties)"

typeset -i TEST_RESULT=$STF_PASS

cat << EOF > $CMDFILE
select ${TEST_SERVICE}
list :properties
end 
EOF

svccfg -f $CMDFILE > $OUTFILE 2>$ERRFILE
ret=$?

# Verify that the return value is as expected - non-fatal error
[[ $ret -ne 0 ]] &&  {
	echo "--DIAG: [${assertion}, test 4]
	svccfg expected to return 0, got $ret
	error output is $(cat $ERRFILE)"

	RESULT=$STF_FAIL
}


for i in :properties 
do
	echo $i >> ${OUTFILE2} 
done

diff ${OUTFILE} ${OUTFILE2} > /dev/null
[[ $? -ne 0 ]] && {
	echo "--DIAG: [ ${assertion}, test 4]
	error in list, expected: $(cat $OUTFILE2)
	got: $(cat $OUTFILE)"
	
	TEST_RESULT=$STF_FAIL
}


print_result $TEST_RESULT
RESULT=$(update_result $TEST_RESULT $RESULT)

rm -f $OUTFILE $ERRFILE $OUTFILE2 $CMDFILE

#
# Assertion list_003
#

assertion=svccfg_list_003

echo "--INFO: Starting $assertion"

typeset -i TEST_RESULT=$STF_PASS

cat << EOF > $CMDFILE
select ${TEST_SERVICE}
list [a-df-z]*
end 
EOF

svccfg -f $CMDFILE > $OUTFILE 2>$ERRFILE
ret=$?

# Verify that the return value is as expected - non-fatal error
[[ $ret -ne 0 ]] &&  {
	echo "--DIAG: [${assertion}]
	svccfg expected to return 0, got $ret
	error output is $(cat $ERRFILE)"

	RESULT=$STF_FAIL
}


for i in :properties 
do
	echo $i >> ${OUTFILE2} 
done

diff ${OUTFILE} ${OUTFILE2} > /dev/null
[[ $? -ne 0 ]] && {
	echo "--DIAG: [ ${assertion}]
	error in list, expected: $(cat $OUTFILE2)
	got: $(cat $OUTFILE)"
	
	TEST_RESULT=$STF_FAIL
}


print_result $TEST_RESULT
RESULT=$(update_result $TEST_RESULT $RESULT)

rm -f $OUTFILE $ERRFILE $OUTFILE2 $CMDFILE

#
# Assertion list_004
#

assertion=svccfg_list_004

echo "--INFO: Starting $assertion"

typeset -i TEST_RESULT=$STF_PASS

cat << EOF > $CMDFILE
select ${TEST_SERVICE}
list *
end 
EOF

svccfg -f $CMDFILE > $OUTFILE 2>$ERRFILE
ret=$?

# Verify that the return value is as expected - non-fatal error
[[ $ret -ne 0 ]] &&  {
	echo "--DIAG: [${assertion}]
	svccfg expected to return 0, got $ret
	error output is $(cat $ERRFILE)"

	RESULT=$STF_FAIL
}


for i in :properties $ENTITY_DATA
do
	echo $i >> ${OUTFILE2} 
done

diff ${OUTFILE} ${OUTFILE2} > /dev/null
[[ $? -ne 0 ]] && {
	echo "--DIAG: [ ${assertion}]
	error in list, expected: $(cat $OUTFILE2)
	got: $(cat $OUTFILE)"
	
	TEST_RESULT=$STF_FAIL
}


print_result $TEST_RESULT
RESULT=$(update_result $TEST_RESULT $RESULT)

exit $RESULT
