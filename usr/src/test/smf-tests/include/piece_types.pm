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

$SERVICEBUNDLE =	1;
$SERVICE =		10;

#
# These types are associated with a service
# while some can be associated with an instance
# as repeats.  But because of the numbering scheme
# it will allow me to just reuse the types.
#
$CREATEDEFAULTINSTANCE =	100;
$SINGLEINSTANCE =		101;
$RESTARTER =			102;
$DEPENDENCY =			103;
$DEPENDENT =			104;
$METHODCONTEXT =		105;
$EXECMETHOD =			106;
$PROPERTYGROUP =		107;
$INSTANCE =			108;
$STABILITY =			109;
$TEMPLATE =			110;

#
# These types are associate with the Template type.
#
$COMMONNAME =		201;
$DESCRIPTION =		202;
$DOCUMENTATION =	203;
$PGPATTERN =		204;

#
# types associated with a pg_pattern, the commonname
# and description types can be associated at this level
# but will be picked up from the above types.  Again
# with the numbering scheme they will start at the top
# of the associations list and will be in the correct
# order.
#
$PROPPATTERN =	300;

#
# types associated with a prop_pattern type.
#
$UNITS =		400;
$VISIBILITY =		401;
$CARDINALITY =		402;
$INTERNALSEPARATORS =	402;
$VALUES =		403;
$CONSTRAINTS =		404;
$CHOICES =		405;


#
# types associated with choices
#
$VALUE =	420;
$RANGE =	421;
$VALUESET =	422;
$ALLVALUES =	423;

#
# types associated with documentation
#
$DOCLINK =	440;
$MANPAGE =	441;

#
# types associated with a restarter
#
$SERVICEFMRI =	450;

#
# types assocaited with an method_context
#
$METHODPROFILE =	460;
$METHODCREDENTIAL =	461;
$METHODENVIRONMENT =	462;

#
# types associated with property_group
#
$PROPVAL =	500;
$PROPERTY =	501;

#
# types associated with a property
#
$COUNTLIST =		550;
$INTEGERLIST =		551;
$OPAQUELIST =		552;
$HOSTLIST =		553;
$HOSTNAMELIST =		554;
$NETADDRESSV4LIST =	555;
$NETADDRESSV6LIST =	556;
$TIMELIST =		557;
$ASTRINGLIST =		558;
$USTRINGLIST =		559;
$BOOLEANLIST =		560;
$FMRILIST =		561;
$URILIST =		562;

$VALUENODE =		600;
