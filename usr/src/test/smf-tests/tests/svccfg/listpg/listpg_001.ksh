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
# ASSERTION: svccfg_listpg_001
#
# DESCRIPTION:
#	The 'listpg [pattern]' subcommand lists all the property 
#	groups of the currently selected entity which match the 
#	glob pattern.  The types and flags are also displayed.
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

function create_property_groups
{
	entity=$1

	echo "select $entity" >> $CMDFILE
	cat $MYLOC/pg_data | 
	while read pg type flag
	do
		echo "addpg $pg $type $flag" >> $CMDFILE
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

assertion=svccfg_listpg_001

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
	error adding service $TEST_SERVICE needed for test
	error output is $(cat $ERRFILE)"

        RESULT=$(update_result $STF_UNRESOLVED $RESULT)
	exit $RESULT
}

create_property_groups ${TEST_SERVICE}

# 
# Test #1: Simple '*' 
#

echo "--INFO: Starting $assertion, test 1 (use of \'*\')"

typeset -i TEST_RESULT=$STF_PASS

cat << EOF > $CMDFILE
select ${TEST_SERVICE}
listpg pg_*
end
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
	stderr not expected, but got $(cat $ERRFILE)"

	RESULT=$STF_FAIL
}

# Verify the output

lines_1=$(wc -l $MYLOC/pg_data | awk '{print $1}')
lines_2=$(wc -l $OUTFILE | awk '{print $1}')
[[ $lines_1 -ne $lines_2 ]] &&  
	RESULT=$STF_FAIL

cat $MYLOC/pg_data |
while read pg type flag
do
	set $(grep "^$pg " $OUTFILE) > /dev/null 2>&1
	pg_cmp=$1
	type_cmp=$2
	flag_cmp=$3

	case $flag in 
		"P") flag=NONPERSISTENT;;
		"p") flag= ;;
	esac

	[[ "$pg_cmp" != "$pg" ]] && RESULT=$STF_FAIL
	[[ "$type_cmp" != "$type" ]] && RESULT=$STF_FAIL
	[[ "$flag_cmp" != "$flag" ]] && RESULT=$STF_FAIL
done


[[ $RESULT -eq $STF_FAIL ]]  && {

	echo "--DIAG: An error was found with the listpg command"
	echo "--DIAG: Here\'s the output of the listpg command:
	$(cat $OUTFILE)"

	echo "--DIAG: Here\'s the expected output:
	$(cat $MYLOC/pg_data)"
}

rm -f $ERRFILE $OUTFILE $CMDFILE

print_result $RESULT

# 
# Test #2: Use [a-z] and ? character
#

echo "--INFO: Starting $assertion, test 2 (use of [a-z] and ?)"

typeset -i TEST_RESULT=$STF_PASS

cat << EOF > $CMDFILE
select ${TEST_SERVICE}
listpg ?g_[a-z]*
end
EOF

svccfg -f $CMDFILE > $OUTFILE 2>$ERRFILE
ret=$?

# Verify that the return value is as expected - non-fatal error
[[ $ret -ne 0 ]] &&  {
	echo "--DIAG: [${assertion}, test 2]
	svccfg expected to return 0, got $ret"
	
	RESULT=$STF_FAIL
}


# Verify that nothing in stderr - non-fatal error
[[ -s $ERRFILE ]] &&  {
	echo "--DIAG: [${assertion}, test 2]
	stderr not expected, but got $(cat $ERRFILE)"

	RESULT=$STF_FAIL
}

# Verify the output

lines_1=$(wc -l $MYLOC/pg_data | awk '{print $1}')
lines_2=$(wc -l $OUTFILE | awk '{print $1}')
[[ $lines_1 -ne $lines_2 ]] &&  
	RESULT=$STF_FAIL

cat $MYLOC/pg_data |
while read pg type flag
do
	set $(grep "^$pg " $OUTFILE) > /dev/null 2>&1
	pg_cmp=$1
	type_cmp=$2
	flag_cmp=$3

	case $flag in 
		"P") flag=NONPERSISTENT ;;
		"p") flag= ;;
	esac

	[[ "$pg_cmp" != "$pg" ]] && RESULT=$STF_FAIL
	[[ "$type_cmp" != "$type" ]] && RESULT=$STF_FAIL
	[[ "$flag_cmp" != "$flag" ]] && RESULT=$STF_FAIL
done


[[ $RESULT -eq $STF_FAIL ]]  && {

	echo "--DIAG: An error was found with the listpg command"
	echo "--DIAG: Here\'s the output of the listpg command:
	$(cat $OUTFILE)"

	echo "--DIAG: Here\'s the expected output:
	$(cat $MYLOC/pg_data)"
}

rm -f $ERRFILE $OUTFILE $CMDFILE

print_result $RESULT
RESULT=$(update_result $TEST_RESULT $RESULT)


exit $RESULT
