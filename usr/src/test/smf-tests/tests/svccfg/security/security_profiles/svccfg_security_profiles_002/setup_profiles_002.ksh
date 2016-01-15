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

readonly prog=${0##*/}

# Calculate the location of the current test's ksh library
bname=${STF_EXEC##*/}; exec_parent=${STF_EXEC%%/$bname}
parent=${exec_parent##*/}

INC_PARENT=${exec_parent%%/$parent}
INC_FILE=${INC_PARENT##*/}

# Source STF library, GL test library, current directory's test library
. ${STF_TOOLS}/include/stf.kshlib
. ${STF_SUITE}/include/gltest.kshlib
. ${STF_SUITE}/${INC_PARENT}/include/${INC_FILE}.kshlib

# Determine the name of the STF post-setup environment file
readonly f_postsetup=${1}

# Initialize variables
typeset -i result=$STF_PASS
typeset -i rc=0

# Write the names of the test services/instances/pgs/properties
# into the post setup file
PWD=${PWD:=`pwd`}
suffix=${PWD##*/}

cat > ${f_postsetup} <<EOF
export GL_TEST_SERVICE="SVC_gltest_$suffix"
export GL_TEST_INSTANCE="INST_gltest_$suffix"
export GL_TEST_SVCPG="SVCPG_gltest_$suffix"
export GL_TEST_SVCPG_TYPE="userdefined"
export GL_TEST_INSTPG="INSTPG_gltest_$suffix"
export GL_TEST_INSTPG_TYPE="userdefined"
export GL_TEST_USERNAME="${LOGNAME}"
export tested_authorization="'Service Management' profile"
EOF

# Source the post-setup file to give ourselves the same env as the test.
. ${f_postsetup}

#
# Set up the auths in /etc/user_attr
#
[[ $result == $STF_PASS ]] && \
	user_attr_mod add "profiles" "Service Management"

return ${result}

#
### Script ends here
#
