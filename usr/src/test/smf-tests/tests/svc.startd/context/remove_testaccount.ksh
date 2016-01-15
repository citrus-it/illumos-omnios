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

zone=`/bin/zonename`
if [ "$zone" == "global" ]
then
	echo "Resource Management:"
	echo "\tDeleting resource pool $ctx_resourcepool"
	poolcfg -c "destroy pool $ctx_resourcepool" -d

	echo "\tDeleting resource pool $ctx_default_resourcepool"
	poolcfg -c "destroy pool $ctx_default_resourcepool" -d
	[ -f resourcepools ] && {
		rm -f resourcepools
		echo "\tDisabling resource pools"
		pooladm -d
	}
fi


echo "User Attributes:"
echo "\tRemoving $ctx_user from /etc/user_attr"
grep -v "^$ctx_user:" /etc/user_attr > /etc/user_attr.new
cp /etc/user_attr /etc/user_attr.old && \
	cp /etc/user_attr.new /etc/user_attr && \
	rm /etc/user_attr.old

echo "Project:"
echo "\tDeleting project $ctx_project"
projdel $ctx_project
if [ $? -ne 0 ]; then
	echo "--DIAG: Could not delete project $ctx_project"
	exit 1
fi

echo "User and Group:"
echo "\tDeleting user $ctx_user"
userdel $ctx_user
if [ $? -ne 0 ]; then
	echo "--DIAG: Could not remove user $ctx_user"
	exit 1
fi

echo "\tDeleting user $ctx_lockeduser"
userdel $ctx_lockeduser
if [ $? -ne 0 ]; then
	echo "--DIAG: could not remove user $ctx_lockeduser"
	exit 1
fi

echo "\tDeleting group $ctx_group"
groupdel $ctx_group
if [ $? -ne 0 ]; then
	echo "--DIAG: could not remove group $ctx_group"
	exit 1
fi

echo "Security attributes:"
echo "\tRemoving '$ctx_profilename' from exec_attr"
cp /etc/security/exec_attr /etc/security/exec_attr.old
grep -v "^$ctx_profilename:" /etc/security/exec_attr \
	> /etc/security/exec_attr.new
cp /etc/security/exec_attr.new /etc/security/exec_attr
if [ $? -ne 0 ]; then
	echo "--DIAG: Could not remove profile from exec_attr"
	exit 1
fi

echo "\tRemoving '$ctx_profilename' from prof_attr"
cp /etc/security/prof_attr /etc/security/prof_attr.old
grep -v "^$ctx_profilename:" /etc/security/prof_attr \
	> /etc/security/prof_attr.new
cp /etc/security/prof_attr.new /etc/security/prof_attr
if [ $? -ne 0 ]; then
	echo "--DIAG: Could not remove profile from prof_attr"
	exit 1
fi

exit 0
