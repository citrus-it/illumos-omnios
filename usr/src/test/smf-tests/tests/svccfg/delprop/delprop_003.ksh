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
# ASSERTION: svccfg_delprop_003
#
# DESCRIPTION:
#	If the property group, 'pg', specified in the 
#	'delprop pg' subcommand does not exist in the selected 
#	entity then a diagnostic message is sent to stderr and 
#	the subcommand will exit with a status of 1.
#
#
# STRATEGY:
#	Verify with the following conditions:
#	#1: delete a property group that was never defined
#	#2: delete a property group that was already deleted within the svccfg 
#	    sessions.
#	#3: delete a property group that was already deleted within 
#	    another session.
#
# end __stf_assertion__
###############################################################################


# First STF library
. ${STF_TOOLS}/include/stf.kshlib

# Load GL library
. ${STF_SUITE}/include/gltest.kshlib

# Assertion ID
readonly assertion=svccfg_delprop_003

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


cat << EOF > $CMDFILE
add $TEST_SERVICE
select $TEST_SERVICE
EOF

svccfg -f $CMDFILE > $OUTFILE 2>$ERRFILE
ret=$?
[[ $ret -ne 0 ]] && {
	echo "--DIAG: [${assertion}, setup]
	svccfg expected to return 0, got $ret
	error output is $(cat $ERRFILE)"

	RESULT=$(update_result $STF_UNRESOLVED $RESULT)
	exit $RESULT
}


#
# Test #1: delete a property that was never defined
#

echo "--INFO: Starting $assertion, test 1 (delete undefined property)"

typeset -i TEST_RESULT=$STF_PASS

# Call delprop with extra options

cat << EOF > $CMDFILE
select $TEST_SERVICE
delprop $TEST_PROPERTY
EOF


svccfg -f $CMDFILE > $OUTFILE 2>$ERRFILE
ret=$?

# Verify that the return value is as expected
[[ $ret -ne 1 ]] &&  {
	echo "--DIAG: [${assertion}, test 1]
	svccfg expected to return 1, got $ret"

	TEST_RESULT=$STF_FAIL
}

# Verify that nothing in stdout
[[ -s $OUTFILE ]] &&  {
	echo "--DIAG: [${assertion}, test 1]
	did not expect stdout, but got:
	$(cat $OUTFILE)"

	TEST_RESULT=$STF_FAIL
}

# Verify that message in stderr - non-fatal error
if ! egrep -s "$NO_PROPGRP_ERRMSG" $ERRFILE
then
	echo "--DIAG: [${assertion}, test 1]
	Expected error message \"$NO_PROPGRP_ERRMSG\"
	but got \"$(cat $ERRFILE)\""

	TEST_RESULT=$STF_FAIL
fi


rm -f $OUTFILE $ERRFILE $CMDFILE

print_result $TEST_RESULT
RESULT=$(update_result $TEST_RESULT $RESULT)

#
# Test #2: delete a property that was already deleted within the 
#	svccfg sessions.
#

echo "--INFO: Starting $assertion, test 2 (delete already deleted "
echo "property in same session"

typeset -i TEST_RESULT=$STF_PASS

cat << EOF > $CMDFILE
select $TEST_SERVICE
addpg $TEST_PROPERTY framework
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

cat << EOF > $CMDFILE
select $TEST_SERVICE
delprop $TEST_PROPERTY
delprop $TEST_PROPERTY
EOF

svccfg -f $CMDFILE > $OUTFILE 2>$ERRFILE
ret=$?

# Verify that the return value is as expected
[[ $ret -ne 1 ]] &&  {
	echo "--DIAG: [${assertion}, test 2]
	svccfg expected to return 1, got $ret"

	TEST_RESULT=$STF_FAIL
}

# Verify that nothing in stdout
[[ -s $OUTFILE ]] &&  {
	echo "--DIAG: [${assertion}, test 2]
	did not expect stdout, but got:
	$(cat $OUTFILE)"

	TEST_RESULT=$STF_FAIL
}

# Verify that message in stderr - non-fatal error
if ! egrep -s "$NO_PROPGRP_ERRMSG" $ERRFILE
then
	echo "--DIAG: [${assertion}, test 2]
	Expected error message \"$NO_PROPGRP_ERRMSG\"
	but got \"$(cat $ERRFILE)\""

	TEST_RESULT=$STF_FAIL
fi


rm -f $OUTFILE $ERRFILE $CMDFILE

print_result $TEST_RESULT
RESULT=$(update_result $TEST_RESULT $RESULT)


#
# Test #3: delete a property that was already deleted within another session.
#

typeset -i TEST_RESULT=$STF_PASS

echo "--INFO: Starting $assertion, test 3 (delete already deleted"
echo "property in different sessions"

cat << EOF > $CMDFILE
select $TEST_SERVICE
addpg $TEST_PROPERTY framework
EOF

svccfg -f $CMDFILE > $OUTFILE 2>$ERRFILE
ret=$?

# Verify that the return value is as expected
[[ $ret -ne 0 ]] &&  {
	echo "--DIAG: [${assertion}, test 3]
	svccfg expected to return 0, got $ret
	error output is $(cat $ERRFILE)"

	RESULT=$(update_result $STF_UNRESOLVED $RESULT)
	exit $RESULT
}

# Delete added property in this svccfg call.  

cat << EOF > $CMDFILE
select $TEST_SERVICE
delprop $TEST_PROPERTY
EOF

svccfg -f $CMDFILE > $OUTFILE 2>$ERRFILE
ret=$?

# Verify that the return value is as expected
[[ $ret -ne 0 ]] &&  {
	echo "--DIAG: [${assertion}, test 3]
	svccfg expected to return 0, got $ret
	error output is $(cat $ERRFILE)"

	RESULT=$(update_result $STF_UNRESOLVED $RESULT)
	exit $RESULT
}


# Delete pg that was deleted in previous invocation
cat << EOF > $CMDFILE
select $TEST_SERVICE
delprop $TEST_PROPERTY
EOF

svccfg -f $CMDFILE > $OUTFILE 2>$ERRFILE
ret=$?

# Verify that the return value is as expected
[[ $ret -ne 1 ]] &&  {
	echo "--DIAG: [${assertion}, test 3]
	svccfg expected to return 1, got $ret"

	TEST_RESULT=$STF_FAIL
}


# Verify that nothing in stdout
[[ -s $OUTFILE ]] &&  {
	echo "--DIAG: [${assertion}, test 3]
	did not expect stdout, but got:
	$(cat $OUTFILE)"

	TEST_RESULT=$STF_FAIL
}

# Verify that message in stderr - non-fatal error
if ! egrep -s "$NO_PROPGRP_ERRMSG" $ERRFILE
then
	echo "--DIAG: [${assertion}, test 3]
	Expected error message \"$NO_PROPGRP_ERRMSG\"
	but got \"$(cat $ERRFILE)\""

	TEST_RESULT=$STF_FAIL
fi


rm -f $OUTFILE $ERRFILE $CMDFILE

print_result $TEST_RESULT
RESULT=$(update_result $TEST_RESULT $RESULT)

exit $RESULT



