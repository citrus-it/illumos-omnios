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

#
# Setup script for the import-export tests:
# Build the expected output files for the individual test directories.
#
typeset -i setup_result=$STF_PASS

for dir in $TEST_BUILD_SUBDIRS; do
	test_subdir=${STF_SUITE}/${STF_EXEC}/$dir
	services=`egrep -v '^#|^ *$' $test_subdir/services`
	for service in $services
	do
		#
		# Add the test with stf_add_assert
		#
		stf_addassert -u root -t "import-export_${dir}_${service}    " \
		    -c ${STF_SUITE}/${STF_EXEC}/impexp_lib $dir $service
	done
done

exit $setup_result
