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

#
# start __stf_assertion__
#
# ASSERTION: depends_014
# DESCRIPTION:
#  A service with multiple dependencies in a "require_any" grouping
#  in the service's definition is online. If all of the dependencies 
#  reaches a state of offline or disabled then the service will be
#  transitioned into the offline state.
#  Service: a, b, c; a depends on b, c
#
# end __stf_assertion__
#

. ${STF_TOOLS}/include/stf.kshlib
. ${STF_SUITE}/include/gltest.kshlib
. ${STF_SUITE}/include/svc.startd_config.kshlib
. ${STF_SUITE}/tests/svc.startd/include/svc.startd_common.kshlib

typeset service_setup=0
function cleanup {
	common_cleanup
	rm -f $service_state1 $service_state2 $service_state3
}

function enable_wait {
	typeset service="$1"
	echo "--INFO: Enabling service $service"
	svcadm enable $service
	if [ $? -ne 0 ]; then
		echo "--DIAG: $assertion: Service $service did not enable"
	        exit $STF_FAIL
	fi

	echo "--INFO: Waiting for $service to come online"
	service_wait_state $service online
	if [ $? -ne 0 ]; then
		echo "--DIAG: $assertion: Service $service did not "
		echo "  come online"
		exit $STF_FAIL
	fi
}

function disable_wait {
	typeset service="$1"
	echo "--INFO: Disabling service $service"
	svcadm disable $service
	if [ $? -ne 0 ]; then
		echo "--DIAG: $assertion: Service $service did not disable"
	        exit $STF_FAIL
	fi

	echo "--INFO: Waiting for $service to go disabled"
	service_wait_state $service disabled
	if [ $? -ne 0 ]; then
		echo "--DIAG: $assertion: Service $service did not "
		echo "  goto disabled"
		exit $STF_FAIL
	fi
}

trap cleanup 0 1 2 15

readonly ME=$(whence -p ${0})
readonly MYLOC=$(dirname ${ME})

DATA=$MYLOC

readonly registration_template=$DATA/service_014.xml

extract_assertion_info $ME

# make sure that the svc.startd is running
verify_daemon
if [ $? -ne 0 ]; then
	echo "--DIAG: $assertion: svc.startd is not executing. Cannot "
	echo "  continue"
	exit $STF_UNRESOLVED
fi

# Make sure the environment is clean - the test service isn't running
echo "--INFO: Cleanup any old $test_service state"
service_cleanup $test_service
if [ $? -ne 0 ]; then
	echo "--DIAG: $assertion: cleanup of a previous instance failed"
	exit $STF_UNRESOLVED
fi

echo "--INFO: generating manifest for importation into repository"
manifest_generate $registration_template \
	TEST_SERVICE=$test_service \
	TEST_INSTANCE1=$test_instance1 \
	TEST_INSTANCE2=$test_instance2 \
	TEST_INSTANCE3=$test_instance3 \
	SERVICE_APP=$service_app \
	LOGFILE=$service_log \
	STATEFILE1=$service_state1 \
	STATEFILE2=$service_state2 \
	STATEFILE3=$service_state3 \
	> $registration_file

echo "--INFO: Importing service into repository"
manifest_purgemd5 $registration_file
svccfg -v import $registration_file >$svccfg_errfile 2>&1

if [ $? -ne 0 ]; then
	echo "--DIAG: $assertion: Unable to import the services $test_FMRI1"
        echo "  $test_FMRI2 and $test_FMRI3 error messages from svccfg: "
        echo "  \"$(cat $svccfg_errfile)\""
	exit $STF_UNRESOLVED
fi
service_setup=1

echo "--INFO: Wait for $test_FMRI3 to come online - it should not"
service_wait_state $test_FMRI2 online
if [ $? -eq 0 ]; then
	echo "--DIAG: $assertion: Service $test_FMRI3 came online"
	exit $STF_FAIL
fi

enable_wait $test_FMRI1

echo "--INFO: Wait for $test_FMRI3 to come online - it should"
service_wait_state $test_FMRI3 online
if [ $? -ne 0 ]; then
	echo "--DIAG: $assertion: Service $test_FMRI3 didn't come online"
	echo "  it's in the '$(svcs -H -o STATE $test_FMRI3)' state."
	exit $STF_FAIL
fi

disable_wait $test_FMRI1

echo "--INFO: Verifying that $test_FMRI3 goes offline"
service_wait_state $test_FMRI3 offline
if [ $? -ne 0 ]; then
	echo "--DIAG: $assertion: Service $test_FMRI3 did not go offline"
	echo "  it's in the '$(svcs -H -o STATE $test_FMRI3)' state."
	exit $STF_FAIL
fi

disable_wait $test_FMRI3

echo "--INFO: Enabling $test_FMRI3"
svcadm enable $test_FMRI3
if [ $? -ne 0 ]; then
        echo "--DIAG: $assertion: Service $test_FMRI3 did not enable"
        exit $STF_FAIL
fi

echo "--INFO: Waiting for $test_FMRI3 to stay offline"
service_wait_state $test_FMRI3 offline
if [ $? -ne 0 ]; then
	echo "--DIAG: $assertion: Service $test_FMRI3 did not goto offline"
	echo "  it's in the '$(svcs -H -o STATE $test_FMRI3)' state."
	exit $STF_FAIL
fi

enable_wait $test_FMRI2

echo "--INFO: Waiting for $test_FMRI3 to come online"
service_wait_state $test_FMRI3 online
if [ $? -ne 0 ]; then
	echo "--DIAG: $assertion: Service $test_FMRI3 did not come online"
	echo "  it's in the '$(svcs -H -o STATE $test_FMRI3)' state."
	exit $STF_FAIL
fi

disable_wait $test_FMRI2

echo "--INFO: Waiting for $test_FMRI3 to stay offline"
service_wait_state $test_FMRI3 offline
if [ $? -ne 0 ]; then
	echo "--DIAG: $assertion: Service $test_FMRI3 did not goto offline"
	echo "  it's in the '$(svcs -H -o STATE $test_FMRI3)' state."
	exit $STF_FAIL
fi

echo "--INFO: Cleaning up service"
cleanup

exit $STF_PASS
