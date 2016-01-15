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
# ASSERTION: svccfg_security_profiles_001
#
# DESCRIPTION:
#	Users not belonging to the 'Service Management' profile cannot
#	create, delete or modify any services, service instances, pgs
#	or properties on the system.
#
# end __stf_assertion__
################################################################################

readonly prog=${0##*/}
# Assertion ID
readonly assertion=svccfg_security_profiles_001

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

# Begin test
extract_assertion_info ${me}

# ---------------------------------------------------------------------------- #
# PART 1: Services cannot be created without the 'Service Management' profile
echo "--INFO: PART 1: Services cannot be created without the
	'Service Management' profile"

retmsg=$(svccfg add ${GL_TEST_SERVICE}_new 2>&1 1>/dev/null)
rc=$?
expmsg="svccfg: Permission denied."

if [[ $rc != 1 ]] || [[ "$retmsg" != "$expmsg" ]]; then
	result=$STF_FAIL
	svccfg delete ${GL_TEST_SERVICE}_new 2>/dev/null
	echo "--DIAG: svccfg security error during service creation
	EXPECTED: rc = 1, error = '$expmsg'
	RETURNED: rc = $rc, error = '$retmsg'"
fi

# ---------------------------------------------------------------------------- #
# PART 2: Instances cannot be created without the 'Service Management' profile
echo "--INFO: PART 2: Instances cannot be created without the
	'Service Management' profile"

retmsg=$(svccfg -f - 2>&1 1>/dev/null <<EOF
select ${GL_TEST_SERVICE}
add ${GL_TEST_INSTANCE}_new
EOF)
rc=$?
expmsg="svccfg (<stdin>, line 2): Permission denied."

if [[ $rc != 1 ]] || [[ "$retmsg" != "$expmsg" ]]; then
	result=$STF_FAIL
	svccfg delete ${GL_TEST_SERVICE}:${GL_TEST_INSTANCE}_new 2>/dev/null
	echo "--DIAG: svccfg security error during instance creation
	EXPECTED: rc = 1, error = '$expmsg'
	RETURNED: rc = $rc, error = '$retmsg'"
fi

# ---------------------------------------------------------------------------- #
# PART 3: PGs cannot be created without the 'Service Management' profile
echo "--INFO: PART 3: PGs cannot be created without the
	'Service Management' profile"

# Test service-pg
retmsg=$(svccfg -f - 2>&1 1>/dev/null <<EOF
select ${GL_TEST_SERVICE}
addpg ${GL_TEST_SVCPG}_new ${GL_TEST_SVCPG_TYPE}
EOF)
rc=$?
expmsg="svccfg (<stdin>, line 2): Permission denied."

if [[ $rc != 1 ]] || [[ "$retmsg" != "$expmsg" ]]; then
	result=$STF_FAIL
	# Bogus data added, will be cleaned up in cleanup script
	echo "--DIAG: svccfg security error during SVC-PG creation
	EXPECTED: rc = 1, error = '$expmsg'
	RETURNED: rc = $rc, error = '$retmsg'"
fi

# Test instance-pg
retmsg=$(svccfg -f - 2>&1 1>/dev/null <<EOF
select svc:/${GL_TEST_SERVICE}:${GL_TEST_INSTANCE}
addpg ${GL_TEST_INSTPG}_new ${GL_TEST_INSTPG_TYPE}
EOF)
rc=$?
expmsg="svccfg (<stdin>, line 2): Permission denied."

if [[ $rc != 1 ]] || [[ "$retmsg" != "$expmsg" ]]; then
	result=$STF_FAIL
	# Bogus data added, will be cleaned up in cleanup script
	echo "--DIAG: svccfg security error during INST-PG creation
	EXPECTED: rc = 1, error = '$expmsg'
	RETURNED: rc = $rc, error = '$retmsg'"
fi

# ---------------------------------------------------------------------------- #
# PART 4: Properties cannot be created without the 'Service Management' profile
echo "--INFO: PART 4: Properties cannot be created without
	the 'Service Management' profile"
echo "	a.k.a pg's cannot be modified without
	the 'Service Management' profile"

# Test service-pg
retmsg=$(svccfg -f - 2>&1 1>/dev/null <<EOF
select svc:/${GL_TEST_SERVICE}
setprop ${GL_TEST_SVCPG}_userdefined/modby = astring: "${GL_TEST_USERNAME}"
EOF)
rc=$?
expmsg="svccfg (<stdin>, line 2): Permission denied."

if [[ $rc != 1 ]] || [[ "$retmsg" != "$expmsg" ]]; then
	result=$STF_FAIL
	# Bogus data added, will be cleaned up in cleanup script
	echo "--DIAG: svccfg security error during svcpg-property creation
	EXPECTED: rc = 1, error = '$expmsg'
	RETURNED: rc = $rc, error = '$retmsg'"
fi

# Test instance-pg
retmsg=$(svccfg -f - 2>&1 1>/dev/null <<EOF
select svc:/${GL_TEST_SERVICE}:${GL_TEST_INSTANCE}
setprop ${GL_TEST_INSTPG}_userdefined/modby = astring: "${GL_TEST_USERNAME}"
EOF)
rc=$?
expmsg="svccfg (<stdin>, line 2): Permission denied."

if [[ $rc != 1 ]] || [[ "$retmsg" != "$expmsg" ]]; then
	result=$STF_FAIL
	# Bogus data added, will be cleaned up in cleanup script
	echo "--DIAG: svccfg security error during instpg-property creation
	EXPECTED: rc = 1, error = '$expmsg'
	RETURNED: rc = $rc, error = '$retmsg'"
fi

# ---------------------------------------------------------------------------- #
# PART 5: Property values cannot be modified without 'Service Management'
echo "--INFO: PART 5: Property values cannot be modified without
	the 'Service Management' profile"

# Test only service-pg
retmsg=$(svccfg -f - 2>&1 1>/dev/null <<EOF
select svc:/${GL_TEST_SERVICE}
setprop ${GL_TEST_SVCPG}_userdefined/createdby = "${GL_TEST_USERNAME}"
EOF)
rc=$?
expmsg="svccfg (<stdin>, line 2): Permission denied."

if [[ $rc != 1 ]] || [[ "$retmsg" != "$expmsg" ]]; then
	result=$STF_FAIL
	# Bogus data added, will be cleaned up in cleanup script
	echo "--DIAG: svccfg security error during svcpg-property modification
	EXPECTED: rc = 1, error = '$expmsg'
	RETURNED: rc = $rc, error = '$retmsg'"
fi

# ---------------------------------------------------------------------------- #
# PART 6: Properties cannot be deleted without the 'Service Management' profile
echo "--INFO: PART 6: Properties cannot be deleted without
	the 'Service Management' profile"

# Test only instance-pg
retmsg=$(svccfg -f - 2>&1 1>/dev/null <<EOF
select svc:/${GL_TEST_SERVICE}:${GL_TEST_INSTANCE}
delprop ${GL_TEST_INSTPG}_userdefined/createdby
EOF)
rc=$?
expmsg="svccfg (<stdin>, line 2): Permission denied."

if [[ $rc != 1 ]] || [[ "$retmsg" != "$expmsg" ]]; then
	result=$STF_FAIL
	# Bogus data added, will be cleaned up in cleanup script
	echo "--DIAG: svccfg security error during instpg-property deletion
	EXPECTED: rc = 1, error = '$expmsg'
	RETURNED: rc = $rc, error = '$retmsg'"
fi

# ---------------------------------------------------------------------------- #
# PART 7: PG's cannot be deleted without the 'Service Management' profile
echo "--INFO: PART 7: PG's cannot be deleted without
	the 'Service Management' profile"

# Test only svc-pgs
retmsg=$(svccfg -f - 2>&1 1>/dev/null <<EOF
select svc:/${GL_TEST_SERVICE}
delpg ${GL_TEST_SVCPG}_userdefined
EOF)
rc=$?
expmsg="svccfg (<stdin>, line 2): Permission denied."

if [[ $rc != 1 ]] || [[ "$retmsg" != "$expmsg" ]]; then
	result=$STF_FAIL
	# Bogus data added, will be cleaned up in cleanup script
	echo "--DIAG: svccfg security error during svc-pg deletion
	EXPECTED: rc = 1, error = '$expmsg'
	RETURNED: rc = $rc, error = '$retmsg'"
fi

# ---------------------------------------------------------------------------- #
# PART 8: Instances cannot be deleted without the 'Service Management' profile
echo "--INFO: PART 8: Service instances cannot be deleted without
	the 'Service Management' profile."

retmsg=$(
svccfg delete svc:/${GL_TEST_SERVICE}:${GL_TEST_INSTANCE} 2>&1 1>/dev/null
)
rc=$?
expmsg="svccfg: Permission denied."

if [[ $rc != 1 ]] || [[ "$retmsg" != "$expmsg" ]]; then
	result=$STF_FAIL
	# Bogus data added, will be cleaned up in cleanup script
	echo "--DIAG: svccfg security error during instance deletion
	EXPECTED: rc = 1, error = '$expmsg'
	RETURNED: rc = $rc, error = '$retmsg'"
fi

# ---------------------------------------------------------------------------- #
# PART 9: Services cannot be deleted without the 'Service Management' profile
echo "--INFO: PART 9: Services cannot be deleted without
	the 'Service Management' profile"

retmsg=$(svccfg delete ${GL_TEST_SERVICE} 2>&1 1>/dev/null)
rc=$?
expmsg="svccfg: Permission denied."

if [[ $rc != 1 ]] || [[ "$retmsg" != "$expmsg" ]]; then
	result=$STF_FAIL
	# Bogus data added, will be cleaned up in cleanup script
	echo "--DIAG: svccfg security error during svc-pg deletion
	EXPECTED: rc = 1, error = '$expmsg'
	RETURNED: rc = $rc, error = '$retmsg'"
fi

# ---------------------------------------------------------------------------- #
#
echo "--RSLT: ${STF_RESULT_NAMES[$result]}"
return $result

#
### Script ends here
#
