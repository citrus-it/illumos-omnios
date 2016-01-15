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

CTX_GENT=$(getent group $ctx_group)
if [ -z "$CTX_GENT" ]; then
	echo "Adding group $ctx_group"
	groupadd $ctx_group
	if [ $? -ne 0 ]; then
		print -- "--DIAG: could not add group $ctx_group"
		exit 1
	fi
else
	echo "Group $ctx_group already present"
fi

CTX_UENT=$(getent passwd $ctx_user)
if [ -z "$CTX_UENT" ]; then
	echo "Adding user $ctx_user"
	useradd -g $ctx_group -d $(pwd) $ctx_user
	if [ $? -ne 0 ]; then
		print -- "--DIAG: Could not add user $ctx_user"
		groupdel $ctx_group
		exit 1
	fi
else
	echo "User $ctx_user already present"
fi

echo "blanking $ctx_user's password"
passwd -r files -d $ctx_user >/dev/null

CTX_UENT=$(getent passwd $ctx_lockeduser)
if [ -z "$CTX_UENT" ]; then
	echo "Adding locked account $ctx_lockeduser"
	useradd -g $ctx_group -d $(pwd) $ctx_lockeduser
else
	echo "User $ctx_user is already installed"
fi

echo "Locking $ctx_lockeduser"
passwd -r files -l $ctx_lockeduser >/dev/null

# Add the execution profile "Test Context Profile" which has the properties
# of uid=$ctx_user;gid=$ctx_group
grep "$ctx_profilename" /etc/security/exec_attr >/dev/null 2>&1
if [ $? -ne 0 ]; then
	echo "adding: '$ctx_profilename' to exec_attr"
	if [ "`/bin/zonename`" == "global" ]; then
		echo "$ctx_profilename:suser:cmd:::$service_app:uid=$ctx_user;gid=$ctx_group;privs=basic,file_dac_write,file_dac_search;limitprivs=all" >> /etc/security/exec_attr
	else
		echo "$ctx_profilename:suser:cmd:::$service_app:uid=$ctx_user;gid=$ctx_group;privs=basic,file_dac_write,file_dac_search;limitprivs=zone" >> /etc/security/exec_attr
	fi
fi

grep "$ctx_profilename" /etc/security/prof_attr >/dev/null 2>&1
if [ $? -ne 0 ]; then
	echo "adding '$ctx_profilename' to prof_attr"
	echo "$ctx_profilename:::Testing Profile:auths=solaris.*" >> /etc/security/prof_attr
fi

# add the project 'ctxproj' to the projects
proj=$(getent project $ctx_project)
if [ -z "$proj" ]; then
	echo "adding $ctx_project to /etc/projects"
	projadd -U $ctx_user $ctx_project
	if [ $? -ne 0 ]; then
		echo "Failed to create project $ctx_project"
		exit 1
	fi
	# manual fricking modification for project.pool attribute
	# why isn't this command line supported?
	sed "s/^$ctx_project:.*/&project.pool=$ctx_default_resourcepool/" \
		/etc/project > /etc/project.new && \
	cp /etc/project /etc/project.old && \
	cp /etc/project.new /etc/project && \
	rm /etc/project.old
fi

# add in the default project
grep $ctx_user /etc/user_attr >/dev/null 2>&1
if [ $? -ne 0 ]; then
	echo "Adding $ctx_user informarion to /etc/user_attr"
	echo "$ctx_user::::project=$ctx_project" >> /etc/user_attr
	if [ $? -ne 0 ]; then
		echo "Failed to add $ctx_user into /etc/user_attr"
		exit 1
	fi
fi

zone=`/bin/zonename`
if [ "$zone" != "global" ]
then
	exit 0
fi

# resource pool stuff (probably needs work)
pooladm 2>/dev/null >/dev/null
if [ $? -ne 0 ]; then
	echo "Enabling resource pools"
	touch resourcepools
	pooladm -e
fi

echo "Creating test pool $ctx_resourcepool"
poolcfg -c "create pool $ctx_resourcepool" -d
if [ $? -ne 0 ]; then
	echo "Could not create resource pool $ctx_resourcepool"
	[ -f resourcepools ] && {
		rm -f resourcepools
		pooladm -d
	}
fi

echo "Creating test pool $ctx_default_resourcepool"
poolcfg -c "create pool $ctx_default_resourcepool" -d
if [ $? -ne 0 ]; then
	echo "Could not create resource pool $ctx_default_resourcepool"
	[ -f resourcepools ] && {
		rm -f resourcepools
		poolcfg -c "destroy pool $ctx_resourcepool" -d
		pooladm -d
	}
fi

exit 0
