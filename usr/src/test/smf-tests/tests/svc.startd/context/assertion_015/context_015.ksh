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
# ASSERTION: context_015
# DESCRIPTION:
#  svc.startd will start a method using the profile specified by the
#  name attribute of the method_profile.
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
}

trap cleanup 0 1 2 15

readonly ME=$(whence -p ${0})
readonly MYLOC=$(dirname ${ME})

DATA=$MYLOC

readonly registration_template=$DATA/service_015.xml

extract_assertion_info $ME

# make sure that the svc.startd is running
verify_daemon
if [ $? -ne 0 ]; then
	print -- "--DIAG: $assertion: svc.startd is not executing. Cannot "
	print -- "  continue"
	exit $STF_UNRESOLVED
fi

# Make sure the environment is clean - the test service isn't running
print -- "--INFO: Cleanup any old $test_FMRI state"
service_cleanup $test_service
if [ $? -ne 0 ]; then
	print -- "--DIAG: $assertion: cleanup of a previous instance failed"
	exit $STF_UNRESOLVED
fi

print -- "--INFO: create world read/writeable log file for the service"
rm -f $service_log
touch $service_log
if [ $? -ne 0 ]; then
	print -- "--DIAG: $assertion: could not create log file"
	exit $STF_UNRESOLVED
fi
chmod a+rw $service_log
if [ $? -ne 0 ]; then
	print -- "--DIAG: $assertion: could not make log file world writeable"
	exit $STF_UNRESOLVED
fi
chmod a+rwx $RUNDIR
if [ $? -ne 0 ]; then
	print -- "--DIAG: $assertion: could not make $RUNDIR world rwx"
	exit $STF_UNRESOLVED
fi

print -- "--INFO: initializing service_statefile"
$service_app -s $test_service -i $test_instance -f $service_state -m init
if [ ! -f $service_state ]; then
	print -- "--DIAG: Could not create service state"
	exit $STF_UNRESOLVED
fi
chmod a+rw $service_state

print -- "--INFO: generating manifest for importation into repository"
manifest_generate $registration_template \
	TEST_SERVICE=$test_service \
	TEST_INSTANCE=$test_instance \
	SERVICE_APP=$service_app \
	LOGFILE=$service_log \
	TEST_PROFILENAME="$ctx_profilename" \
	STATEFILE=$service_state > $registration_file

print -- "--INFO: Importing service into repository"
manifest_purgemd5 $registration_file
svccfg -v import $registration_file >$svccfg_errfile 2>&1

if [ $? -ne 0 ]; then
	print -- "--DIAG: $assertion: Unable to import the service $test_FMRI"
	print -- "  error messages from svccfg: \"$(cat $svccfg_errfile)\""
	exit $STF_UNRESOLVED
fi
service_setup=1

print -- "--INFO: Wait for $test_FMRI to come online"
service_wait_state $test_FMRI online
if [ $? -ne 0 ]; then
	print -- "--DIAG: $assertion: Service $test_FMRI did not go online"
	exit $STF_FAIL
fi

print -- "--INFO: Checking start method's profile"
line=`grep_logline_entry $test_service $test_instance start groupname`
if [ $? -ne 0 ]; then
	print -- "--DIAG: Could not find groupname line from "
	print -- " start method '${line}'"
	exit $STF_FAIL
fi
if [ "$line" != $ctx_group ]; then
	print -- "--DIAG: Did not have groupname==$ctx_group in '$line'"
	exit $STF_FAIL
fi
line=`grep_logline_entry $test_service $test_instance start username`
if [ $? -ne 0 ]; then
	print -- "--DIAG: Could not find username line from "
	print -- " start method '${line}'"
	exit $STF_FAIL
fi
if [ "$line" != $ctx_user ]; then
	print -- "--DIAG: Did not have username==$ctx_user in '$line'"
	exit $STF_FAIL
fi
line=`grep_logline_entry $test_service $test_instance start privileges`
if [ $? -ne 0 ]; then
	print -- "--DIAG: Could not find privileges line in start method"
	exit $STF_FAIL
fi
if [ "${line%file_dac_write*}" = "${line}" ]; then
	print -- "--DIAG: Did not have file_dac_write privilege in set '$line'"
	exit $STF_FAIL
fi

print -- "--INFO: Cleaning up service"
cleanup

exit $STF_PASS
