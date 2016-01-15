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
# ASSERTION: svccfg_security_auths_006
#
# DESCRIPTION:
#	A user possessing the 'solaris.smf.modify' authorization can
#	change values in, create, delete or modify property groups of
#	any type, including user-defined types
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
readonly assertion=svccfg_security_auths_006

readonly me=$(whence -p ${0})
readonly myloc=$(dirname ${me})

# Initialize variables
typeset -i result=$STF_PASS
typeset -i tmp_result=$result
typeset -i rc=0
typeset retmsg=""
typeset expmsg=""
readonly tested_authorization="'solaris.smf.modify.framework'"

# Begin test
extract_assertion_info ${me}

# ---------------------------------------------------------------------------- #
# Set up the repository data
echo "--INFO: Add various kinds of data to the repository"
setup_repository 
rc=$?

if [[ $rc -ne 0 ]]; then
	result=$STF_FAIL
	echo "--DIAG: ${prog}: Error adding repository data"
	# Forcibly delete everything under the test service and exit
	svccfg delete ${GL_TEST_SERVICE}
	echo "--RSLT: ${STF_RESULT_NAMES[$result]}"
	return $result
fi

# ---------------------------------------------------------------------------- #
# Clean up the repository data
echo "--INFO: Delete various kinds of data from the repository"
cleanup_repository_with_return
rc=$?

if [[ $rc -ne 0 ]]; then
	result=$STF_FAIL
	# Forcibly delete everyting under the test service and exit
	svccfg delete ${GL_TEST_SERVICE}
fi

cleanup_repository

# ---------------------------------------------------------------------------- #
#
echo "--RSLT: ${STF_RESULT_NAMES[$result]}"
return $result

#
### Script ends here
#
