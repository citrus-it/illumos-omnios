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

###############################################################################
# start __stf_assertion__
#
# ASSERTION: svccfg_inventory_001
#
# DESCRIPTION:
#	Calling the 'inventory file' subcommand, where file is a
#	service bundle, then the FMRIs of the services and instances
#	it describes are printed to stdout.  For each service, the
#	FMRIs of its insances are displayed before the FMRI of the
#	service.
#
# STRATEGY:
#	Inventory a .xml file that has multiple services and instances.
#	The output is compared against an expected output file.  This
#	is a good verification strategy because the output of the
#	command should be stable.
#	
#
# end __stf_assertion__
###############################################################################


# First STF library
. ${STF_TOOLS}/include/stf.kshlib

# Load GL library
. ${STF_SUITE}/include/gltest.kshlib

readonly ME=$(whence -p ${0})
readonly MYLOC=$(dirname ${ME})

# Initialize test result 
typeset -i RESULT=$STF_PASS


function cleanup {
	
	rm -f $OUTFILE $ERRFILE 

	exit $RESULT
}

trap cleanup 0 1 2 15

# make sure that the environment is sane - svc.configd  is up and running
check_gl_env
[[ $? -ne 0 ]] && {
	echo "--DIAG: 
	     	Invalid test environment - svc.configd  not available"

        RESULT=$STF_UNRESOLVED
	exit $RESULT
}

assertion=svccfg_inventory_001

# extract and print assertion information from this source script.
extract_assertion_info $ME

# Before starting make sure that the test service doesn't already exist.
# If it does then consider it a fatal error.

typeset service_prefix=svccfg_inventory


svccfg list ${service_prefix}* > $OUTFILE 2>$ERRFILE
[[ -s ${OUTFILE}  ]] && {
	echo "--DIAG: [${assertion}]
	services with prefix "$service_prefix" should not exist in
	repository but does.  They are:
	$(cat $OUTFILE))"

	RESULT=$(update_result $STF_UNRESOLVED $RESULT)
	exit $RESULT
}

#  
#Start assertion testing
#

readonly registration_file=$MYLOC/inventory_001.xml

svccfg  inventory $registration_file > $OUTFILE 2>$ERRFILE
ret=$?

[[ $ret -ne 0 ]] &&  {
	echo "--DIAG: [${assertion}]
	svccfg inventory expected to return 0, got $ret"

	RESULT=$STF_FAIL
}

[[ ! -f ${MYLOC}/inventory_001.out ]] &&  {
	echo "--DIAG: [${assertion}]
	file ${MYLOC}/inventory_001.out not available - needed for the test"

	RESULT=$(update_result $STF_UNRESOLVED $RESULT)
	exit $RESULT
}


# Verify that nothing in stdout - this is a non-fatal error
cmp -s $OUTFILE ${MYLOC}/inventory_001.out 
ret=$?
[[ $ret -ne 0 ]] &&  {
	echo "--DIAG: [${assertion}]
	expected output of svccfg inventory command is:
	$(cat ${MYLOC}/inventory_001.out)
	the actual output is:
	$(cat ${OUTFILE})"

	RESULT=$STF_FAIL
}

# Verify that nothing in stderr - this is a non-fatal error
[[ -s $ERRFILE ]] &&  {
	echo "--DIAG: [${assertion}]
	stderr not expected, but got $(cat $ERRFILE)"

	RESULT=$STF_FAIL
}

# Make sure that the service doesn't exist in the repository - not likely 
# to happen but it's an easy check.
#
svccfg list ${service_prefix}* > $OUTFILE 2>$ERRFILE
[[ -s ${OUTFILE}  ]] && {
	echo "--DIAG: [${assertion}]
	services with prefix "$service_prefix" should not exist in
	repository but does"

	RESULT=$(update_result $STF_UNRESOLVED $RESULT)
	exit $RESULT
}


exit $RESULT
