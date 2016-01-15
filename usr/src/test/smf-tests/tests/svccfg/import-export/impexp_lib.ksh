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

. ${STF_TOOLS}/include/stf.kshlib

typeset test_subdir=${STF_SUITE}/${STF_EXEC}/${1}
typeset service=${2}

typeset -i test_result=0

necho() {
	/usr/ucb/echo -n "$@ "
}

props() {
	$SVCCFG -v <<EOF
select $service
listprop
EOF
}

delete() {
	$SVCCFG -v delete -f $service  2>&1
	if [[ $? -ne 0 ]]; then
		echo "Couldn't delete $service, do a make clean" >&2
		exit 1
	fi
}

check() {
	props $service >$newpropout
	[[ $? -ne 0 ]] && exit $STF_FAIL

	diff -u $propout $newpropout >/dev/null 2>&1
	if [[ $? -ne 0 ]]; then
		echo "$SVCPROP of $service ($newpropout) differs from expected ($propout)" 
		diff -u $propout $newpropout 
		exit $STF_FAIL
	fi

	rm $newpropout
	return 0
}

$SVCCFG -v import ${STF_SUITE}/${STF_EXEC}/standard.xml >/dev/null 2>&1

echo "$test_subdir/$service ["
xmlin="$test_subdir/$service.in.xml"
xmlout="$TMP_DIR/$service.out.xml"
newxmlout="$TMP_DIR/$service.out.xml"
propout="$TMP_DIR/$service.out.prop"
newpropout="$TMP_DIR/$service.out.prop"
SUBDIR=${test_subdir##*/}

$SVCCFG -v delete -f $service >/dev/null 2>&1

echo import
$SVCCFG -v import $xmlin  2>&1
ret=$?
if [[ $ret -ne 0 ]]; then
	if [[ "$SUBDIR" = "invalid" ]]; then
		if [[ $ret -eq 139 ]]; then
			exit $STF_FAIL
		else
			exit $STF_PASS
		fi
	else
		exit $STF_FAIL
	fi
else
	[[ "$SUBDIR" = "invalid" ]] && exit $STF_FAIL
fi

echo export
$SVCCFG -v export $service >$newxmlout
[[ $? -ne 0 ]] && exit $STF_FAIL

if [[ -n "$BUILD_EXP_OUTPUTS" ]]; then
	mv $newxmlout $xmlout
	exit $RESULT
fi

echo check
check
echo "check - done"
	
diff -u $xmlout $newxmlout >/dev/null 2>&1
if [[ $? -ne 0 ]]; then
	echo "XML export of $service ($newxmlout) differs from expected ($xmlout)" 
	diff -u $xmlout $newxmlout 
	exit $STF_FAIL
fi

echo delete
delete

echo re-import
$SVCCFG -v import $newxmlout  2>&1
[[ $? -ne 0 ]] && exit $STF_FAIL || rm $newxmlout

echo re-check
check

echo delete
delete
exit $RESULT
