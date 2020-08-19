#!/bin/ksh -p
#
# This file and its contents are supplied under the terms of the
# Common Development and Distribution License ("CDDL"), version 1.0.
# You may only use this file in accordance with the terms of version
# 1.0 of the CDDL.
#
# A full copy of the text of the CDDL should have accompanied this
# source. A copy of the CDDL is also available via the Internet at
# http://www.illumos.org/license/CDDL.

# Copyright 2020 OmniOS Community Edition (OmniOSce) Association.

SRCDIR=$(dirname $0)
SED=${SED:=/usr/bin/sed}

typeset -i err=0
typeset -i pass=0
typeset -i fail=0

function fatal {
	echo "[FATAL] $*" > /dev/stderr
	exit 1
}

function header {
	echo "**"
	echo "**** $*"
	echo "**"
}

function ldiff {
	diff -u "$@" | $SED 1,2d
}

function setup {
	TMPD=`mktemp -d`
	[ -n "$TMPD" -a -d "$TMPD" ] || fatal "Coult not create temp directory"

	printf "%s\n" a b c d e f g h a j > $TMPD/input1 || \
	    fatal "Could not populate input1 file"
	printf "%s\n" m n o p q a z > $TMPD/input2 || \
	    fatal "Could not populate input2 file"
	cp $TMPD/input1{,~} || fatal "Could not create backup input1"
	cp $TMPD/input2{,~} || fatal "Could not create backup input2"

	EXPECT=$SRCDIR/expect
	output=$TMPD/output

	exec 4>&1 5>&2
}

function cleanup {
	exec 1>&4 2>&5
	rm -rf $TMPD
}

function start {
	curtest="$1"
	exec 1>&4 2>&5
	exec >"$output"

	cp $TMPD/input1{~,} || fatal "Failed to restore input1"
	cp $TMPD/input2{~,} || fatal "Failed to restore input2"
}

function end {
	[ ! -f $EXPECT/$curtest ] && \
	    cp $output $EXPECT/$curtest

	if cmp -s $output $EXPECT/$curtest; then
		echo "[PASS] $curtest"
		((pass++))
	else
		echo "[FAIL] $curtest"
		ldiff $EXPECT/$curtest $output
		((fail++))
		err=1
	fi 1>&4 2>&5
	rm -f $output
}


function run_addrtest {
	typeset script="$1"
	typeset expect="$2"
	typeset files="${3:-$TMPD/input1}"

	typeset ef=`mktemp`
	[[ -n "$expect" ]] && printf "%s\n" $expect > $ef

	$SED -n "$script" $files > $output
	if [[ $? -eq 0 ]] && cmp -s $output $ef; then
		echo "[PASS] sed $script $files"
		((pass++))
	else
		echo "[FAIL] sed $script $files"
		ldiff $ef $output
		((fail++))
		err=1
	fi
	rm -f $ef
}

# Address and address range tests
function tests_addr {
	header "Address tests"

	# Simple
	run_addrtest "3p" "c"
	run_addrtest "\$p" "j"
	run_addrtest "7,\$p" "g h a j"
	run_addrtest "/d/p" "d"
	run_addrtest "/a/p" "a a"

	# Ranges
	run_addrtest "5,7p" "e f g"
	run_addrtest "5,4p" "e"
	run_addrtest "/a/,4p" "a b c d a"
	run_addrtest "0,/b/p" ""
	run_addrtest "4,/a/p" "d e f g h a"
	run_addrtest "/d/,/g/p" "d e f g"

	# Relative ranges
	run_addrtest "3,+0p" "c"
	run_addrtest "3,+1p" "c d"
	run_addrtest "5,+3p" "e f g h"
	run_addrtest "6,+3p" "f g h a"
	run_addrtest "7,+3p" "g h a j"
	run_addrtest "8,+3p" "h a j"
	run_addrtest "/a/,+1p" "a b a j"
	run_addrtest "/a/,+8p" "a b c d e f g h a"
	run_addrtest "/a/,+9p" "a b c d e f g h a j"

	# Alternate delimiters
	run_addrtest "\%^c%p" "c"
	run_addrtest "\%^a%p" "a a"
	run_addrtest "\=a=,\=d=p" "a b c d a j"
	run_addrtest "\=a=,\%d%p" "a b c d a j"

	# Negative
	run_addrtest "4,7!p" "a b c h a j"
	run_addrtest "6,+3!p" "a b c d e j"
	run_addrtest "7,+3!p" "a b c d e f"
	run_addrtest "8,+3!p" "a b c d e f g"

	# Branch
	run_addrtest "4,7 { /e/b
			p
		}" "d f g"
	run_addrtest "4,+3 { /e/b
			p
		}" "d f g"

	# stdin
	cat $TMPD/input2 $TMPD/input2 | run_addrtest "\$p" "z" " "

	# Multi-file
	for fileset in \
		"$TMPD/input1 $TMPD/input2" \
		"$TMPD/input1 /dev/null $TMPD/input2" \
		"/dev/null $TMPD/input1 $TMPD/input2" \
		"$TMPD/input1 $TMPD/input2 /dev/null" \
		"$TMPD/input1 /dev/null $TMPD/input2 /dev/null" \
		"/dev/null $TMPD/input1 /dev/null $TMPD/input2" \
		"$TMPD/input1 $TMPD/input2 /dev/null /dev/null" \
		"$TMPD/input1 /dev/null /dev/null $TMPD/input2 /dev/null" \
		"/dev/null $TMPD/input1 /dev/null /dev/null $TMPD/input2" \
	; do
		run_addrtest "\$p" "z" "$fileset"
		run_addrtest "3p" "c" "$fileset"
		run_addrtest "13p" "o" "$fileset"
	done
}

# In-place editing tests
function tests_inplace {
	header "In-place tests"

	for f in i I; do
		start ${f}nplace.simple.1
		$SED -${f} 's/b/x/' $TMPD/input1; cat $TMPD/input1; end

		start ${f}nplace.mfile.1
		$SED -${f} 's/a/x/' $TMPD/input{1,2}; cat $TMPD/input{1,2}; end

		start ${f}nplace.mfile.2
		$SED -${f} '3d' $TMPD/input{1,2}; cat $TMPD/input{1,2}; end

		start ${f}nplace.mfile.3
		$SED -${f} '13d' $TMPD/input{1,2}; cat $TMPD/input{1,2}; end

		start ${f}nplace.backup.1
		$SED -${f}.bak '2d' $TMPD/input2; ldiff $TMPD/input2{.bak,}; end
		rm -f $TMPD/input2.bak

		start ${f}nplace.backup.2
		$SED -${f} .bak '2d' $TMPD/input2; ldiff $TMPD/input2{.bak,}
		end; rm -f $TMPD/input2.bak

		start ${f}nplace.backup.3
		$SED -${f} '' '2,5d' $TMPD/input2; cat $TMPD/input2; end
	done
}

setup
tests_addr
tests_inplace
cleanup

echo
echo "Pass/fail - $pass/$fail"
