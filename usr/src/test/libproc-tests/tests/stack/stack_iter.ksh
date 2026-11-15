#!/usr/bin/ksh
#
#
# This file and its contents are supplied under the terms of the
# Common Development and Distribution License ("CDDL"), version 1.0.
# You may only use this file in accordance with the terms of version
# 1.0 of the CDDL.
#
# A full copy of the text of the CDDL should have accompanied this
# source.  A copy of the CDDL is also available via the Internet at
# http://www.illumos.org/license/CDDL.
#

#
# Copyright 2026 Oxide Computer Company
#

#
# This runs the stack_iter test using both a 32-bit and a 64-bit controller.
# The target is always 32-bit.
#

unalias -a

stk_dir=$(dirname $0)
stk_iter32="$stk_dir/stack_iter.32"
stk_iter64="$stk_dir/stack_iter.64"
stk_targ32="$stk_dir/stack_target.32"
stk_exit=0

printf "Running 32-bit controller against 32-bit target\n"
if ! $stk_iter32 $stk_targ32; then
	stk_exit=1
fi

printf "\nRunning 64-bit controller against 32-bit target\n"
if ! $stk_iter64 $stk_targ32; then
	stk_exit=1
fi

if (( stk_exit == 0 )); then
	printf "All variants passed successfully\n"
fi

exit $stk_exit
