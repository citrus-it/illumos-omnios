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

################################################################################
# start __stf_assertion__
#
# ASSERTION: svccfg_security_auths_012
#
# DESCRIPTION:
#	A user possessing the 'solaris.smf.manage' auth can modify
#	the 'action_authorization' property of the framework pg 
#	named 'general, regardless of whether s/he also has the
#	'solaris.smf.modify.framework' authorization.
#
# end __stf_assertion__
################################################################################

readonly prog=${0##*/}
readonly assertion=svccfg_security_auths_012

# Source STF library, GL test library
# Calculate the location of the current test's ksh library
bname=${STF_EXEC##*/}; exec_parent=${STF_EXEC%%/$bname}
parent=${exec_parent##*/}

INC_PARENT=${exec_parent%%/$parent}
INC_FILE=${INC_PARENT##*/}

# Source STF library, GL test library, current directory's test library
. ${STF_TOOLS}/include/stf.kshlib
. ${STF_SUITE}/include/gltest.kshlib
. ${STF_SUITE}/${INC_PARENT}/include/${INC_FILE}.kshlib

readonly me=$(whence -p ${0})
readonly myloc=$(dirname ${me})

# Initialize variables
typeset -i result=$STF_PASS
typeset -i tmp_result=$result
typeset -i rc=0
typeset retmsg=""
typeset expmsg=""

# Begin test
extract_assertion_info ${me}

# ---------------------------------------------------------------------------- #
# PART 1: Try to modify the action_authorization property
echo "--INFO: List of all user's authorizations:"
echo "	$(auths ${GL_TEST_USERNAME})"

echo "--INFO: List of user's SMF authorizations:"
echo "	${tested_auths}"

echo "--INFO: Get the current value of 'general/action_authorization'
	under service ${GL_TEST_SERVICE}"
propval=$(echo "select ${GL_TEST_SERVICE}
listprop general/action_authorization" | \
svccfg -f - 2>/dev/null | awk '{ print $3 }')

tmp_result=$STF_PASS
# Test svc pg: modify the value of the property
new_propval="solaris.smf.modify.application"
retmsg=$(svccfg -f - 2>&1 1>/dev/null <<EOF
select ${GL_TEST_SERVICE}
setprop general/action_authorization = "$new_propval"
EOF)
rc=$?
expmsg=""
if [[ $rc -ne 0 ]] || [[ "$retmsg" != "$expmsg" ]]; then
	tmp_result=$STF_FAIL
	echo "--DIAG: User with auths
		${tested_auths}
	could not modify framework pg general and set
		action_authorization = ${new_propval}
	EXPECTED: rc = 0, error = '$expmsg'
	RETURNED: rc = $rc, error = '$retmsg'"
fi

# If we have successfully exited from svccfg above, verify that the
# property was actually modified
if [[ $tmp_result != $STF_FAIL ]]; then
	propval=$(echo "select ${GL_TEST_SERVICE}
listprop general/action_authorization" | \
	svccfg -f - 2>/dev/null | awk '{ print $3 }')

	if [[ $propval != $new_propval ]]; then
		tmp_result=$STF_FAIL

		echo "--DIAG: svccfg setprop command succeeded but
	property value was not modified
	(property: general/action_authorization)
	EXPECTED: value = $new_propval
	RETURNED: value = $propval"

	fi
fi

[[ $result != $STF_FAIL ]] && result=$tmp_result
echo "--INFO: [${STF_RESULT_NAMES[$tmp_result]}]"

# ---------------------------------------------------------------------------- #
# PART 2: Deleting the property is not allowed
echo "--INFO: The 'solaris.smf.manage' authorization grants
	the ability to delete the 'action_authorization'
	property of the framework pg 'general'"

tmp_result=$STF_PASS
#
retmsg=$(svccfg -f - 2>&1 1>/dev/null <<EOF
select svc:/${GL_TEST_SERVICE}:${GL_TEST_INSTANCE}
delprop general/action_authorization
EOF)
rc=$?
expmsg=""
if [[ $rc -ne 0 ]] || [[ "$retmsg" != "$expmsg" ]]; then
	tmp_result=$STF_FAIL
	echo "--DIAG: svccfg security error deleting inst-pg
	property
		general/action_authorization
	EXPECTED: rc = 1, error = '$expmsg'
	RETURNED: rc = $rc, error = '$retmsg'"
fi

[[ $result != $STF_FAIL ]] && result=$tmp_result
echo "--INFO: [${STF_RESULT_NAMES[$tmp_result]}]"

# ---------------------------------------------------------------------------- #
# PART 3: Deleting the pg is not allowed
echo "--INFO: The 'solaris.smf.manage' authorization does not
	grant the ability to delete the framework pg 'general'"

tmp_result=$STF_PASS
# Dependency-type pg
retmsg=$(svccfg -f - 2>&1 1>/dev/null <<EOF
select svc:/${GL_TEST_SERVICE}:${GL_TEST_INSTANCE}
delpg general
EOF)
rc=$?
expmsg="svccfg (<stdin>, line 2): Permission denied."
if [[ $rc -ne 1 ]] || [[ "$retmsg" != "$expmsg" ]]; then
	tmp_result=$STF_FAIL
	echo "--DIAG: svccfg security error deleting inst pg
		general
	EXPECTED: rc = 1, error = '$expmsg'
	RETURNED: rc = $rc, error = '$retmsg'"
fi

[[ $result != $STF_FAIL ]] && result=$tmp_result
echo "--INFO: [${STF_RESULT_NAMES[$tmp_result]}]"

# ---------------------------------------------------------------------------- #
#
echo "--RSLT: ${STF_RESULT_NAMES[$result]}"
return $result

#
### Script ends here
#
