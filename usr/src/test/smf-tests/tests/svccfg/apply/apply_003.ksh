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
# ASSERTION: svccfg_apply_003
#
# DESCRIPTION:
#	Calling the "apply file" subcommand where the file contains a
#	service archive will result in a diagnostic message being sent
#	to stderr and the command exiting with an exit status of 1.
#
# end __stf_assertion__
###############################################################################


# First STF library
. ${STF_TOOLS}/include/stf.kshlib

# Load GL library
. ${STF_SUITE}/include/gltest.kshlib

# Load svc.startd library for manifest_generate
. ${STF_SUITE}/include/svc.startd_config.kshlib

readonly ME=$(whence -p ${0})
readonly MYLOC=$(dirname ${ME})

# Initialize test result
typeset -i RESULT=$STF_PASS

function cleanup {
	# Note that $TEST_SERVICE may not exist.  So don't check
	# reslts.  Just make sure the service is gone

	manifest_purgemd5 $manifest_file

	rm -f $OUTFILE $ERRFILE $manifest_file $archive_file
}

trap cleanup 0 1 2 15

# make sure that the environment is sane -- svc.configd is up and running
check_gl_env
[[ $? -ne 0 ]] && {
	echo "--DIAG: 
     	Invalid test environment - svc.configd  not available"

        RESULT=$STF_UNRESOLVED
	exit $RESULT
}

assertion=svccfg_apply_003

# Extract and print assertion information from this source script
extract_assertion_info $ME

# Before starting, make sure that the test service doesn't already exist.
# If it does, then consider it a fatal error.  This also mean the previous
# test did not successfully clean up after itself
#
service_exists $TEST_SERVICE
ret=$?
[[ $ret -eq 0 ]] && {
	echo "--DIAG: [${assertion}]
	service $TEST_SERVICE should not exist in
	repository but does"

	RESULT=$(update_result $STF_UNRESOLVED $RESULT)
	exit $RESULT
}

# Make sure that no instances of the test service are running.  This
# is to ensure that the subsequent pgrep used to verify the assertion
# does not falsely fail.
#
pgrep -z $(zonename) $(basename $SERVICE_APP) > /dev/null 2>&1
ret=$?
[[ $ret -eq 0 ]] && {
       	echo "--DIAG: [${assertion}]
	an instance of $(basename $SERVICE_APP) is running but should not be
	to ensure correct validation of this test"

	RESULT=$(update_result $STF_UNRESOLVED $RESULT)
	exit $RESULT
}


# # Start assertion testing
#
readonly archive_file=$PWD/svccfg_archive.xml
readonly manifest_file=$archive_file

manifest_purgemd5 $manifest_file

# Create an svccfg archive file from the current contents of the repository
#
echo "--INFO: Create an svccfg archive file from current repository contents"
svccfg archive > $archive_file 2>&1
ret=$?
if [[ $ret -ne 0 ]]; then
	echo "--DIAG: [${assertion}]
	'svccfg archive' failed to generate proper archives
	EXPECTED: command exitcode 0
	RETURNED: command exitcode $ret"
	RESULT=$(update_result $STF_UNRESOLVED $RESULT)
	exit $RESULT
fi

# Now verify the assertion
echo "--INFO: Now attempt to 'apply' the generated svccfg archive.
	Expect failure."

svccfg apply $manifest_file > $OUTFILE 2>$ERRFILE
ret=$?
[[ $ret -ne 1 ]] && {
	echo "--DIAG: [$assertion]
	'svccfg apply' did not fail as expected when supplied an archive file
	EXPECTED: command exitcode 1
	RETURNED: command exitcode $ret"

	RESULT=$STF_FAIL
}

# Verify that there's nothing in stdout
[[ -s $OUTFILE ]] && {
	echo "--DIAG: $assertion
	stdout not expected, but got $(cat $OUTFILE)"

	RESULT=$STF_FAIL
}

# Verify that stderr is not empty
[[ ! -s $ERRFILE ]] && {
	echo "--DIAG: $assertion
	Expected error output to stderr, but got nothing"

	RESULT=$STF_FAIL
}

# Verify that stderr contains the expected error message
if ! egrep -s "$NOT_PROFILE_ERRMSG" $ERRFILE
then
        echo "--DIAG: [${assertion}]
        Expected error message \"$NOT_PROFILE_ERRMSG\"
        but got \"$(cat $ERRFILE)\""

        RESULT=$STF_FAIL
fi

exit $RESULT
