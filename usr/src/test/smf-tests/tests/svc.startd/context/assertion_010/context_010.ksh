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
# ASSERTION: context_010
# DESCRIPTION:
#  A method with a group id specified that does not exist in the group
#  name service shall be executed under that group ID.
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

# Find an unused group:
test_id=0
while [ -n "`getent group $test_id`" ]; do
	test_id=$((test_id + 1))
done

DATA=$MYLOC

readonly registration_template=$DATA/service_010.xml

extract_assertion_info $ME

# make sure that the svc.startd is running
verify_daemon
if [ $? -ne 0 ]; then
	echo "--DIAG: $assertion: svc.startd is not executing. Cannot "
	echo "  continue"
	exit $STF_UNRESOLVED
fi

# Make sure the environment is clean - the test service isn't running
echo "--INFO: Cleanup any old $test_FMRI state"
service_cleanup $test_service
if [ $? -ne 0 ]; then
	echo "--DIAG: $assertion: cleanup of a previous instance failed"
	exit $STF_UNRESOLVED
fi

echo "--INFO: create world read/writeable log file for the service"
rm -f $service_log
touch $service_log
if [ $? -ne 0 ]; then
	echo "--DIAG: $assertion: could not create log file"
	exit $STF_UNRESOLVED
fi
chmod a+rw $service_log
if [ $? -ne 0 ]; then
	echo "--DIAG: $assertion: could not make log file world writeable"
	exit $STF_UNRESOLVED
fi
chmod a+rwx $RUNDIR
if [ $? -ne 0 ]; then
	echo "--DIAG: $assertion: could not make $RUNDIR world rwx"
	exit $STF_UNRESOLVED
fi

echo "--INFO: generating manifest for importation into repository"
manifest_generate $registration_template \
	TEST_SERVICE=$test_service \
	TEST_INSTANCE=$test_instance \
	SERVICE_APP=$service_app \
	LOGFILE=$service_log \
	TEST_USER=$ctx_user \
	TEST_GROUP=$test_id \
	STATEFILE=$service_state > $registration_file
manifest_zone_clean $registration_file

echo "--INFO: Importing service into repository"
manifest_purgemd5 $registration_file
svccfg -v import $registration_file >$svccfg_errfile 2>&1

if [ $? -ne 0 ]; then
	echo "--DIAG: $assertion: Unable to import the service $test_FMRI"
	echo "  error messages from svccfg: \"$(cat $svccfg_errfile)\""
	exit $STF_UNRESOLVED
fi
service_setup=1

echo "--INFO: Wait for $test_FMRI to go to online"
service_wait_state $test_FMRI online
if [ $? -ne 0 ]; then
	echo "--DIAG: $assertion: Service $test_FMRI did not go to"
	echo "  online State was: '$(svcs -H -o STATE $test_FMRI)'"
	exit $STF_FAIL
fi

echo "--INFO: Verifying group id was $test_id"
line=$(grep_logline_entry $test_service $test_instance start groupid)
if [ $? -ne 0 ]; then
	echo "--DIAG: $assertion: could not find groupid in the service log"
	exit $STF_FAIL
fi

if [ "$line" != "$test_id" ]; then
	echo "--DIAG: $assertion: group id was '$line' instead of '$test_id'"
	exit $STF_FAIL
fi

echo "--INFO: Cleaning up service"
cleanup

exit $STF_PASS
