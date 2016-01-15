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

#
# start __stf_assertion__
#
# ASSERTION: depends_053a
# DESCRIPTION:
#  A service with multiple dependencies in a "require_all" grouping
#  property. All of the dependencies are satisfied.
#  svc.startd will transition the service into the online state.
#  Services: a, b, c; c depends on a and b
#  All services are instances of different services. Dependencies are service
#  and instance specified.
#
# end __stf_assertion__
#

. ${STF_SUITE}/include/gltest.kshlib

export RUNDIR=$(/bin/pwd)
export DATA=$(dirname $0)

export assertion=${saved_assertion}b
export registration_template=$DATA/service_053a.xml
export registration_file=$RUNDIR/service_053a.xml

extract_assertion_info $0

exec ${0%[a-z]}_main
