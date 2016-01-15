#!/usr/bin/ksh

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
# Copyright 2015 Nexenta Systems, Inc.  All rights reserved.
#

#
# Test wrapper for SMF tests
#
# Define necessary environments and config variables here
#
export STF_SUITE=/opt/smf-tests
export RUNFILE=$STF_SUITE/tests/stf_description
export STF_TOOLS=/opt/SUNWstc-stf
PATH=$PATH:/opt/SUNWstc-stf/bin/$(isainfo -n)	# stf_ tools
PATH=$PATH:/opt/SUNWstc-genutils/bin		# genutils tools
PATH=$PATH:/opt/SUNWstc-checkenv/bin		# checkenv tools
export PATH

usage() {
	echo "Usage: $0 tests"
	echo "Where"
	echo "   tests	tests to run separated by space"
	echo "       	run all if not specified"
	exit 1
}

#
# Must be run by root
#
if [ `id -u` -ne 0 ]; then
	echo Must run by root
	exit 1
fi

TESTS=$1
#
# Some global environments
# Modify test specific config.vars to reset test specific envs
#
ALL="svcadm svccfg svc.startd manifests"

if [ -z "$TESTS" ]; then	# run all
	TESTS=$ALL
fi

#
# Override STF test log setup
#
#STF_LOGDIR=/var/tmp/smf-tests/results
#export STF_LOGDIR

#
# Set RUNFILE to run test
#
for t in `echo $TESTS`; do
	#
	# Is test valid?
	#
	if [ "${ALL#*$t}" = "$ALL" ]; then
		echo $t is not a valid test, skip
		continue
	fi
	#	
	# Reset STF_EXECUTE_SUBDIRS in RUNFILE
	# to run the test
	#
	eval sed -i "s/^STF_EXECUTE_SUBDIRS=.*/STF_EXECUTE_SUBDIRS='$t'/g" $RUNFILE
	#
	# Configure test
	#
	stf_configure

	#
	# Run test
	#
	stf_execute

	#
	# Unconfigure test
	#
	stf_unconfigure

	#
	# Save individual test log
	#
	#mv $STF_LOGDIR/journal* $STF_LOGDIR/$t.$(date +%Y%m%d%H%M)
done
