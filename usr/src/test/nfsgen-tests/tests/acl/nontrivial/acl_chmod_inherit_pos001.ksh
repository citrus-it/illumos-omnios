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
# Copyright 2009 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#

. $STF_SUITE/tests/acl/acl_common.kshlib

#################################################################################
#
# __stc_assertion_start
#
# ID: acl_chmod_inherit_pos001
#
# DESCRIPTION:
#	Verify chmod have correct behaviour to directory and file when setting
#	different inherit strategy to them.
#	
# STRATEGY:
#	1. Loop super user and non-super user to run the test case.
#	2. Create basedir and a set of subdirectores and files within it.
#	3. Separately chmod basedir with different inherite options.
#	4. Then create nested directories and files like the following.
#	
#                                                   _ odir4
#                                                  |_ ofile4
#                                         _ odir3 _|
#                                        |_ ofile3
#                               _ odir1 _|
#                              |_ ofile2
#                     basefile |
#          chmod -->  basedir -| 
#                              |_ nfile1
#                              |_ ndir1 _ 
#                                        |_ nfile2
#                                        |_ ndir2 _
#                                                  |_ nfile3
#                                                  |_ ndir3
#
#	5. Verify each directories and files have the correct access control
#	   capability.
#	
# TESTABILITY: explicit
#
# TEST_AUTOMATION_LEVEL: automated
#
# __stc_assertion_end
#
################################################################################

[[ :$NFSGEN_DEBUG: = *:${NAME}:* || :${NFSGEN_DEBUG}: = *:all:* ]] \
	&& set -x

function case_cleanup
{
	[[ :$NFSGEN_DEBUG: = *:${NAME}:* || :${NFSGEN_DEBUG}: = *:all:* ]] \
		&& set -x

	[[ $1 != $STF_PASS ]] && [[ -f $STF_TMPDIR/$NAME.$$ ]] \
		&& cat $STF_TMPDIR/$NAME.$$
	rm -rf $STF_TMPDIR/$NAME.$$.*

	if [[ -f $basefile ]]; then
		RUN_CHECK $RM -f $basefile 
	fi
	if [[ -d $basedir ]]; then
		RUN_CHECK $RM -rf $basedir
	fi

	# restore the mount option and enable attribute cache
	RUN_CHECK cd /
	RUN_CHECK do_remount

	[[ -n $1 ]] && exit $1 || return 0
}

echo "ASSERTION: Verify chmod have correct behaviour to directory and file when " \
	"setting different inherit strategies to them."

# This case needs to disable the attribute cache.
RUN_CHECK cd / || case_cleanup $STF_FAIL
RUN_CHECK do_remount noac || case_cleanup $STF_FAIL
RUN_CHECK cd $MNTDIR || case_cleanup $STF_FAIL

# Define inherit flag
set -A object_flag file_inherit dir_inherit file_inherit/dir_inherit
set -A strategy_flag "" inherit_only no_propagate inherit_only/no_propagate

# Defile the based directory and file
basedir=$TESTDIR/basedir;  basefile=$TESTDIR/basefile

# Define the existed files and directories before chmod
odir1=$basedir/odir1; odir2=$odir1/odir2; odir3=$odir2/odir3
ofile1=$basedir/ofile1; ofile2=$odir1/ofile2; ofile3=$odir2/ofile3

# Define the files and directories will be created after chmod
ndir1=$basedir/ndir1; ndir2=$ndir1/ndir2; ndir3=$ndir2/ndir3
nfile1=$basedir/nfile1; nfile2=$ndir1/nfile2; nfile3=$ndir2/nfile3

# Verify all the node have expected correct access control
allnodes="$basedir $ndir1 $ndir2 $ndir3 $nfile1 $nfile2 $nfile3"
allnodes="$allnodes $odir1 $odir2 $odir3 $ofile1 $ofile2 $ofile3"


#
# According to inherited flag, verify subdirectories and files within it has
# correct inherited access control.
#
function verify_inherit #<object> [strategy]
{
	[[ :$NFSGEN_DEBUG: = *:${NAME}:* || :${NFSGEN_DEBUG}: = *:all:* ]] \
		&& set -x

	# Define the nodes which will be affected by inherit.
	typeset inherit_nodes
	typeset obj=$1
	typeset str=$2

	RUN_CHECK usr_exec $MKDIR -p $ndir3 || case_cleanup $STF_FAIL
	RUN_CHECK usr_exec $TOUCH $nfile1 $nfile2 $nfile3 \
		|| case_cleanup $STF_FAIL

	# Except for inherit_only, the basedir was affected always.
	if [[ $str != *"inherit_only"* ]]; then
		inherit_nodes="$inherit_nodes $basedir"
	fi
	# Get the files which inherited ACE.
	if [[ $obj == *"file_inherit"* ]]; then
		inherit_nodes="$inherit_nodes $nfile1"

		if [[ $str != *"no_propagate"* ]]; then
			inherit_nodes="$inherit_nodes $nfile2 $nfile3"
		fi
	fi
	# Get the directores which inherited ACE.
	if [[ $obj == *"dir_inherit"* ]]; then
		inherit_nodes="$inherit_nodes $ndir1"

		if [[ $str != *"no_propagate"* ]]; then
			inherit_nodes="$inherit_nodes $ndir2 $ndir3"
		fi
	fi
	
	for node in $allnodes; do
		if [[ " $inherit_nodes " == *" $node "* ]]; then
			RUN_CHECKNEG chgusr_exec $ACL_OTHER1 $LS -vd $node \
				> $STF_TMPDIR/$NAME.$$ 2>&1 \
				|| case_cleanup $STF_FAIL
		else
			RUN_CHECK chgusr_exec $ACL_OTHER1 $LS -vd $node \
				> $STF_TMPDIR/$NAME.$$ 2>&1 \
				|| case_cleanup $STF_FAIL
		fi
	done
}

for user in root $ACL_STAFF1; do
	RUN_CHECK set_cur_usr $user || case_cleanup $STF_FAIL

	for obj in "${object_flag[@]}"; do
		for str in "${strategy_flag[@]}"; do
			typeset inh_opt=$obj
			(( ${#str} != 0 )) && inh_opt=$inh_opt/$str
			aclspec="A+user:$ACL_OTHER1:read_acl:$inh_opt:deny"

			RUN_CHECK usr_exec $MKDIR $basedir \
				|| case_cleanup $STF_FAIL
			RUN_CHECK usr_exec $TOUCH $basefile \
				|| case_cleanup $STF_FAIL
			RUN_CHECK usr_exec $MKDIR -p $odir3 \
				|| case_cleanup $STF_FAIL
			RUN_CHECK usr_exec $TOUCH $ofile1 $ofile2 $ofile3 \
				|| case_cleanup $STF_FAIL

			#
			# Inherit flag can only be placed on a directory,
			# otherwise it will fail.
			#
			RUN_CHECK usr_exec $CHMOD $aclspec $basefile \
				|| case_cleanup $STF_FAIL

			#
			# Place on a directory should succeed.
			#
			RUN_CHECK usr_exec $CHMOD $aclspec $basedir \
				|| case_cleanup $STF_FAIL
			
			verify_inherit $obj $str
			
			RUN_CHECK usr_exec $RM -rf $basefile $basedir \
				|| case_cleanup $STF_FAIL
		done
	done
done

# Verify chmod inherit behaviour passed.
case_cleanup $STF_PASS
