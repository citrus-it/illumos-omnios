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


#
# Attempt to import the manifest, collecting any output
# produced.  If there is no second argument there should
# be no error output, and the file should import without
# complaint.  If there is a second argument this is the
# list of errors that should be recognized anything additional
# is considered a failure.
#

# First STF library
. ${STF_TOOLS}/include/stf.kshlib

#
# Error message translations
#
SCF_TERR_MISSING_PG="Required property group missing"
SCF_TERR_WRONG_PG_TYPE="Property group has bad type"
SCF_TERR_MISSING_PROP="Required property missing"
SCF_TERR_WRONG_PROP_TYPE="Property has bad type"
SCF_TERR_CARDINALITY_VIOLATION="Number of property values violates cardinality restriction"
SCF_TERR_VALUE_CONSTRAINT_VIOLATED="Property has illegal value"
SCF_TERR_RANGE_VIOLATION="Property value is out of range"
SCF_TERR_PROP_TYPE_MISMATCH="Properyt type and value type mismatch"
SCF_TERR_VALUE_OUT_OF_RANGE="Value is out of range"
SCF_TERR_INVALID_VALUE="Value is not valid"
ERR_UNIQUE_NAME_TYPE="pg_pattern with name .*. and type .*. is not unique"
ERR_UNIQUE_NAME="pg_pattern with name .*. and empty type is not unique"
ERR_UNIQUE_TYPE="pg_pattern with empty name and type .*. is not unique"

function verify_import {
	ret=$1

	if [ $2 ]
	then
			exec 3<$2

			cnt=0;
			cp $ERRFILE /tmp/scfvtesterrfile.$$
			while read -u3 line
			do
				eval grep "\"\$${line}\"" \$ERRFILE
				if [ $? -eq 0 ]; then
					cnt=`expr $cnt + 1`
					#
					# Now remove that line
					# so that subsequent checks
					# do not find that line
					#
					eval myl="\$${line}"
					LNUM=`grep -n "$myl" $ERRFILE | \
				    	head -1 | awk -F: '{print $1}'`
					eval sed -e '${LNUM}d' $ERRFILE > ${ERRFILE}.tmp
					mv ${ERRFILE}.tmp $ERRFILE
				fi
			done
			trueerrcnt=`wc -l $2 | awk '{print $1}'`
			if [ $cnt -ne $trueerrcnt ]
			then
				echo "--DIAG: import did not produce the \c"
				echo "correct messages cnt = $cnt " \
			    	"trueerrcnt = $trueerrcnt"

				echo "------------- ERROR OUTPUT --------------------"
				cat /tmp/scfvtesterrfile.$$
				echo "-----------------------------------------------"
				RESULT=$STF_FAIL
			fi
			rm -f /tmp/scfvtesterrfile.$$
	else 
		if [ $ret -ne 0 ]
		then
			echo "-- DIAG: " \
			"svccfg import expected to return 0, got $ret"

			echo "$OUTFILE :"
			cat $OUTFILE
			echo "$ERRFILE :"
			cat $ERRFILE

			RESULT=$STF_FAIL
		else
			errcnt=`wc -l $ERRFILE | awk '{print $1}'`
			if [ $errcnt -gt 0 ]
			then
				echo "--DIAG: unexpected error messages \c"
				echo "were generated"

				echo "$ERRFILE :"
				cat $ERRFILE

				RESULT=$STF_FAIL
			fi
		fi
	fi
}

# Initialize test result
typeset -i RESULT=$STF_PASS
typeset fmri=$1
typeset errfile=$2

#
# Dump the manifest with the expected errors if any.
#
echo "--INFO: Import the following manifest :"
echo "----------------------------------------------------------"
cat "$fmri"
echo "----------------------------------------------------------"
if [ $errfile ]; then
	if [ `wc -l $errfile | awk '{print $1}'` -gt 1 ]; then
		e="errors"
	else
		e="error"
	fi
	echo "--INFO: The following $e is expected."
	echo "----------------------------------------------------------"
	exec 3<$errfile
	while read -u3 line
	do
		eval echo "\$${line}"
	done
	echo "----------------------------------------------------------"
fi
	

svcname=`grep "service name" $1 | awk '{print $2}' | awk -F= '{print $2}' | \
    sed -e s/\'//g`
echo "svccfg import $fmri"
svccfg import $fmri >$OUTFILE 2>$ERRFILE
iret=$?

echo "--INFO: Verify the import"
verify_import $iret $errfile

if [ $iret -ne 0 ]
then
	exit $RESULT
fi

svccfg validate ${svcname}:default >$OUTFILE 2>$ERRFILE
vret=$?

echo "--INFO: Verify the validate"
verify_import $vret $errfile

svccfg delete $svcname

exit $RESULT
