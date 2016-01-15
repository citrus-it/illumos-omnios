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
# ASSERTION: svccfg_import_011
#
# DESCRIPTION:
#	Calling the 'import' subcommand with a valid service manifest that
#	lists dependencies on other services will create these dependencies,
#	assuming these services are known and valid.
#
# STRATEGY:
#	Simple test which imports an .xml file with dependencies and
#	verifies that they are successfully loaded into the repository.
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
	
	# Note that $TEST_SERVICE may or may not exist so don't check
	# results.  Just make sure the service is gone.

	manifest_purgemd5 ${registration_file_1}
	manifest_purgemd5 ${registration_file_2}

	service_cleanup ${TEST_SERVICE_1}
	service_cleanup ${TEST_SERVICE_2}

	service_exists ${TEST_SERVICE_1}
	[[ $? -eq 0 ]] && {
		echo "--DIAG: [${assertion}, cleanup]
		service ${TEST_SERVICE_1} should not exist in 
		repository after being deleted, but does"

		RESULT=$(update_result $STF_UNRESOLVED $RESULT)
	}

	service_exists ${TEST_SERVICE_2}
	[[ $? -eq 0 ]] && {
		echo "--DIAG: [${assertion}, cleanup]
		service ${TEST_SERVICE_2} should not exist in 
		repository after being deleted, but does"

		RESULT=$(update_result $STF_UNRESOLVED $RESULT)
	}


	rm -f $OUTFILE $ERRFILE ${LOGFILE_1} ${LOGFILE_2} \
		${STATEFILE_1} ${STATEFILE_2}  ${registration_file_1} \
		${registration_file_2}

	exit $RESULT
}

trap cleanup 0 1 2 15

# make sure that the environment is sane - svc.configd  is up and running
check_gl_env
[[ $? -ne 0 ]] && {
	echo "--DIAG: 
	     	Invalid test environment - svc.configd  not available"

        RESULT=$STF_UNRESOLVED
	exit $RESULT
}

assertion=svccfg_import_011

# extract and print assertion information from this source script.
extract_assertion_info $ME

# Before starting make sure that the test services don't already exist.
# If it does then consider it a fatal error.

service_exists ${TEST_SERVICE_1}
[[ $? -eq 0 ]] && {
	echo "--DIAG: [${assertion}]
	service ${TEST_SERVICE_1} should not exist in
	repository but does"

	RESULT=$(update_result $STF_UNRESOLVED $RESULT)
	exit $RESULT
}

service_exists ${TEST_SERVICE_2}
[[ $? -eq 0 ]] && {
	echo "--DIAG: [${assertion}]
	service ${TEST_SERVICE_2} should not exist in
	repository but does"

	RESULT=$(update_result $STF_UNRESOLVED $RESULT)
	exit $RESULT
}


# # Start assertion testing
#

readonly TEST_SERVICE_1=${TEST_SERVICE}_1
readonly TEST_SERVICE_2=${TEST_SERVICE}_2
readonly TEST_INSTANCE_1=${TEST_INSTANCE}_1
readonly TEST_INSTANCE_2=${TEST_INSTANCE}_2
readonly LOGFILE_1=/tmp/log.$$.1
readonly LOGFILE_2=/tmp/log.$$.2
readonly STATEFILE_1=/tmp/state.$$.1
readonly STATEFILE_2=/tmp/state.$$.2
readonly registration_file_1=$MYLOC/foo_1.xml
readonly registration_file_2=$MYLOC/foo_2.xml
readonly FMRI_1=${TEST_SERVICE_1}:${TEST_INSTANCE_1}
readonly FMRI_2=${TEST_SERVICE_2}:${TEST_INSTANCE_2}


# Use the registration file from import_001
registration_template=$MYLOC/import_001.xml

manifest_generate $registration_template \
	TEST_SERVICE=${TEST_SERVICE_1} \
	TEST_INSTANCE=${TEST_INSTANCE_1} \
	SERVICE_APP=$SERVICE_APP  \
 	LOGFILE=${LOGFILE_1} \
	STATEFILE=${STATEFILE_1} >${registration_file_1}


manifest_purgemd5 ${registration_file_1}

svccfg import ${registration_file_1} > $OUTFILE 2>$ERRFILE
ret=$?
[[ $ret -ne 0 ]] &&  {
	echo "--DIAG: [${assertion}]
	svccfg import expected to return 0, got $ret"

	RESULT=$STF_FAIL
}

# Verify that nothing in stdout - this is a non-fatal error
[[ -s $OUTFILE ]] &&  {
	echo "--DIAG: [${assertion}]
	stdout not expected, but got $(cat $OUTFILE)"

	RESULT=$STF_FAIL
}

# Verify that nothing in stderr - this is a non-fatal error
[[ -s $ERRFILE ]] &&  {
	echo "--DIAG: [${assertion}]
	stderr not expected, but got $(cat $ERRFILE)"

	RESULT=$STF_FAIL
}

service_exists ${TEST_SERVICE_1}
[[ $? -ne 0 ]] && {
       	echo "--DIAG: [${assertion}]
	service ${TEST_SERVICE_1} should exist in repository but does not"

	RESULT=$STF_FAIL
}

registration_template=$MYLOC/import_011.xml

manifest_generate $registration_template \
	TEST_SERVICE=${TEST_SERVICE_2} \
	TEST_INSTANCE=${TEST_INSTANCE_2} \
	SERVICE_APP=$SERVICE_APP  \
 	LOGFILE=${LOGFILE_1} \
	STATEFILE=${STATEFILE_2} | sed "s/FMRI/${FMRI_1}/" >${registration_file_2}

manifest_purgemd5 ${registration_file_2}

svccfg  import ${registration_file_2} > $OUTFILE 2>$ERRFILE
ret=$?
[[ $ret -ne 0 ]] &&  {
	echo "--DIAG: [${assertion}]
	svccfg import expected to return 0, got $ret"

	RESULT=$STF_FAIL
}


svcprop ${FMRI_2} | egrep test_dependency | egrep -s ${FMRI_1} > /dev/null 2>&1
ret=$?
[[ $ret -ne 0 ]] && {
       	echo "--DIAG: [${assertion}]
	dependency on service ${FMRI_1} not in the repository but should be"

	RESULT=$STF_FAIL
}

rm -f $ERRFILE $OUTFILE 
exit $RESULT
