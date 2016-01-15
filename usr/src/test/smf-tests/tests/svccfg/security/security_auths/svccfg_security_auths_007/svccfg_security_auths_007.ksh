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
# ASSERTION: svccfg_security_auths_007
#
# DESCRIPTION:
#	The 'modify_authorization' property of a property group can
#	specify a set of authorizations; users possessing any one of
#	those authorizations can change values in the property group,
#	or create, modify and delete new properties.
#
# end __stf_assertion__
################################################################################

################################################################################
# start __stf_assertion__
#
# ASSERTION: svccfg_security_auths_010
#
# DESCRIPTION:
#	A user possessing only one of the 'modify_authorization' auths
#	can create, delete and change values of all properties in the
#	pg, including 'value_authorization' and 'modify_authorization'.
#
# end __stf_assertion__
################################################################################

################################################################################
# start __stf_assertion__
#
# ASSERTION: svccfg_security_auths_011
#
# DESCRIPTION:
#	The auths in 'value_authorization' and 'modify_authorization'
#	do not grant the ability to delete the property group.
#	(only 'modify_authorization' is tested here).
#
# end __stf_assertion__
################################################################################

readonly prog=${0##*/}

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

# Assertion ID
readonly assertion=svccfg_security_auths_007

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
# PART 1: Try to modify a dependency-type pg
echo "--INFO: Modification of a dependency-type pg which has
	modify_authorization = ${dep_auths}
	is allowed if the user has 
	authorizations = ${tested_auths}"

propval=$(echo "select ${GL_TEST_SERVICE}
listprop ${GL_TEST_SVCPG}_dependency/modify_authorization" | \
svccfg -f - 2>/dev/null | awk '{ print $3 }')

tmp_result=$STF_PASS
# Test svc pg: modify the value of the property named 'createdby'
retmsg=$(svccfg -f - 2>&1 1>/dev/null <<EOF
select ${GL_TEST_SERVICE}
setprop ${GL_TEST_SVCPG}_dependency/createdby = "${GL_TEST_USERNAME}"
EOF)
rc=$?
expmsg=""
if [[ $rc -ne 0 ]] || [[ "$retmsg" != "$expmsg" ]]; then
	tmp_result=$STF_FAIL
	echo "--DIAG: User with auths
		${tested_auths}
	could not modify pg ${GL_TEST_SVCPG}_dependency with
		modify_authorization = ${propval}
	EXPECTED: rc = 0, error = '$expmsg'
	RETURNED: rc = $rc, error = '$retmsg'"
fi

# If we have successfully exited from svccfg above, verify that the
# property was actually modified
if [[ $tmp_result != $STF_FAIL ]]; then
	propval=$(echo "select ${GL_TEST_SERVICE}
listprop ${GL_TEST_SVCPG}_dependency/createdby" | \
	svccfg -f - 2>/dev/null | awk '{ print $3 }')

	if [[ $propval != $GL_TEST_USERNAME ]]; then
		tmp_result=$STF_FAIL

		echo "--DIAG: svccfg setprop command succeeded but
	property value was not modified
	(property: ${GL_TEST_SVCPG}_dependency/createdby)
	EXPECTED: value = $GL_TEST_USERNAME
	RETURNED: value = $propval"

	fi
fi

[[ $result != $STF_FAIL ]] && result=$tmp_result
echo "--INFO: [${STF_RESULT_NAMES[$tmp_result]}]"

# ---------------------------------------------------------------------------- #
#
# PART 2: Try to modify an application-type pg
echo "--INFO: Modification of an application-type pg which has
	modify_authorization = ${app_auths}
	is allowed if the user has
	authorizations = ${tested_auths}"

propval=$(echo "select ${GL_TEST_SERVICE}
listprop ${GL_TEST_SVCPG}_application/modify_authorization" | \
svccfg -f - 2>/dev/null | awk '{ print $3 }')

tmp_result=$STF_PASS
# Test svc pg: modify the value of the property named 'createdby'
retmsg=$(svccfg -f - 2>&1 1>/dev/null <<EOF
select ${GL_TEST_SERVICE}
setprop ${GL_TEST_SVCPG}_application/createdby = "${GL_TEST_USERNAME}"
EOF)
rc=$?
expmsg=""
if [[ $rc -ne 0 ]] || [[ "$retmsg" != "$expmsg" ]]; then
	tmp_result=$STF_FAIL
	echo "--DIAG: User with auths
		${tested_auths}
	could not modify pg ${GL_TEST_SVCPG}_application with
		modify_authorization = ${propval}
	EXPECTED: rc = 0, error = '$expmsg'
	RETURNED: rc = $rc, error = '$retmsg'"
fi

# If we have successfully exited from svccfg above, verify that the
# property was actually modified
if [[ $tmp_result != $STF_FAIL ]]; then
	propval=$(echo "select ${GL_TEST_SERVICE}
listprop ${GL_TEST_SVCPG}_application/createdby" | \
	svccfg -f - 2>/dev/null | awk '{ print $3 }')

	if [[ $propval != $GL_TEST_USERNAME ]]; then
		tmp_result=$STF_FAIL

		echo "--DIAG: svccfg setprop command succeeded but
	property value was not modified
	(property: ${GL_TEST_SVCPG}_application/createdby)
	EXPECTED: value = $GL_TEST_USERNAME
	RETURNED: value = $propval"

	fi
fi

[[ $result != $STF_FAIL ]] && result=$tmp_result
echo "--INFO: [${STF_RESULT_NAMES[$tmp_result]}]"

# ---------------------------------------------------------------------------- #
#
# PART 3: Try to modify a userdefined-type pg
echo "--INFO: Modification of a user defined-type pg which has
	modify_authorization = ${all_auths}
	is allowed if the user has
	authorizations = ${tested_auths}"

propval=$(echo "select ${GL_TEST_SERVICE}
listprop ${GL_TEST_SVCPG}_userdefined/modify_authorization" | \
svccfg -f - 2>/dev/null | awk '{ print $3 }')

tmp_result=$STF_PASS
# Test svc pg: modify the value of the property named 'createdby'
retmsg=$(svccfg -f - 2>&1 1>/dev/null <<EOF
select ${GL_TEST_SERVICE}
setprop ${GL_TEST_SVCPG}_userdefined/createdby = "${GL_TEST_USERNAME}"
EOF)
rc=$?
expmsg=""
if [[ $rc -ne 0 ]] || [[ "$retmsg" != "$expmsg" ]]; then
	tmp_result=$STF_FAIL
	echo "--DIAG: User with auths
		${tested_auths}
	could not modify pg ${GL_TEST_SVCPG}_userdefined with
		modify_authorization = ${propval}
	EXPECTED: rc = 0, error = '$expmsg'
	RETURNED: rc = $rc, error = '$retmsg'"
fi

# If we have successfully exited from svccfg above, verify that the
# property was actually modified
if [[ $tmp_result != $STF_FAIL ]]; then
	propval=$(echo "select ${GL_TEST_SERVICE}
listprop ${GL_TEST_SVCPG}_userdefined/createdby" | \
	svccfg -f - 2>/dev/null | awk '{ print $3 }')

	if [[ $propval != $GL_TEST_USERNAME ]]; then
		tmp_result=$STF_FAIL

		echo "--DIAG: svccfg setprop command succeeded but
	property value was not modified
	(property: ${GL_TEST_SVCPG}_userdefined/createdby)
	EXPECTED: value = $GL_TEST_USERNAME
	RETURNED: value = $propval"

	fi
fi

[[ $result != $STF_FAIL ]] && result=$tmp_result
echo "--INFO: [${STF_RESULT_NAMES[$tmp_result]}]"

# ---------------------------------------------------------------------------- #
# PART 4: New properties can be created
echo "--INFO: Authorizations specified in 'modify_authorizations'
	enable the user to create new properties in the pg"

tmp_result=$STF_PASS

# Dependency-type pg
retmsg=$(svccfg -f - 2>&1 1>/dev/null <<EOF
select svc:/${GL_TEST_SERVICE}:${GL_TEST_INSTANCE}
setprop ${GL_TEST_INSTPG}_dependency/modby = astring: "${GL_TEST_USERNAME}"
EOF)
rc=$?
expmsg=""
if [[ $rc -ne 0 ]] || [[ "$retmsg" != "$expmsg" ]]; then
	tmp_result=$STF_FAIL
	echo "--DIAG: User with auths
		${tested_auths}
	could not create new property in pg 
		${GL_TEST_SVCPG}_dependency
	EXPECTED: rc = 0, error = '$expmsg'
	RETURNED: rc = $rc, error = '$retmsg'"
fi

[[ $result != $STF_FAIL ]] && result=$tmp_result
echo "--INFO: [${STF_RESULT_NAMES[$tmp_result]}]"

# ---------------------------------------------------------------------------- #
# PART 5: Deleting the property group is not allowed
echo "--INFO: Authorizations specified in 'modify_authorizations'
	do not grant the ability to delete the pg"

tmp_result=$STF_PASS
# Dependency-type pg
retmsg=$(svccfg -f - 2>&1 1>/dev/null <<EOF
select svc:/${GL_TEST_SERVICE}:${GL_TEST_INSTANCE}
delpg ${GL_TEST_INSTPG}_dependency
EOF)
rc=$?
expmsg="svccfg (<stdin>, line 2): Permission denied."
if [[ $rc -ne 1 ]] || [[ "$retmsg" != "$expmsg" ]]; then
	tmp_result=$STF_FAIL
	echo "--DIAG: svccfg security error deleting inst pg
	${GL_TEST_INSTPG}_dependency
	EXPECTED: rc = 1, error = '$expmsg'
	RETURNED: rc = $rc, error = '$retmsg'"
fi

# Application-type pg
retmsg=$(svccfg -f - 2>&1 1>/dev/null <<EOF
select svc:/${GL_TEST_SERVICE}:${GL_TEST_INSTANCE}
delpg ${GL_TEST_INSTPG}_application
EOF)
rc=$?
expmsg="svccfg (<stdin>, line 2): Permission denied."
if [[ $rc -ne 1 ]] || [[ "$retmsg" != "$expmsg" ]]; then
	tmp_result=$STF_FAIL
	echo "--DIAG: svccfg security error deleting inst pg
	${GL_TEST_INSTPG}_application
	EXPECTED: rc = 1, error = '$expmsg'
	RETURNED: rc = $rc, error = '$retmsg'"
fi

# User defined-type pg
retmsg=$(svccfg -f - 2>&1 1>/dev/null <<EOF
select svc:/${GL_TEST_SERVICE}:${GL_TEST_INSTANCE}
delpg ${GL_TEST_INSTPG}_userdefined
EOF)
rc=$?
expmsg="svccfg (<stdin>, line 2): Permission denied."
if [[ $rc -ne 1 ]] || [[ "$retmsg" != "$expmsg" ]]; then
	tmp_result=$STF_FAIL
	echo "--DIAG: svccfg security error deleting inst pg
	${GL_TEST_INSTPG}_userdefined
	EXPECTED: rc = 1, error = '$expmsg'
	RETURNED: rc = $rc, error = '$retmsg'"
fi

[[ $result != $STF_FAIL ]] && result=$tmp_result
echo "--INFO: [${STF_RESULT_NAMES[$tmp_result]}]"

# ---------------------------------------------------------------------------- #
# PART 6: Values of 'value_authorization' can be modified and the property
#	  can be deleted
echo "--INFO: Authorizations in 'modify_authorization' enable the
	modification of values in 'value_authorization'"

tmp_result=$STF_PASS
#
new_auth="solaris.smf.manage"

retmsg=$(svccfg -f - 2>&1 1>/dev/null <<EOF
select ${GL_TEST_SERVICE}
setprop ${GL_TEST_SVCPG}_application/value_authorization = "$new_auth"
EOF)
rc=$?
expmsg=""
if [[ $rc -ne 0 ]] || [[ "$retmsg" != "$expmsg" ]]; then
	tmp_result=$STF_FAIL
	echo "--DIAG: svccfg security error modifying property
	${GL_TEST_SVCPG}_application/value_authorization
	EXPECTED: rc = 1, error = '$expmsg'
	RETURNED: rc = $rc, error = '$retmsg'"
fi

# Verify that the property was not actually modified.
if [[ $tmp_result != $STF_FAIL ]]; then
	propval=$(echo "select ${GL_TEST_SERVICE}
listprop ${GL_TEST_SVCPG}_application/value_authorization" | \
	svccfg -f - 2>/dev/null | awk '{ print $3 }')

	if [[ $propval != $new_auth ]]; then
		tmp_result=$STF_FAIL

		echo "--DIAG: svccfg setprop succeeded, but 
	property value was not modified correctly
	(property: ${GL_TEST_SVCPG}_application/value_authorization)
	EXPECTED: value = $new_auth
	RETURNED: value = $propval"

	fi
fi
#
[[ $result != $STF_FAIL ]] && result=$tmp_result
echo "--INFO: [${STF_RESULT_NAMES[$tmp_result]}]"

# ---------------------------------------------------------------------------- #
#
echo "--INFO: Authorizations in 'modify_authorization' enable the
	deletion of the 'value_authorization' property"

tmp_result=$STF_PASS
#
retmsg=$(svccfg -f - 2>&1 1>/dev/null <<EOF
select svc:/${GL_TEST_SERVICE}:${GL_TEST_INSTANCE}
delprop ${GL_TEST_INSTPG}_dependency/value_authorization
EOF)
rc=$?
expmsg=""
if [[ $rc -ne 0 ]] || [[ "$retmsg" != "$expmsg" ]]; then
	tmp_result=$STF_FAIL
	echo "--DIAG: svccfg security error deleting property
	${GL_TEST_INSTPG}_dependency/value_authorization
	EXPECTED: rc = 1, error = '$expmsg'
	RETURNED: rc = $rc, error = '$retmsg'"
fi

[[ $result != $STF_FAIL ]] && result=$tmp_result
echo "--INFO: [${STF_RESULT_NAMES[$tmp_result]}]"

# ---------------------------------------------------------------------------- #
# PART 7: Values of 'modify_authorization' can be modified and the property
#	  can be deleted
echo "--INFO: Authorizations in 'modify_authorization' enable the
	modification of values in 'modify_authorization'"

tmp_result=$STF_PASS
#
new_auth="solaris.smf.manage"

retmsg=$(svccfg -f - 2>&1 1>/dev/null <<EOF
select ${GL_TEST_SERVICE}
setprop ${GL_TEST_SVCPG}_userdefined/modify_authorization = "$new_auth"
EOF)
rc=$?
expmsg=""
if [[ $rc -ne 0 ]] || [[ "$retmsg" != "$expmsg" ]]; then
	tmp_result=$STF_FAIL
	echo "--DIAG: svccfg security error modifying property
	${GL_TEST_SVCPG}_userdefined/modify_authorization
	EXPECTED: rc = 1, error = '$expmsg'
	RETURNED: rc = $rc, error = '$retmsg'"
fi

# Verify that the property was not actually modified.
if [[ $tmp_result != $STF_FAIL ]]; then
	propval=$(echo "select ${GL_TEST_SERVICE}
listprop ${GL_TEST_SVCPG}_userdefined/modify_authorization" | \
	svccfg -f - 2>/dev/null | awk '{ print $3 }')

	if [[ $propval != $new_auth ]]; then
		tmp_result=$STF_FAIL

		echo "--DIAG: svccfg setprop succeeded, but 
	property value was not modified correctly
	(property: ${GL_TEST_SVCPG}_userdefined/modify_authorization)
	EXPECTED: value = $new_auth
	RETURNED: value = $propval"

	fi
fi
#
[[ $result != $STF_FAIL ]] && result=$tmp_result
echo "--INFO: [${STF_RESULT_NAMES[$tmp_result]}]"

# ---------------------------------------------------------------------------- #
#
echo "--INFO: Authorizations in 'modify_authorization' enable the
	deletion of the 'modify_authorization' property"

tmp_result=$STF_PASS
#
retmsg=$(svccfg -f - 2>&1 1>/dev/null <<EOF
select svc:/${GL_TEST_SERVICE}:${GL_TEST_INSTANCE}
delprop ${GL_TEST_INSTPG}_application/modify_authorization
EOF)
rc=$?
expmsg=""
if [[ $rc -ne 0 ]] || [[ "$retmsg" != "$expmsg" ]]; then
	tmp_result=$STF_FAIL
	echo "--DIAG: svccfg security error deleting property
	${GL_TEST_INSTPG}_application/modify_authorization
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
