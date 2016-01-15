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

# A script to clean up any stray instances of service_app before starting
# the svccfg apply tests.

# Load the STF library
. ${STF_TOOLS}/include/stf.kshlib
# for find_nontransient_pids
. ${STF_SUITE}/include/gltest.kshlib

# This is our test service app name
typeset SERVICE_APP=${STF_SUITE}/tests/bin/${STF_EXECUTE_MODE}/service_app
typeset -i result=$STF_PASS

# Verify that no instance of ${SERVICE_APP} is currently running
SVC_APP=${SERVICE_APP##*/}
typeset pids=$(find_nontransient_pids ${SVC_APP})
typeset -i num_apps=$?
if [[ $num_apps -eq 0 ]]; then
	echo "--INFO: Success
	No instance of ${SVC_APP} was found running on the system"
	
	echo "--RSLT: ${STF_RESULT_NAMES[$STF_PASS]}"
	exit $STF_PASS
fi

# If we are here, at least one stray instance was found.
#
echo "--INFO: Attempting to kill all ${SVC_APP} processes"
pkill -9 -z $(zonename) ${SVC_APP} >/dev/null 2>&1

# If we still couldn't kill all service_app processes, it will 
# interfere with the rest of the tests in this directory.  Therefore,
# we exit with UNRESOLVED, so that the tests are not even executed.
num_apps=$(pgrep -z $(zonename) $SVC_APP 2>/dev/null | wc -l)
[[ $num_apps -ne 0 ]] && {
	echo "--DIAG: Fatal error: pkill failed
	EXPECTED: All instances of ${SVC_APP} were killed
	RETURNED: $num_apps instances are still running"

	result=$STF_UNRESOLVED
}

echo "--RSLT: ${STF_RESULT_NAMES[$result]}"
exit $result
