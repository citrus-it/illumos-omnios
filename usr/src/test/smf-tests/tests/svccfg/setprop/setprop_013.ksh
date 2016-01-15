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
# ASSERTION: svccfg_setprop_013
#
# DESCRIPTION:
#	The 'setprop pg/name = ([values . . . ])' subcommand
#	accepts multiple values for the corresponding existing
#	pg/name.
#
# STRATEGY:
#	- Add a test service to the repository
#	- Add a service propertygroup to the test service
#	- Add a pair of multivalued properties to the newly added service pg
#	- Add a test instance to the test service
#	- Add a propertygroup to the test instance
#	- Add a pair of multivalued properties to the newly added instance pg
#	- Export the service.
#	- Compare the exported manifest to an expected manifest containing 
#	  the same multivalued properties added during the test
#	- Verify that the manifests are identical
#
# end __stf_assertion__
###############################################################################


# First STF library
. ${STF_TOOLS}/include/stf.kshlib

# Load GL library
. ${STF_SUITE}/include/gltest.kshlib

# Load svc.startd config library
. ${STF_SUITE}/include/svc.startd_config.kshlib


readonly ME=$(whence -p ${0})
readonly MYLOC=$(dirname ${ME})
readonly assertion=svccfg_setprop_013

# Initialize test result
typeset -i RESULT=$STF_PASS

function cleanup {
	# Note that $TEST_SERVICE may or may not exist, so don't check
	# results.  Just make sure the service is gone.
	service_delete $TEST_SERVICE

	service_exists $TEST_SERVICE
	[[ $? -eq 0 ]] && {
		echo "--DIAG: [${assertion}, cleanup]
	service $TEST_SERVICE should not exist in the repository
	after being deleteed, but does"

		RESULT=$(update_result $STF_UNRESOLVED $RESULT)
	}

	rm -f $OUTFILE $ERRFILE $CMDFILE

	exit $RESULT
}

trap cleanup 0 1 2 15

# Extract and print assertion information from this source script
extract_assertion_into $ME

# Make sure that the environment is sane - svc.configd is up and running
check_gl_env
[[ $? -ne 0 ]] && {
	echo "--DIAG: [$assertion]
	Invalid test environment -- svc.configd is not available"

	RESULT=$STF_UNRESOLVED
	exit $RESULT
}

# Before starting, make sure that the test service does not already exist.
# If it does, treat that as a fatal error.
export TEST_SERVICE=${assertion}_service
export TEST_INSTANCE=${assertion}_instance

service_exists $TEST_SERVICE
[[ $? -eq 0 ]] && {
	echo "--DIAG: [$assertion]
	service $TEST_SERVICE should not exist in the repository,
	but does"

	RESULT=$(update_result $STF_UNRESOLVED $RESULT)
	exit $RESULT
}

# Add the test service and instance to the repository and set up a 
# test service pg and instace pg

typeset svcpg=${assertion}_svcpg
typeset instpg=${assertion}_instpg

cat <<EOF > $CMDFILE
add $TEST_SERVICE
select $TEST_SERVICE
addpg $svcpg application
add $TEST_INSTANCE
addpg $instpg application
EOF

svccfg -f $CMDFILE > $OUTFILE 2>$ERRFILE
ret=$?
[[ $ret -ne 0 ]] && {
	echo "--DIAG: [$assertion]
	error adding service $TEST_SERVICE needed for test
	Error output is $(cat $ERRFILE)"

	RESULT=$(update_result $STF_UNRESOLVED $RESULT)
	exit $RESULT
}

#
# Add a couple of multivalued properties to the servicepg.  Treat all failures
# as fatal
#
typeset astringprop=${assertion}_astringprop
typeset countprop=${assertion}_countprop

svccfg -s $TEST_SERVICE <<EOF
setprop $svcpg/$astringprop = astring: ("first svc prop" "second svc prop")
setprop $svcpg/$countprop = count: (1 2 3 4 5)
EOF > $OUTFILE 2>$ERRFILE

ret=$?
[[ $ret -ne 0 ]] && {
	echo "--DIAG: [$assertion]
	error adding service pg $svcpg to $TEST_SERVICE
	Error output is $(cat $ERRFILE)"

	RESULT=$(update_result $STF_UNRESOLVED $RESULT)
	exit $RESULT
}

#
# Add a couple of multivalued properties to the instancepg.  Treat all failures
# as fatal
#

svccfg -s ${TEST_SERVICE}:${TEST_INSTANCE} <<EOF
setprop $instpg/$astringprop = astring: ("first inst prop" "second inst prop")
setprop $instpg/$countprop = count: (11 12 13 14 15)
EOF > $OUTFILE 2>$ERRFILE

ret=$?
[[ $ret -ne 0 ]] && {
	echo "--DIAG: [$assertion]
	error adding instance pg $instpg to ${TEST_SERVICE}:${TEST_INSTANCE}
	Error output is $(cat $ERRFILE)"

	RESULT=$(update_result $STF_UNRESOLVED $RESULT)
	exit $RESULT
}

#
# Export the service manifest and capture it in $OUTFILE
#
svccfg export $TEST_SERVICE > $OUTFILE 2>$ERRFILE
ret=$?
[[ $ret -ne 0 ]] && {
	echo "--DIAG: [$assertion]
	error adding instance pg $instpg to ${TEST_SERVICE}:${TEST_INSTANCE}
	Error output is $(cat $ERRFILE)"

	RESULT=$(update_result $STF_UNRESOLVED $RESULT)
	exit $RESULT
}

#
# Compare the manifest with the expected manifest
#
expected_manifest=${MYLOC}/setprop_013.xml

cmp -s $OUTFILE $expected_manifest
ret=$?
[[ $ret -ne 0 ]] &&  {
	echo "--DIAG: [${assertion}]
	svccfg setprop failed.  Expected service manifest is
	$(cat $expected_manifest)
	the actual output is:
	$(cat ${OUTFILE})"

	RESULT=$STF_FAIL
}

#
exit $RESULT
