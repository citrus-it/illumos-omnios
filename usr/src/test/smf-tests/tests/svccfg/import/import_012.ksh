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
# Copyright 2009 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
 
###############################################################################
# start __stf_assertion__
#
# ASSERTION: svccfg_import_012
#
# DESCRIPTION:
#	Deleting and reimporting services with dependencies to verify
#	everything is working as expected
#
# STRATEGY:
#	Import services with the following dependencies:
#	svccfg_import_012a <-- svccfg_import_012b <-> svccfg_import_012b:default --> svccfg_import_012c
#
#	Delete services in various scenarios and verify expected
#	behavior
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
	
	# Note that $TEST_SERVICE_# may or may not exist so don't check
	# results.  Just make sure the service is gone.

	manifest_purgemd5 ${registration_file_1}
	manifest_purgemd5 ${registration_file_2}
	manifest_purgemd5 ${registration_file_3}

	service_cleanup ${TEST_SERVICE_1}
	service_cleanup ${TEST_SERVICE_2}
	service_cleanup ${TEST_SERVICE_3}

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

	service_exists ${TEST_SERVICE_3}
	[[ $? -eq 0 ]] && {
		echo "--DIAG: [${assertion}, cleanup]
		service ${TEST_SERVICE_2} should not exist in 
		repository after being deleted, but does"

		RESULT=$(update_result $STF_UNRESOLVED $RESULT)
	}

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

assertion=svccfg_import_012

# extract and print assertion information from this source script.
extract_assertion_info $ME

# Start assertion testing

readonly TEST_SERVICE_1=svccfg_import_012a
readonly TEST_SERVICE_2=svccfg_import_012b
readonly TEST_SERVICE_3=svccfg_import_012c
readonly TEST_INSTANCE_1=default
readonly TEST_INSTANCE_2=default
readonly TEST_INSTANCE_3=default
readonly registration_file_1=$MYLOC/import_012a.xml
readonly registration_file_2=$MYLOC/import_012b.xml
readonly registration_file_3=$MYLOC/import_012c.xml
readonly FMRI_1=${TEST_SERVICE_1}:${TEST_INSTANCE_1}
readonly FMRI_2=${TEST_SERVICE_2}:${TEST_INSTANCE_2}
readonly FMRI_3=${TEST_SERVICE_3}:${TEST_INSTANCE_3}

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

service_exists ${TEST_SERVICE_3}
[[ $? -eq 0 ]] && {
	echo "--DIAG: [${assertion}]
	service ${TEST_SERVICE_3} should not exist in
	repository but does"

	RESULT=$(update_result $STF_UNRESOLVED $RESULT)
	exit $RESULT
}


#######################################################################
# Import the 3 services and verify dependencies
#######################################################################

#
# Import each service 
#
service_import ${TEST_SERVICE_1} ${registration_file_1} 
ret=$?
[[ $ret -ne 0 ]] && {
	echo "--DIAG: [${assertion}]
	import of ${TEST_SERVICE_1} failed"

	RESULT=$STF_FAIL
}

# Import 2nd service
service_import ${TEST_SERVICE_2} ${registration_file_2} 
ret=$?
[[ $ret -ne 0 ]] && {
	echo "--DIAG: [${assertion}]
	import of ${TEST_SERVICE_2} failed"

	RESULT=$STF_FAIL
}

# Import 3rd service
service_import ${TEST_SERVICE_3} ${registration_file_3} 
ret=$?
[[ $ret -ne 0 ]] && {
	echo "--DIAG: [${assertion}]
	import of ${TEST_SERVICE_3} failed"

	RESULT=$STF_FAIL
}

#
# Verify dependencies. svccfg_import_012b should list both svccfg_import_012a and svccfg_import_012c
#
svcprop ${FMRI_2} | egrep test_dependency | egrep -s ${TEST_SERVICE_1} > /dev/null 2>&1
ret=$?
[[ $ret -ne 0 ]] && {
 	echo "--DIAG: [${assertion}]
	dependency on service ${FMRI_2} not in the repository but should be"

	RESULT=$STF_FAIL
}

svcprop ${FMRI_2} | egrep test_dependency | egrep -s ${TEST_SERVICE_3} > /dev/null 2>&1
ret=$?
[[ $ret -ne 0 ]] && {
	echo "--DIAG: [${assertion}]
	dependency on service ${FMRI_2} not in the repository but should be"

	RESULT=$STF_FAIL
}

#
# svcs should also report dependencies
#
svcs -dH ${FMRI_2} | egrep -s ${FMRI_1} > /dev/null 2>&1
ret=$?
[[ $ret -ne 0 ]] && {
	echo "--DIAG: [${assertion}]
	dependency on service ${FMRI_2} for ${FMRI_1} not in the repository but should be"

	RESULT=$STF_FAIL
}

svcs -dH ${FMRI_2} | egrep -s ${FMRI_3} > /dev/null 2>&1
ret=$?
[[ $ret -ne 0 ]] && {
	echo "--DIAG: [${assertion}]
	dependency on service ${FMRI_2} for $FMRI_3} not in the repository but should be"

	RESULT=$STF_FAIL
}


#######################################################################
# Delete only dependent service, verify dependencies, and reimport
#######################################################################

#
# Delete the svccfg_import_012b service
#
svccfg delete ${TEST_SERVICE_2} > /dev/null 2>&1
ret=$?
[[ $ret -ne 0 ]] && {
	echo "--DIAG: [${assertion}]
	svccfg delete failed for service ${TEST_SERVICE_2}: ret=$ret"

	RESULT=$STF_FAIL
}

#
# Verify the dummy svccfg_import_012b service still exists
#
svcs -H ${FMRI_2} > /dev/null 2>&1
ret=$?
[[ $ret -ne 0 ]] && {
	echo "--DIAG: [${assertion}]
	Dummy service for ${FMRI_2} does not exist as expected"

	RESULT=$STF_FAIL
}

#
# Verify dependencies still exist for svccfg_import_012b
#
svcs -dH ${FMRI_2} | egrep -s ${FMRI_1} > /dev/null 2>&1
ret=$?
[[ $ret -ne 0 ]] && {
	echo "--DIAG: [${assertion}] 
	dependency on service ${FMRI_2} for ${FMRI_1} not reported as expected"

	RESULT=$STF_FAIL
}

svcs -dH ${FMRI_2} | egrep -s ${FMRI_3} > /dev/null 2>&1
ret=$?
[[ $ret -ne 0 ]] && {
	echo "--DIAG: [${assertion}]
	dependency on service ${FMRI_2} for ${FMRI_3} not reported as expected"

	RESULT=$STF_FAIL
}

#
# Verify extended properties are still reported by svcprop
#
svcprop ${FMRI_2} | egrep test_dependency | egrep -s ${TEST_SERVICE_1} > /dev/null 2>&1
ret=$?
[[ $ret -ne 0 ]] && {
	echo "--DIAG: [${assertion}]
	dependency on service ${FMRI_2} for ${TEST_SERVICE_1} not in the repository but should be"

	RESULT=$STF_FAIL
}

svcprop ${FMRI_2} | egrep test_dependency | egrep -s ${TEST_SERVICE_3} > /dev/null 2>&1
ret=$?
[[ $ret -ne 0 ]] && {
	echo "--DIAG: [${assertion}]
	dependency on service ${FMRI_2} for ${TEST_SERVICE_3} not in the repository but should be"

	RESULT=$STF_FAIL
}

#
# Reimport svccfg_import_012b
#
service_import ${TEST_SERVICE_2} ${registration_file_2} 
ret=$?
[[ $ret -ne 0 ]] && {
	echo "--DIAG: [${assertion}]
	import of ${TEST_SERVICE_2} failed"

	RESULT=$STF_FAIL
}

#
# Verify dependencies are still intact
#
svcs -dH ${FMRI_2} | egrep -s ${FMRI_1} > /dev/null 2>&1
ret=$?
[[ $ret -ne 0 ]] && {
	echo "--DIAG: [${assertion}]
	dependency on service ${FMRI_2} not reported as expected"

	RESULT=$STF_FAIL
}

svcs -dH ${FMRI_2} | egrep -s ${FMRI_3} > /dev/null 2>&1
ret=$?
[[ $ret -ne 0 ]] && {
	echo "--DIAG: [${assertion}]
	dependency on service ${FMRI_2} not reported as expected"

	RESULT=$STF_FAIL
}

#
# Verify dependences remained intact
#
svcprop ${FMRI_2} | egrep test_dependency | egrep -s ${TEST_SERVICE_1} > /dev/null 2>&1
ret=$?
[[ $ret -ne 0 ]] && {
	echo "--DIAG: [${assertion}]
	dependency on service ${FMRI_2} for ${TEST_SERVICE_1} not in the repository but should be"

	RESULT=$STF_FAIL
}

svcprop ${FMRI_2} | egrep test_dependency | egrep -s ${TEST_SERVICE_3} > /dev/null 2>&1
ret=$?
[[ $ret -ne 0 ]] && {
	echo "--DIAG: [${assertion}]
	dependency on service ${FMRI_2} for ${TEST_SERVICE_3} not in the repository but should be"

	RESULT=$STF_FAIL
}


#######################################################################
# Delete svccfg_import_012a and svccfg_import_012b service, verify dependencies, reimport
#######################################################################

#
# Delete svccfg_import_012a and svccfg_import_012b
#
svccfg delete ${TEST_SERVICE_1} > /dev/null 2>&1
ret=$?
[[ $ret -ne 0 ]] && {
	echo "--DIAG: [${assertion}]
	svccfg delete failed for service ${TEST_SERVICE_1}: ret=$ret"

	RESULT=$STF_FAIL
}

svccfg delete ${TEST_SERVICE_2} > /dev/null 2>&1
ret=$?
[[ $ret -ne 0 ]] && {
	echo "--DIAG: [${assertion}]
	svccfg delete failed for service ${TEST_SERVICE_2}: ret=$ret"

	RESULT=$STF_FAIL
}

#
# Verify the dummy svccfg_import_012b service still exists
#
svcs -H ${FMRI_2} > /dev/null 2>&1
ret=$?
[[ $ret -ne 0 ]] && {
	echo "--DIAG: [${assertion}]
	Dummy service for ${FMRI_2} does not exist as expected"

	RESULT=$STF_FAIL
}

#
# Verify appropriate dependencies still exist for svccfg_import_012b
#
svcs -dH ${FMRI_2} | egrep -s ${FMRI_1} > /dev/null 2>&1
ret=$?
[[ $ret -eq 0 ]] && {
	echo "--DIAG: [${assertion}]
	dependency on service ${FMRI_2} not reported as expected"

	RESULT=$STF_FAIL
}

svcs -dH ${FMRI_2} | egrep -s ${FMRI_3} > /dev/null 2>&1
ret=$?
[[ $ret -ne 0 ]] && {
	echo "--DIAG: [${assertion}]
	dependency on service ${FMRI_2} not reported as expected"

	RESULT=$STF_FAIL
}

#
# Verify extended properties are reported as expected by svcprop
#
svcprop ${FMRI_2} | egrep test_dependency | egrep -s ${TEST_SERVICE_1} > /dev/null 2>&1
ret=$?
[[ $ret -eq 0 ]] && {
	echo "--DIAG: [${assertion}]
	dependency on service ${FMRI_2} for ${TEST_SERVICE_1} is in the repository but shouldn't be"

	RESULT=$STF_FAIL
}

svcprop ${FMRI_2} | egrep test_dependency | egrep -s ${TEST_SERVICE_3} > /dev/null 2>&1
ret=$?
[[ $ret -ne 0 ]] && {
	echo "--DIAG: [${assertion}]
	dependency on service ${FMRI_2} not in the repository but should be"

	RESULT=$STF_FAIL
}

#
# Reimport svccfg_import_012a
#
service_import ${TEST_SERVICE_1} ${registration_file_1} 
ret=$?
[[ $ret -ne 0 ]] && {
	echo "--DIAG: [${assertion}]
	import of ${TEST_SERVICE_1} failed"

	RESULT=$STF_FAIL
}

#
# Reimport svccfg_import_012b
#
service_import ${TEST_SERVICE_2} ${registration_file_2} 
ret=$?
[[ $ret -ne 0 ]] && {
	echo "--DIAG: [${assertion}]
	import of ${TEST_SERVICE_2} failed"

	RESULT=$STF_FAIL
}

#
# Verify dependences remained intact
#
svcprop ${FMRI_2} | egrep test_dependency | egrep -s ${TEST_SERVICE_1} > /dev/null 2>&1
ret=$?
[[ $ret -ne 0 ]] && {
	echo "--DIAG: [${assertion}]
	dependency on service ${FMRI_2} for ${TEST_SERVICE_1} not in the repository but should be"

	RESULT=$STF_FAIL
}

svcprop ${FMRI_2} | egrep test_dependency | egrep -s ${TEST_SERVICE_3} > /dev/null 2>&1
ret=$?
[[ $ret -ne 0 ]] && {
	echo "--DIAG: [${assertion}]
	dependency on service ${FMRI_2} for ${TEST_SERVICE_3} not in the repository but should be"

	RESULT=$STF_FAIL
}


#######################################################################
# Delete only svccfg_import_012c service and svccfg_import_012b service
#######################################################################

#
# Delete svccfg_import_012c and svccfg_import_012b, verify dependencies haven't been lost
#
svccfg delete ${TEST_SERVICE_2} > /dev/null 2>&1
ret=$?
[[ $ret -ne 0 ]] && {
	echo "--DIAG: [${assertion}]
	svccfg delete failed for service ${TEST_SERVICE_2}: ret=$ret"

	RESULT=$STF_FAIL
}

svccfg delete ${TEST_SERVICE_3} > /dev/null 2>&1
ret=$?
[[ $ret -ne 0 ]] && {
	echo "--DIAG: [${assertion}]
	svccfg delete failed for service ${TEST_SERVICE_3}: ret=$ret"

	RESULT=$STF_FAIL
}


#
# Verify the dummy svccfg_import_012b service no longer exists
#
svcs -H ${FMRI_2} > /dev/null 2>&1
ret=$?
[[ $ret -eq 0 ]] && {
	echo "--DIAG: [${assertion}]
	Dummy service for ${FMRI_2} should not exist"

	RESULT=$STF_FAIL
}

#
# Verify svcprop fails for svccfg_import_012b
#
svcprop ${FMRI_2} > /dev/null 2>&1
ret=$?
[[ $ret -eq 0 ]] && {
	echo "--DIAG: [${assertion}]
	svcprop succeeded for ${FMRI_2} and shouldn't have"

	RESULT=$STF_FAIL
}

#
# Verify svccfg_import_012b service is *not* listed in dependency graph
#
output=$(echo ::vertex | mdb -p `pgrep startd` | grep svccfg_import_012b)
if [[ -n "$output" ]]; then
	echo "--DIAG: [${assertion}]
	$TEST_SERVICE_2 should not be listed in dependency graph"

	RESULT=$STF_FAIL
fi

#
# Reimport svccfg_import_012b
#
service_import ${TEST_SERVICE_2} ${registration_file_2}
ret=$?
[[ $ret -ne 0 ]] && {
	echo "--DIAG: [${assertion}]
	Import of ${registration_file_2} expected to return 0, got $ret"

	RESULT=$STF_FAIL
}

#
# Reimport svccfg_import_012c
#
service_import ${TEST_SERVICE_3} ${registration_file_3} 
ret=$?
[[ $ret -ne 0 ]] && {
	echo "--DIAG: [${assertion}]
	Import of ${registration_file_2} expected to return 0, got $ret"

	RESULT=$STF_FAIL
}

#
# Verify appropriate dependencies are correct for all services
#
svcs -dH ${FMRI_2} | egrep -s ${FMRI_1} > /dev/null 2>&1
ret=$?
[[ $ret -ne 0 ]] && {
	echo "--DIAG: [${assertion}]
	dependency on service ${FMRI_2} for ${FMRI_1} not reported as expected"

	RESULT=$STF_FAIL
}

svcs -dH ${FMRI_2} | egrep -s ${FMRI_3} > /dev/null 2>&1
ret=$?
[[ $ret -ne 0 ]] && {
	echo "--DIAG: [${assertion}]
	dependency on service ${FMRI_2} for ${FMRI_3} not reported as expected"

	RESULT=$STF_FAIL
}

svcprop ${FMRI_2} | egrep test_dependency | egrep -s ${TEST_SERVICE_1} > /dev/null 2>&1
ret=$?
[[ $ret -ne 0 ]] && {
	echo "--DIAG: [${assertion}]
	dependency on service ${FMRI_2} for ${TEST_SERVICE_1} not in the repository but should be"

	RESULT=$STF_FAIL
}

svcprop ${FMRI_2} | egrep test_dependency | egrep -s ${TEST_SERVICE_3} > /dev/null 2>&1
ret=$?
[[ $ret -ne 0 ]] && {
	echo "--DIAG: [${assertion}]
	dependency on service ${FMRI_2} for ${TEST_SERVICE_3} not in the repository but should be"

	RESULT=$STF_FAIL
}

exit $RESULT
