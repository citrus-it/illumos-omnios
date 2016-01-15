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
# ASSERTION: svccfg_setprop_012
#
# DESCRIPTION:
#	The 'setprop pg/name = [type:] *' subcommand accepts
#	string values as arguments.  If these values which 
#	contain double-quotes or backslashes they must be 
#	enclosed by double-quotes and the contained 
#	double-quotes and backslashes must be quoted by 
#	backslashes.
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
	
	# Note that $TEST_SERVICE may or may not exist so don't check
	# results.  Just make sure the service is gone.
	service_delete $TEST_SERVICE 

	service_exists ${TEST_SERVICE}
	[[ $? -eq 0 ]] && {
		echo "--DIAG: [${assertion}, cleanup]
		service ${TEST_SERVICE} should not exist in 
		repository after being deleted, but does"

        	RESULT=$(update_result $STF_UNRESOLVED $RESULT)
	}

	rm -f $OUTFILE $ERRFILE $CMDFILE

	exit $RESULT
}

function verify_outfile {

	num_lines=$(wc -l ${OUTFILE} | awk '{print $1}')
	[[ $num_lines -ne $total ]] && {
		echo "--DIAG: [${assertion}]
		expected $total lines in $OUTFILE, got $num_lines"
	
		RESULT=$STF_FAIL
	}
	
	count=0
	while [ $count -lt $total ]
	do
		line=$(egrep "^foo/good_$count " ${OUTFILE})
		[[ $? -ne 0 ]] && {
			echo "--DIAG: [${assertion}]
	line ${data_out_array[$count]} not in $OUTFILE, should be"

			TEST_RESULT=$STF_FAIL

			# if this fails move to the next check
			continue
		}

		set $line; shift
		data_type=$1; shift;
		value=$*

		[[ "astring" != "$data_type" ]] && {
			echo "--DIAG: [${assertion}]
		for entry $count was expecting type \"astring\", got \"$data_type\""
			
			TEST_RESULT=$STF_FAIL
		}

		[[ ${data_out_array[$count]} != "$value" ]] && {
			echo "--DIAG: [${assertion}]
	for entry $count was expecting value \"${data_out_array[$count]}\", got \"$value\""
		
			TEST_RESULT=$STF_FAIL
		}

		(( count = count + 1 ))
	done

}


trap cleanup 0 1 2 15

# make sure that the environment is sane - svc.configd is up and running
check_gl_env
[[ $? -ne 0 ]] && {
	echo "--DIAG: 
	Invalid test environment - svc.configd is not available"

        RESULT=$STF_UNRESOLVED 
	exit $RESULT
}

# extract and print assertion information from this source script.
extract_assertion_info $ME


# Before starting make sure that the test service doesn't already exist.
# If it does then consider it a fatal error.
service_exists $TEST_SERVICE
[[ $? -eq 0 ]] && {
	echo "--DIAG: [${assertion}, test 1]
	service $TEST_SERVICE should not exist in 
	repository but does"

        RESULT=$(update_result $STF_UNRESOLVED $RESULT)
	exit $RESULT
}


cat << EOF >$CMDFILE
add $TEST_SERVICE
select $TEST_SERVICE
addpg foo framework
EOF

svccfg -f $CMDFILE > $OUTFILE 2>$ERRFILE
ret=$?
[[ $ret -ne 0 ]] && {
	echo "--DIAG: [${assertion}]
	error adding service/instance ${TEST_SERVICE}:${TEST_INSTANCE} needed for test
	error output is $(cat $ERRFILE)"

        RESULT=$(update_result $STF_UNRESOLVED $RESULT)
	exit $RESULT
}

typeset -i TEST_RESULT=$STF_PASS

assertion=svccfg_setprop_012

# data_in_array is the data that will be passed to setprop
set -A data_in_array 	'"what\\ever"' \
			'"what\"ever"' \
			'"what\\\\\\\\ever"' \
			'"what\"\"ever"' \
			'"what" "\\" "ever"' \
			'"what " " ever"' 

			 
# type_out_array are the values that will be displayed after setprop
set -A data_out_array 	'what\ever' \
		        '"what\"ever"' \
		        'what\\\\ever' \
		        '"what\"\"ever"' \
		        '"what" "\\" "ever"' \
		        '"what " " ever"'

typeset -i total=${#data_in_array[*]}
typeset -i index=0

echo "select $TEST_SERVICE" > $CMDFILE

while [ $index -lt $total ]
do
	cat << EOF >>$CMDFILE
setprop foo/good_$index=astring: (${data_in_array[$index]})
EOF
	(( index = index + 1 ))
done

echo "listprop foo/good_*" >> $CMDFILE


svccfg -f $CMDFILE > $OUTFILE 2>$ERRFILE
ret=$?
[[ $ret -ne 0 ]] && {
	echo "--DIAG: [${assertion}]
	svccfg expected to return 0, got $ret"

	RESULT=$STF_FAIL
}

# Verify that nothing in stderr - this is a fatal error
[[ -s $ERRFILE ]] &&  {
	echo "--DIAG: [${assertion}]
	stderr not expected, but got $(cat $ERRFILE)"

	RESULT=$STF_FAIL
}

[[ $RESULT -ne $STF_PASS ]] && exit $RESULT

verify_outfile

print_result $TEST_RESULT
RESULT=$(update_result $TEST_RESULT $RESULT)

rm -f $OUTFILE $ERRFILE $CMDFILE

exit $RESULT
