#!/usr/bin/ksh -p
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
# ASSERTION: prop_pattern_value_004
#
# DESCRIPTION:
#	Verify the property group common name pattern when the explicitly setting
#	all the attributes with multiple values.
#
# STRATEGY:
#	Import the default service and verify the pattern is correct.
#
# end __stf_assertion__
###############################################################################

# Load the helper functions
. ${STF_SUITE}/${STF_EXEC}/../include/templates.kshlib

# Initialize test result
typeset -i RESULT=$STF_PASS

trap cleanup 0 1 2 15

# make sure that the environment is sane - svc.configd is up and running
check_gl_env
if [ $? -ne 0 ] 
then
	echo "--DIAG:
	Invalid test environment - svc.configd not available"

	RESULT=$STF_UNRESOLVED
	exit $RESULT
fi

extract_assertion_info $ME

assertion=prop_pattern_value_004

typeset -i test_id=1

readonly registration_template=${MANIFEST_VALUES}
readonly registration_file=/tmp/prop_pattern_value_004.xml
readonly test_service=${SERVICE}

service_exists $test_service
if [ $? -eq 0 ]
then
	service_delete $test_service

	service_exists $test_service
	if [ $? -eq 0 ]
	then
		echo "-- DIAG: [${assertion}]" \
		"	Could not remove service"

		RESULT=$STF_UNRESOLVED
		cleanup
	fi
fi

PG_NAME="foo"
PROP_NAME="bar"
CNT=1
VALUE_NAME=`$NAME_GEN 8`
NAMES_LIST="$VALUE_NAME"
NAME_TEXT="<value name='$VALUE_NAME' >\\
		<common_name>\\
			<loctext xml:lang='C'>\\
				Test Value $VALUE_NAME\\
			</loctext>\\
		</common_name>\\
	</value>"

VALUES_DATA="\$NAME_TEXT"
ADDCNT=7
TCNT=1
while [ $CNT -le 16 ]
do
	NCNT=1
	while [ $NCNT -le $ADDCNT ]
	do
		VALUE_NAME=`$NAME_GEN 8`
		echo $NAMES_LIST | grep $VALUE_NAME > /dev/null 2>&1
		while [ $? -eq 0 ]
		do
			VALUE_NAME=`$NAME_GEN 8`
			echo $NAMES_LIST | grep $VALUE_NAME > /dev/null 2>&1
		done

		NAMES_LIST="$NAMES_LIST $VALUE_NAME"
		NAME_TEXT="$NAME_TEXT\\
			<value name='$VALUE_NAME' >\\
				<common_name>\\
					<loctext xml:lang='C'>\\
						Test Value $VALUE_NAME\\
					</loctext>\\
				</common_name>\\
			</value>"

		VALUES_DATA="\$NAME_TEXT"
		NCNT=`expr $NCNT + 1`
	done

	if [ $ADDCNT -eq 7 ]
	then
		ADDCNT=8
	fi

	manifest_generate $registration_template \
		VALUES="$VALUES_DATA" > $registration_file

	manifest_purgemd5 $registration_file

	RCNT=`grep "<value name=" $registration_file | wc -l | sed -e 's/ //g'`
	CURTIME=`date "+%H:%M:%S"`
	echo "--INFO: Validate for $RCNT values : $CURTIME"

	verify_import pos $registration_file $test_service $OUTFILE $ERRFILE

	CURTIME=`date "+%H:%M:%S"`
	echo "--INFO: Service imported : $CURTIME"

	pgn=${PROP_PREFIX_NT}${PG_NAME}_${PROP_NAME}
	for VNAME in $NAMES_LIST
	do
		echo "--INFO: Validate the property group pattern value $VNAME"
		encoded_vn=`$BASE32_CODE -e $VNAME`
		verify_prop QUIET $test_service \
		    $pgn/value_${encoded_vn}_common_name_C ustring \
		    "Test Value $VNAME"
	done

	service_cleanup $test_service

	CNT=`expr $CNT + 1`
done

trap 0 1 2 15

exit $RESULT
