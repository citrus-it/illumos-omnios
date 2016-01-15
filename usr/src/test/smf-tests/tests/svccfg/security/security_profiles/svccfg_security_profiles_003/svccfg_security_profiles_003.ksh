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
# ASSERTION: svccfg_security_profiles_003
#
# DESCRIPTION:
#	A user belonging only to the 'Service Operator' profile cannot
#	create, delete or modify any services, or service instances on
#	the system.
#
# end __stf_assertion__
################################################################################

################################################################################
# start __stf_assertion__
#
# ASSERTION: svccfg_security_profiles_004
#
# DESCRIPTION:
#	A user belonging only to the 'Service Operator' profile can
#	create, delete, modify or change values in all property groups
#	of type 'framework', including the special framework pg named
#	general and the action_authorization property in this pg, but
#	cannot affect pgs of any ther user- or system- defined type.
#
# end __stf_assertion__
################################################################################

readonly prog=${0##*/}
# Assertion ID
readonly assertion=svccfg_security_profiles_003

# Source STF library, GL test library
. ${STF_TOOLS}/include/stf.kshlib
. ${STF_SUITE}/include/gltest.kshlib

readonly me=$(whence -p ${0})
readonly myloc=$(dirname ${me})

# Initialize variables
typeset -i result=$STF_PASS
typeset -i rc=0
typeset retmsg=""
typeset expmsg=""

typeset -i tmp_result=$result

# Begin test
extract_assertion_info ${me}

# ---------------------------------------------------------------------------- #
# PART 1: Services cannot be created with only the 'Service Operator' profile
echo "--INFO: PART 1: Services cannot be created with only the
	${tested_auth}."

tmp_result=$STF_PASS
#
retmsg=$(svccfg add ${GL_TEST_SERVICE}_new 2>&1 1>/dev/null)
rc=$?
expmsg="svccfg: Permission denied."

if [[ $rc != 1 ]] || [[ "$retmsg" != "$expmsg" ]]; then
	tmp_result=$STF_FAIL
	# get rid of service, if created
	svccfg delete ${GL_TEST_SERVICE}_new 2>/dev/null
	echo "--DIAG: svccfg security error during service creation
	EXPECTED: rc = 1, error = '$expmsg'
	RETURNED: rc = $rc, error = '$retmsg'"
fi
#
[[ $result == $STF_PASS ]] && result=$tmp_result
echo "--INFO: [${STF_RESULT_NAMES[$tmp_result]}]"

# ---------------------------------------------------------------------------- #
# PART 2: Instances cannot be created with only the 'Service Operator' profile
echo "--INFO: PART 2: Service instances cannot be created with
	only the ${tested_auth}."

tmp_result=$STF_PASS
#
retmsg=$(svccfg -f - 2>&1 1>/dev/null <<EOF
select ${GL_TEST_SERVICE}
add ${GL_TEST_INSTANCE}_new
EOF)
rc=$?
expmsg="svccfg (<stdin>, line 2): Permission denied."

if [[ $rc != 1 ]] || [[ "$retmsg" != "$expmsg" ]]; then
	tmp_result=$STF_FAIL
	# get rid of instance, if created
	svccfg delete ${GL_TEST_SERVICE}:${GL_TEST_INSTANCE}_new 2>/dev/null
	echo "--DIAG: svccfg security error during instance creation
	EXPECTED: rc = 1, error = '$expmsg'
	RETURNED: rc = $rc, error = '$retmsg'"
fi
#
[[ $result == $STF_PASS ]] && result=$tmp_result
echo "--INFO: [${STF_RESULT_NAMES[$tmp_result]}]"

# ---------------------------------------------------------------------------- #
# PART 3: Framework pgs can be created with only the 'Service Operator' profile
echo "--INFO: PART 3: Framework pgs can be created with only the
	${tested_auth}, including the framework pg
	named 'general'."

echo "--INFO:		Test service pgs"
tmp_result=$STF_PASS
# Test service-pg
retmsg=$(svccfg -f - 2>&1 1>/dev/null <<EOF
select ${GL_TEST_SERVICE}
addpg ${GL_TEST_SVCPG}_fmwk_new framework
addpg general framework
EOF)
rc=$?
expmsg=""
if [[ $rc -ne 0 ]] || [[ "$retmsg" != "$expmsg" ]]; then
	tmp_result=$STF_FAIL
	echo "--DIAG: svccg security error during SVC-PG creation
	EXPECTED: rc = 0, error = '$expmsg'
	RETURNED: rc = $rc, error = '$retmsg'"
fi
#
[[ $result == $STF_PASS ]] && result=$tmp_result
echo "--INFO:		[${STF_RESULT_NAMES[$tmp_result]}]"

# ---------------------------------------------------------------------------- #
echo "--INFO:		Test instance pgs"

tmp_result=$STF_PASS
# Test instance-pg
retmsg=$(svccfg -f - 2>&1 1>/dev/null <<EOF
select svc:/${GL_TEST_SERVICE}:${GL_TEST_INSTANCE}
addpg ${GL_TEST_INSTPG}_fmwk_new framework
addpg general framework
EOF)
rc=$?
expmsg=""
if [[ $rc -ne 0 ]] || [[ "$retmsg" != "$expmsg" ]]; then
	tmp_result=$STF_FAIL
	echo "--DIAG: svccg security error during INST-PG creation
	EXPECTED: rc = 0, error = '$expmsg'
	RETURNED: rc = $rc, error = '$retmsg'"
fi
#
[[ $result == $STF_PASS ]] && result=$tmp_result
echo "--INFO:		[${STF_RESULT_NAMES[$tmp_result]}]"

# ---------------------------------------------------------------------------- #
# PART 4: Non-framework type pgs cannot be created with only the
#	  'Service Operator' profile
echo "--INFO: PART 4: Non-framework type pgs cannot be created
	with only the ${tested_auth}."

tmp_result=$STF_PASS

# Test svc-pgs only
# Run through all other types of pgs, including user-defined types
for pgtype in method dependency application userdefined; do
	retmsg=$(svccfg -f - 2>&1 1>/dev/null <<EOF
select ${GL_TEST_SERVICE}
addpg ${GL_TEST_SVCPG}_${pgtype}_new ${pgtype}
EOF)
	rc=$?
	expmsg="svccfg (<stdin>, line 2): Permission denied."
	if [[ $rc != 1 ]] || [[ "$retmsg" != "$expmsg" ]]; then
		tmp_result=$STF_FAIL
		# Bogus data added, will be cleaned up in cleanup script
		echo "--DIAG: svccfg error during creation of ${pgtype}-type
	service pgs
	EXPECTED: rc = 1, error = '$expmsg'
	RETURNED: rc = $rc, error = '$retmsg'"
	fi
done
#
[[ $result == $STF_PASS ]] && result=$tmp_result
echo "--INFO: [${STF_RESULT_NAMES[$tmp_result]}]"

# ---------------------------------------------------------------------------- #
# PART 5: Users having only the 'Service Operator' profile cannot change
#	  values in non-framework pgs, including pgs of user-defined types
echo "--INFO: PART 5:"
echo "	Users having only the ${tested_auth}
	cannot change values in non-framework type pgs"

tmp_result=$STF_PASS

# Test inst-pgs only
for pgtype in method dependency application userdefined; do
	retmsg=$(svccfg -f - 2>&1 1>/dev/null <<EOF
select svc:/${GL_TEST_SERVICE}:${GL_TEST_INSTANCE}
setprop ${GL_TEST_INSTPG}_${pgtype}/createdby = "${GL_TEST_USERNAME}"
EOF)
	expmsg="svccfg (<stdin>, line 2): Permission denied."
	if [[ $rc != 1 ]] || [[ "$retmsg" != "$expmsg" ]]; then
		tmp_result=$STF_FAIL
		# Bogus data added, will be cleaned up in cleanup script
		echo "--DIAG: svccfg error modifying of ${pgtype}-type inst-pgs
	EXPECTED: rc = 1, error = '$expmsg'
	RETURNED: rc = $rc, error = '$retmsg'"
	fi
done
[[ $result == $STF_PASS ]] && result=$tmp_result
echo "--INFO: [${STF_RESULT_NAMES[$tmp_result]}]"

# ---------------------------------------------------------------------------- #
# PART 6: Users belonging only to the 'Service Operator' profile cannot add
#	  properties to non-framework pgs, including pgs of user-defined types
echo "--INFO: PART 6:"
echo "	Users having only the ${tested_auth}
	cannot add new properties to non-framework pgs"

tmp_result=$STF_PASS

# Test inst-pgs only
for pgtype in method dependency application userdefined; do
	retmsg=$(svccfg -f - 2>&1 1>/dev/null <<EOF
select svc:/${GL_TEST_SERVICE}:${GL_TEST_INSTANCE}
setprop ${GL_TEST_INSTPG}_${pgtype}/modby = astring: "${GL_TEST_USERNAME}"
EOF)
	expmsg="svccfg (<stdin>, line 2): Permission denied."
	if [[ $rc != 1 ]] || [[ "$retmsg" != "$expmsg" ]]; then
		tmp_result=$STF_FAIL
		# Bogus data added, will be cleaned up in cleanup script
		echo "--DIAG: svccfg error modifying of ${pgtype}-type inst-pgs
	EXPECTED: rc = 1, error = '$expmsg'
	RETURNED: rc = $rc, error = '$retmsg'"
	fi
done
[[ $result == $STF_PASS ]] && result=$tmp_result
echo "--INFO: [${STF_RESULT_NAMES[$tmp_result]}]"

# ---------------------------------------------------------------------------- #
# PART 7: Users belonging only to the 'Service Operator' profile cannot delete
#	  pgs of non-framework type, including pgs of user-defined types
echo "--INFO: PART 7:"
echo "	Users having only the ${tested_auth}
	cannot delete non-framework type pgs"

tmp_result=$STF_PASS

# Test svc-pgs only
# Run through all other types of pgs, including user-defined types
for pgtype in method dependency application userdefined; do
	retmsg=$(svccfg -f - 2>&1 1>/dev/null <<EOF
select ${GL_TEST_SERVICE}
delpg ${GL_TEST_SVCPG}_${pgtype}
EOF)
	rc=$?
	expmsg="svccfg (<stdin>, line 2): Permission denied."
	if [[ $rc != 1 ]] || [[ "$retmsg" != "$expmsg" ]]; then
		tmp_result=$STF_FAIL
		echo "--DIAG: svccfg error deleting ${pgtype}-type service pgs
	EXPECTED: rc = 1, error = '$expmsg'
	RETURNED: rc = $rc, error = '$retmsg'"
	fi
done
#
[[ $result == $STF_PASS ]] && result=$tmp_result
echo "--INFO: [${STF_RESULT_NAMES[$tmp_result]}]"

# ---------------------------------------------------------------------------- #
# PART 8: The framework pg property 'general/action_authorization can be
#	  created, modified and deleted by users having the ${tested_auth}
echo "--INFO: PART 8:"
echo "	The framework property 'general/action_authorization'
	can be created, modified and deleted by users having
	the ${tested_auth}."

tmp_result=$STF_PASS
# Test property creation
echo "--INFO:		Test property creation"
retmsg=$(svccfg -f - 2>&1 1>/dev/null <<EOF
select ${GL_TEST_SERVICE}
setprop general/action_authorization = astring: "solaris.smf.modify.dependency"
select ${GL_TEST_SERVICE}:${GL_TEST_INSTANCE}
setprop general/action_authorization = astring: "solaris.smf.modify.dependency"
EOF)
rc=$?
expmsg=""

if [[ $rc != 0 ]] || [[ "$retmsg" != "$expmsg" ]]; then
	tmp_result=$STF_FAIL
	echo "--DIAG: svccfg security error during creation of
	general/action_authorization
	EXPECTED: rc = 0, error = '$expmsg'
	RETURNED: rc = $rc, error = '$retmsg'"
fi
#
[[ $result == $STF_PASS ]] && result=$tmp_result
echo "--INFO:		[${STF_RESULT_NAMES[$tmp_result]}]"

# Test property modification, but only if creation succeeded
if [[ $tmp_result == $STF_PASS ]]; then
	echo "--INFO:		Test property modification"
	retmsg=$(svccfg -f - 2>&1 1>/dev/null <<EOF
select ${GL_TEST_SERVICE}
setprop general/action_authorization = "solaris.smf.modify.method"
select ${GL_TEST_INSTANCE}
setprop general/action_authorization = "solaris.smf.modify.method"
EOF)
	rc=$?
	expmsg=""

	if [[ $rc != 0 ]] || [[ "$retmsg" != "$expmsg" ]]; then
		tmp_result=$STF_FAIL
		echo "--DIAG: svccfg security error during creation of
	general/action_authorization
	EXPECTED: rc = 0, error = '$expmsg'
	RETURNED: rc = $rc, error = '$retmsg'"
		[[ $result == $STF_PASS ]] && result=$tmp_result
		tmp_result="modify-failed"
	fi ### rc != 0
	#
	echo "--INFO:		[${STF_RESULT_NAMES[$result]}]"
fi ### $tmp_result == STF_PASS
#

# Test property deletion, but only of creation succeeded
if [[ $tmp_result != $STF_FAIL ]] && [[ $tmp_result != "modify-failed" ]]; then
	tmp_result=$STF_PASS
	echo "--INFO:		Test property deletion"
	retmsg=$(svccfg -f - 2>&1 1>/dev/null <<EOF
select ${GL_TEST_SERVICE}
delprop general/action_authorization
select ${GL_TEST_INSTANCE}
delprop general/action_authorization
EOF)
	rc=$?
	expmsg=""

	if [[ $rc != 0 ]] || [[ "$retmsg" != "$expmsg" ]]; then
		tmp_result=$STF_FAIL
		echo "--DIAG: svccfg security error during creation of
	general/action_authorization
	EXPECTED: rc = 0, error = '$expmsg'
	RETURNED: rc = $rc, error = '$retmsg'"
	fi ### rc != 0
	#
	[[ $result == $STF_PASS ]] && result=$tmp_result
	echo "--INFO:		[${STF_RESULT_NAMES[$tmp_result]}]"
fi ### tmp_result != STF_FAIL

# ---------------------------------------------------------------------------- #
# PART 9: The framework pg 'general' can be deleted with only the
#	  'Service Operator' profile
echo "--INFO: PART 9:"
echo "	The framework pg 'general' can be deleted with only
	the ${tested_auth}."

# Test only instance-pg
retmsg=$(svccfg -f - 2>&1 1>/dev/null <<EOF
select svc:/${GL_TEST_SERVICE}:${GL_TEST_INSTANCE}
delpg general
unselect
delpg general
EOF)
rc=$?
expmsg=""

if [[ $rc != 0 ]] || [[ "$retmsg" != "$expmsg" ]]; then
	tmp_result=$STF_FAIL
	echo "--DIAG: svccfg security error during creation of
	general/action_authorization
	EXPECTED: rc = 0, error = '$expmsg'
	RETURNED: rc = $rc, error = '$retmsg'"
fi ### rc != 0
#
[[ $result == $STF_PASS ]] && result=$tmp_result
echo "--INFO: [${STF_RESULT_NAMES[$tmp_result]}]"

# ---------------------------------------------------------------------------- #
# PART 10: Instances cannot be deleted with only the 'Service Operator' profile
echo "--INFO: PART 10:"
echo "	Service instances cannot be deleted with only
	the ${tested_auth}."

tmp_result=$STF_PASS
#
retmsg=$(
svccfg delete svc:/${GL_TEST_SERVICE}:${GL_TEST_INSTANCE} 2>&1 1>/dev/null
)
rc=$?
expmsg="svccfg: Permission denied."

if [[ $rc != 1 ]] || [[ "$retmsg" != "$expmsg" ]]; then
	tmp_result=$STF_FAIL
	echo "--DIAG: svccfg security error during instance deletion
	EXPECTED: rc = 1, error = '$expmsg'
	RETURNED: rc = $rc, error = '$retmsg'"
fi
#
[[ $result == $STF_PASS ]] && result=$tmp_result
echo "--INFO: [${STF_RESULT_NAMES[$tmp_result]}]"

# ---------------------------------------------------------------------------- #
# PART 11: Services cannot be deleted with only the ${tested_auth}
echo "--INFO: PART 11:"
echo "	Services cannot be deleted with only
	the ${tested_auth}."

tmp_result=$STF_PASS
#
retmsg=$(svccfg delete ${GL_TEST_SERVICE} 2>&1 1>/dev/null)
rc=$?
expmsg="svccfg: Permission denied."

if [[ $rc != 1 ]] || [[ "$retmsg" != "$expmsg" ]]; then
	tmp_result=$STF_FAIL
	echo "--DIAG: svccfg security error during svc-pg deletion
	EXPECTED: rc = 1, error = '$expmsg'
	RETURNED: rc = $rc, error = '$retmsg'"
fi
#
[[ $result == $STF_PASS ]] && result=$tmp_result
echo "--INFO: [${STF_RESULT_NAMES[$tmp_result]}]"

# ---------------------------------------------------------------------------- #
#
echo "--RSLT: ${STF_RESULT_NAMES[$result]}"
return $result

#
### Script ends here
#
