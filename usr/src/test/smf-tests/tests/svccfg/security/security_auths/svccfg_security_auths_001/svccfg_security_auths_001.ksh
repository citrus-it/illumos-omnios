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
# ASSERTION: svccfg_security_auths_001
#
# DESCRIPTION:
#	A user not possessing the 'solaris.smf.modify' authorization
#	cannot create, delete or modify any services, service instances
#	property groups or properties.
#
# end __stf_assertion__
################################################################################

readonly prog=${0##*/}

# Source STF library, GL test library
. ${STF_TOOLS}/include/stf.kshlib
. ${STF_SUITE}/include/gltest.kshlib

readonly result_tbl=${STF_RESULT_NAMES}

# Assertion ID
readonly assertion=svccfg_security_auths_001

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
# PART 1: Services cannot be created without 'solaris.smf.modify'
echo "--INFO: PART 1:"
echo "	Services cannot be created without 'solaris.smf.modify'"

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
# PART 2: Instances cannot be created without 'solaris.smf.modify'
echo "--INFO: PART 2:"
echo "	Instances cannot be created without 'solaris.smf.modify'"

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
# PART 3: PGs cannot be created without 'solaris.smf.modify'
echo "--INFO: PART 3:"
echo "	Propertygroups cannot be created without 'solaris.smf.modify'"

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
# PART 4: Properties cannot be created without 'solaris.smf.modify'
echo "--INFO: PART 4:"
echo "	Properties cannot be created without 'solaris.smf.modify'"
echo "	a.k.a pg's cannot be modified without 'solaris.smf.modify'"

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
# PART 5: Property values cannot be modified without 'solaris.smf.modify'
echo "--INFO: PART 5:"
echo "	Property values cannot be modified without 'solaris.smf.modify'"

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
# PART 6: Properties cannot be deleted without 'solaris.smf.modify'
echo "--INFO: PART 6:"
echo "	Properties cannot be deleted without 'solaris.smf.modify'"

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
# PART 7: PG's cannot be deleted without 'solaris.smf.modify'
echo "--INFO: PART 7:"
echo "	PG's cannot be deleted without 'solaris.smf.modify'"

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
# PART 8: Instances cannot be deleted without 'solaris.smf.modify'
echo "--INFO: PART 8:"
echo "	Instances cannot be deleted without 'solaris.smf.modify'"

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
# PART 9: Services cannot be deleted without 'solaris.smf.modify'
echo "--INFO: PART 9:"
echo "	Services cannot be deleted without 'solaris.smf.modify'"

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
echo "--RSLT: ${result_tbl[$result]}"
return $result

#
### Script ends here
#
