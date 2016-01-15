#! /usr/bin/ksh
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

# First STF library
. ${STF_TOOLS}/include/stf.kshlib

#
# run svccfg validate against the service or manifest and check for
# failure or any stderr output that would indicate a problem.
# If there are no problems report pass, otherwise journal the problem
# with the stderr and stdout.
#
function validate {
	siorm=$1
	item=$2

	echo "--INFO : Validating the $siorm $item"
	svccfg validate $item > $outfile 2> $errfile
	if [ $? -ne 0 -o `wc -l $errfile | awk '{print $1}'` -gt 0 ]; then
		result=$STF_FAIL

		echo "--DIAG : The validation failed with the following output"
		echo "--DIAG : -------- stderr "
		cat $errfile
		echo "--DIAG : -------- stdout "
		cat $outfile
	else
		echo "--INFO : $item passed validation"
	fi
}

#
# Dump the test information into the journal along with a list
# of the items to be tested.
#
echo "--INFO: Validating the following items with svccfg validate"
echo "--INFO: Any validation failures should be considered a bug"
echo "--INFO: against the manifest or service in question."
for i in $@
do
	echo "     $i"
done
echo "------------------------------------------------"


#
# Test the validity of each of the command line arguments, whether
# it's a manifest or service.
#
result=$STF_PASS
for i in $@
do
	echo $i | grep "^/" > /dev/null 2>&1
	if [ $? -eq 0 ]; then
		validate manifest $i
	else
		validate service $i
	fi
done

exit $result
