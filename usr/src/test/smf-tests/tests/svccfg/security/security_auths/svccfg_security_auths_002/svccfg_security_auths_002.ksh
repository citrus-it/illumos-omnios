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
# ASSERTION: svccfg_security_auths_002
#
# DESCRIPTION:
#	A user having only the 'solaris.smf.modify.method' authorization
#	can create, delete or modify property groups of type 'method',
#	but not of other types.
#
# end __stf_assertion__
################################################################################

readonly prog=${0##*/}

# Source STF library, GL test library
. ${STF_TOOLS}/include/stf.kshlib
. ${STF_SUITE}/include/gltest.kshlib

# Assertion ID
readonly assertion=svccfg_security_auths_002

readonly me=$(whence -p ${0})
readonly myloc=$(dirname ${me})

# Initialize variables
typeset -i result=$STF_PASS
typeset -i tmp_result=$result
typeset -i rc=0
typeset retmsg=""
typeset expmsg=""
readonly tested_authorization="'solaris.smf.modify.method'"

# Begin test
extract_assertion_info ${me}

# ---------------------------------------------------------------------------- #
# PART 1: 'method'-type property groups can be created
echo "--INFO: PART 1:"
echo "	Users having only the ${tested_authorization} auth
	can create 'method'-type property groups"

tmp_result=$STF_PASS

# Test service-pg
retmsg=$(svccfg -f - 2>&1 1>/dev/null <<EOF
select ${GL_TEST_SERVICE}
addpg ${GL_TEST_SVCPG}_new method
EOF)
rc=$?
expmsg=""

if [[ $rc != 0 ]] || [[ "$retmsg" != "$expmsg" ]]; then
	tmp_result=$STF_FAIL
	# Could not add property group.
	echo "--DIAG: svccfg failed to add method-type pg ${GL_TEST_SVCPG}_new
	EXPECTED: rc = 0, error = '$expmsg'
	RETURNED: rc = $rc, error = '$retmsg'"
fi

# Test instance-pg
retmsg=$(svccfg -f - 2>&1 1>/dev/null <<EOF
select svc:/${GL_TEST_SERVICE}:${GL_TEST_INSTANCE}
addpg ${GL_TEST_INSTPG}_new method
EOF)
rc=$?
expmsg=""
if [[ $rc != 0 ]] || [[ "$retmsg" != "$expmsg" ]]; then
	tmp_result=$STF_FAIL
	# Could not add property group.
	echo "--DIAG: svccfg failed to add method-type pg ${GL_TEST_INSTPG}_new
	EXPECTED: rc = 0, error = '$expmsg'
	RETURNED: rc = $rc, error = '$retmsg'"
fi
[[ $result == $STF_PASS ]] && result=$tmp_result
echo "--INFO: [${STF_RESULT_NAMES[$tmp_result]}]"

# ---------------------------------------------------------------------------- #
# PART 2: Existing props in 'method' pgs can be modified by users with
#	  'solaris.smf.modify.method'
echo "--INFO: PART 2:"
echo "	Users having only the ${tested_authorization} auth
	can modify existing properties in 'method'-type pgs"

tmp_result=$STF_PASS

# Test only svc-pg: reset the value of the "createdby" property to 
# ${GL_TEST_USERNAME}
retmsg=$(svccfg -f - 2>&1 1>/dev/null <<EOF
select ${GL_TEST_SERVICE}
setprop ${GL_TEST_SVCPG}_method/createdby = "${GL_TEST_USERNAME}"
EOF)
rc=$?
expmsg=""
if [[ $rc != 0 ]] || [[ "$retmsg" != "$expmsg" ]]; then
	tmp_result=$STF_FAIL
	# Could not modify pg
	echo "--DIAG: svccfg failed to modify pg ${GL_TEST_SVCPG}_method
	EXPECTED: rc = 0, error = '$expmsg'
	RETURNED: rc = $rc, error = '$retmsg'"
fi

if [[ $tmp_result != $STF_FAIL ]]; then
	# Verify that the property was actually modified
	typeset propval=$(echo "select ${GL_TEST_SERVICE}
			listprop ${GL_TEST_SVCPG}_method/createdby" | \
			svccfg -f - 2>/dev/null | \
			awk '{ printf $3 }')
	rc=$?
	expmsg="${GL_TEST_USERNAME}"

	if [[ $rc != 0 ]] || [[ "$propval" != "$expmsg" ]]; then
		tmp_result=$STF_FAIL
		# Couldn't actually modify the pg
		echo "--DIAG: svccfg returned success from pg modification, but
	did not actually modify the pg!
	EXPECTED: rc = 0, new property value = $expmsg
	RETURNED: rc = $rc, new property value = $propval"
	fi
fi
[[ $result == $STF_PASS ]] && result=$tmp_result
echo "--INFO: [${STF_RESULT_NAMES[$tmp_result]}]"

# ---------------------------------------------------------------------------- #
# PART 3: New properties can be added to method pgs by users with the
#	  ${tested_authorization} auth
echo "--INFO: PART 3:"
echo "	Users having the ${tested_authorization} auth
	can add new properties can be added to method-type pgs"

tmp_result=$STF_PASS

# Test only instance-pg: create a new prop named 'modby' and set to
# ${GL_TEST_USERNAME}
retmsg=$(svccfg -f - 2>&1 1>/dev/null <<EOF
select svc:/${GL_TEST_SERVICE}:${GL_TEST_INSTANCE}
setprop ${GL_TEST_INSTPG}_method/modby = astring: "${GL_TEST_USERNAME}"
EOF)
rc=$?
expmsg=""
if [[ $rc != 0 ]] || [[ "$retmsg" != "$expmsg" ]]; then
	tmp_result=$STF_FAIL
	# Could not modify pg
	echo "--DIAG: svccfg failed to add pg ${GL_TEST_INSTPG}_method
	EXPECTED: rc = 0, error = '$expmsg'
	RETURNED: rc = $rc, error = '$retmsg'"
fi

if [[ $tmp_result != $STF_FAIL ]]; then
	# Verify that the property was actually modified
	typeset propval=$(echo "
			select svc:/${GL_TEST_SERVICE}:${GL_TEST_INSTANCE}
			listprop ${GL_TEST_INSTPG}_method/modby" | \
			svccfg -f - 2>/dev/null | \
			awk '{ printf $3 }')
	rc=$?
	expmsg="${GL_TEST_USERNAME}"

	if [[ $rc != 0 ]] || [[ "$propval" != "$expmsg" ]]; then
		tmp_result=$STF_FAIL
		# Couldn't actually modify the pg
		echo "--DIAG: svccfg returned success from pg modification, but
	did not actually modify the pg!
	EXPECTED: rc = 0, new property value = $expmsg
	RETURNED: rc = $rc, new property value = $propval"
	fi
fi ### result != STF_FAIL

[[ $result == $STF_PASS ]] && result=$tmp_result
echo "--INFO: [${STF_RESULT_NAMES[$tmp_result]}]"

# ---------------------------------------------------------------------------- #
# PART 4: 'method'-type pgs can be deleted with ${tested_authorization} auth
echo "--INFO: PART 4:"
echo "	Users having only the ${tested_authorization} auth
	can delete method-type pgs from the repository"

tmp_result=$STF_PASS

# Delete service-pg added during test
retmsg=$(svccfg -f - 2>&1 1>/dev/null <<EOF
select ${GL_TEST_SERVICE}
delpg ${GL_TEST_SVCPG}_new
EOF)
rc=$?
expmsg=""
if [[ $rc != 0 ]] || [[ "$retmsg" != "$expmsg" ]]; then
	tmp_result=$STF_FAIL
	# Could not delete pg
	echo "--DIAG: svccfg failed to delete pg ${GL_TEST_SVCPG}_new
	EXPECTED: rc = 0, error = '$expmsg'
	RETURNED: rc = $rc, error = '$retmsg'"
fi

# Delete inst-pg added during test
retmsg=$(svccfg -f - 2>&1 1>/dev/null <<EOF
select svc:/${GL_TEST_SERVICE}:${GL_TEST_INSTANCE}
delpg ${GL_TEST_INSTPG}_new
EOF)
rc=$?
expmsg=""
if [[ $rc != 0 ]] || [[ "$retmsg" != "$expmsg" ]]; then
	tmp_result=$STF_FAIL
	# Could not delete pg
	echo "--DIAG: svccfg failed to delete pg ${GL_TEST_INSTPG}_new
	EXPECTED: rc = 0, error = '$expmsg'
	RETURNED: rc = $rc, error = '$retmsg'"
fi
[[ $result == $STF_PASS ]] && result=$tmp_result
echo "--INFO: [${STF_RESULT_NAMES[$tmp_result]}]"

# ---------------------------------------------------------------------------- #
# PART 5: Non-'method' pgs cannot be created by users having only the 
#	  ${tested_authorization} auth
echo "--INFO: PART 5:"
echo "	Users having only the ${tested_authorization} auth cannot
	create non-'method' type pgs, including user-defined type pgs"

tmp_result=$STF_PASS

# Test svc-pgs only
# Run through all other types of pgs, including user-defined types
for pgtype in dependency framework application userdefined; do
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
[[ $result == $STF_PASS ]] && result=$tmp_result
echo "--INFO: [${STF_RESULT_NAMES[$tmp_result]}]"

# ---------------------------------------------------------------------------- #
# PART 6: Users having only the ${tested_authorization} auth cannot change
#	  values in non-method pgs, including pgs of user-defined types
echo "--INFO: PART 6:"
echo "	Users having only the ${tested_authorization} auth cannot
	change values in non-method pgs, including user-defined type pgs"

tmp_result=$STF_PASS

# Test inst-pgs only
for pgtype in dependency framework application userdefined; do
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
# PART 7: Users having only the ${tested_authorization} auth cannot add
#	  properties to non-method pgs, including pgs of user-defined types
echo "--INFO: PART 7:"
echo "	Users having only the ${tested_authorization} auth cannot
	add new props to non-method type pgs, including pgs of 
	user-defined type"

tmp_result=$STF_PASS

# Test inst-pgs only
for pgtype in dependency framework application userdefined; do
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
# PART 8: Users having only the ${tested_authorization} auth cannot delete
#	  pgs of non-method type, including pgs of user-defined types
echo "--INFO: PART 8:"
echo "	Users having only the ${tested_authorization} auth cannot
	delete non-method type pgs, including user-defined type pgs"

tmp_result=$STF_PASS

# Test svc-pgs only
# Run through all other types of pgs, including user-defined types
for pgtype in dependency framework application userdefined; do
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
[[ $result == $STF_PASS ]] && result=$tmp_result
echo "--INFO: [${STF_RESULT_NAMES[$tmp_result]}]"

# ---------------------------------------------------------------------------- #
#
echo "--RSLT: ${STF_RESULT_NAMES[$result]}"
return $result

#
### Script ends here
#
