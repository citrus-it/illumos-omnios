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

# Test to verify SERVER log the correct data in nfslog given the specified
# tag.  This test is part of the sharemnt/stc2 harness to test various
# combinations of valid nfslog.conf tag entries.
#
# Usage:
#      test_nfslogd <test_case_name> <test_file> <tag_name>
#		<LOG_OK> <BUF_OK> <FHT_OK> <DEBUG_switch>
#
#      - LOG_OK indicates if the logging tag contains a valid log file
#	 This is either 1 for a valid file or 0 for an invalid file.
#	 (This should always be 1)
#      - BUF_OK indicates if the logging tag contains a valid buffer file.
#	 This is either 1 for a valid file or 0 for an invalid file.
#	 (This should always be 1 for nfsv2/3)
#      - FHT_OK indicates if the logging tag contains a valid file handle table.
#	 This is either 1 for a valid file or 0 for an invalid file.
#	 (This should always be 1 for nfsv2/3)
#
# This test verifies that nfslogd successfully logged the expected
# information specified by the specified <tag>.  We use the <tag> to
# search /etc/nfs/nfslog.conf on the $SERVER to find the corresponding
# buffer, logfile, fh2path_table and verify that they were created, or
# not created as expected.  The logfile is then searched to verify
# that the appropriate log record has been created.  This log record of
# of the form "$NFSLOGDDIR/$TESTFILE write" or "$NFSLOGDDIR/$TESTFILE i" for
# the log format.
#
# This test will copy itself to the $STF_TMPDIR directory in the host defined
# by $SERVER.  It will then execute the copy in the $SERVER
#
# Note that the tag in nfslog.conf has to be unique.  We grep for this
# tag in the nfslog.conf file, and rely on the fact that we will only
# find one match.

#
# Returns 0 if the requested pattern ($2) is defined in the input line ($1).
# The value of the pattern is stored in $NEWVALUE.
# Returns non-zero if the pattern is not found.
#
function get_pattern_value {
	[[ -n $DEBUG ]]  && set -x

	NEWVALUE=$(echo $1 | /usr/bin/nawk -v pattern=$2 \
		'{for (i = 1; i <= NF; i++) if ($i ~ pattern) print $i; }')
	# Not defined; we're done
	[[ -z $NEWVALUE ]] && return 1
	NEWVALUE=$(echo $NEWVALUE | /usr/bin/nawk -F= '{print $2}')
	return 0
}

#
# Returns the absolute path of the requested pattern.
# Prepends $THE_DEFAULTDIR to the requested pattern if it is a relative
# path.
# The absolute value is stored in $NEWVALUE
#
function get_absolute_path {
	[[ -n $DEBUG ]]  && set -x

	FIRSTCHAR=$(echo $1 | /usr/bin/cut -c1)
	[[ $FIRSTCHAR == "/" ]] && NEWVALUE=$1 || NEWVALUE=$THE_DEFAULTDIR/$1
	return 0
}

#
# Sets the value of the current entry of the /etc/nfs/nfslog.conf file.
# The values are stored in:
#       defaultdir= $THE_DEFAULTDIR
#       buffer= $THE_BUFFER
#       fhtable= $THE_FHPATH
#       log= $THE_LOGPATH
#       logformat= $THE_LOGFORMAT
#
# The values are all absolute directory values (except for THE_LOGFORMAT).
#
function get_nfslogconf {
	[[ -n $DEBUG ]]  && set -x

	GLOBAL_LINE=$(grep "^global" /etc/nfs/nfslog.conf)

	if [[ -n $GLOBAL_LINE ]]; then
		# Global tag defined, get the new values
		get_pattern_value "$GLOBAL_LINE" "defaultdir="
		(( $? == 0 )) && THE_DEFAULTDIR=$NEWVALUE

		get_pattern_value "$GLOBAL_LINE" "buffer="
		(( $? == 0 )) && THE_BUFFER=$NEWVALUE

		get_pattern_value "$GLOBAL_LINE" "fhtable="
		(( $? == 0 )) && THE_FHPATH=$NEWVALUE

		get_pattern_value "$GLOBAL_LINE" "log="
		(( $? == 0 )) && THE_LOGPATH=$NEWVALUE

		get_pattern_value "$GLOBAL_LINE" "logformat="
		(( $? == 0 )) && THE_LOGFORMAT=$NEWVALUE
	fi

	TAG_LINE=$(grep "^$LOGTAG" /etc/nfs/nfslog.conf)
	if [[ -n $TAG_LINE ]]; then
		# Specified tag defined, get the new values
		get_pattern_value "$TAG_LINE" "defaultdir="
		(( $? == 0 )) && THE_DEFAULTDIR=$NEWVALUE

		get_pattern_value "$TAG_LINE" "buffer="
		(( $? == 0 )) && THE_BUFFER=$NEWVALUE

		get_pattern_value "$TAG_LINE" "fhtable="
		(( $? == 0 )) && THE_FHPATH=$NEWVALUE

		get_pattern_value "$TAG_LINE" "log="
		(( $? == 0 )) && THE_LOGPATH=$NEWVALUE

		get_pattern_value "$TAG_LINE" "logformat="
		(( $? == 0 )) && THE_LOGFORMAT=$NEWVALUE
	fi

	#
	# Obtain the absolute path of the interesting files
	#
	get_absolute_path $THE_BUFFER
	THE_BUFFER=$NEWVALUE
	get_absolute_path $THE_FHPATH
	THE_FHPATH=$NEWVALUE
	get_absolute_path $THE_LOGPATH
	THE_LOGPATH=$NEWVALUE
}

#
# Print the contents of the nfslog.conf file entry we're interested in.
#
function print_nfslogconf {
	echo "defaultdir= " $THE_DEFAULTDIR
	echo "buffer= " $THE_BUFFER
	echo "fhtable= " $THE_FHPATH
	echo "log= " $THE_LOGPATH
	echo "logformat= " $THE_LOGFORMAT
}

#
# Verifies that the buffer, fhtable and logfile all exist if the entires
# in BUF_OK, FHT_OK and LOG_OK are all set to 1. Verifies if FHT_OK or
# LOG_OK are set to 0 that the fhtable or logfile do not exist. Verifies
# that the expected log record was recorded in the logfile. Returns 0 on
# success, non-zero otherwise.
#
function check_nfslog_files {
	[[ -n $DEBUG ]]  && set -x

	LOG_OK=$1
	BUF_OK=$2
	FHT_OK=$3
	get_nfslogconf

	#
	# Check for the existence of the logging files.
	# This area checks for the existence of the log file, buffer file
	# and the fh2path_table files. The contents of the log file is
	# checked in the next section.
	#

	if (( $BUF_OK == 1 )); then
		if [[ ! -f ${THE_BUFFER}_log_in_process ]]; then
		    echo "$TNAME: Could not locate buffer. Tag=$LOGTAG"
		    return 1
		fi
	else
		if [[ -f ${THE_BUFFER}_log_in_process ]]; then
		    echo "$TNAME: Buffer file exists but should not. Tag=$LOGTAG"
		    return 1
		fi
	fi

	if (( $FHT_OK == 1 )); then
		if [ ! -f $THE_FHPATH.* ]; then
		    echo "$TNAME: Could not locate fhtable. Tag=$LOGTAG"
		    return 1
		fi
	else
		if [[ ! -f $THE_FHPATH.* ]]; then
		    echo "$TNAME: File Handle table exists but should not. "
		    echo "\cTag=$LOGTAG"
		    return 1
		fi
	fi

	if (( $LOG_OK == 1 )); then
		if [[ ! -f $THE_LOGPATH ]]; then
		    echo "$TNAME: Could not locate logfile. Tag=$LOGTAG"
		    return 1
		else
		    awk '{printf("%s %s\n", $9, $12)}' $THE_LOGPATH \
		        2>&1 > $STF_TMPDIR/$NAME.awk.$$
		fi
	else
		if [[ ! -f $THE_LOGPATH ]]; then
		    return 0
		else
		    echo "$TNAME: Log file exists but should not. Tag=$LOGTAG"
		    return 1
		fi
	fi

	#
	# Check Log file.
	# This section parses the log file and checks this for the write
	# line from the write to a file in the test.
	#

	if (( $FHT_OK == 1 )); then
		if [[ $THE_LOGFORMAT == extended ]]; then
		    grep "$NFSLOGDDIR/$TESTFILE write" $STF_TMPDIR/$NAME.awk.$$ \
		        > /dev/null 2>&1
		    RET_VAL=$?
		else
		    grep "$NFSLOGDDIR/$TESTFILE i" $STF_TMPDIR/$NAME.awk.$$ \
		        > /dev/null 2>&1
		    RET_VAL=$?
		fi
	else
		if [[ $THE_LOGFORMAT == extended ]]; then
		    grep "$TESTFILE write" $STF_TMPDIR/$NAME.awk.$$ \
		        > /dev/null 2>&1
		    RET_VAL=$?
		else
		    grep "$TESTFILE i" $STF_TMPDIR/$NAME.awk.$$ \
		        > /dev/null 2>&1
		    RET_VAL=$?
		fi
	fi

	if (( $RET_VAL != 0 )); then
		echo "$TNAME: Write operation not logged in logfile."
		echo "\ctag=$LOGTAG. testfile=$TESTFILE"
		cat $THE_LOGPATH
		return 1
	fi

	return 0
}

#
# Return 0 if nfslogd is running, return non-zero otherwise.
# Shutdowns nfslogd to make sure that the nfslog has been flushed.
# the daemon will be started in cleanup{}
#
function kill_nfslogd {
	[[ -n $DEBUG ]]  && set -x

        /usr/bin/pgrep -z $ZONENAME -x -u 0 nfslogd > /dev/null 2>&1
	if (( $? != 0 )); then
            echo "$TNAME: nfslogd not running on server. Tag=$LOGTAG"
            return 1
        fi

        #
        # Wait up to 10 seconds for nfslogd to gracefully handle SIGHUP
        #
        /usr/bin/pkill -HUP -z $ZONENAME -x -u 0 nfslogd
	condition="! /usr/bin/pgrep -z $ZONENAME -x -u 0 nfslogd > /dev/null"
	wait_now 10 "$condition"
        #
        # Kill nfslogd more forcefully if it did not shutdown during
        # the grace period
        #
	if (( $? != 0 )); then
                /usr/bin/pkill -TERM -z $ZONENAME -x -u 0 nfslogd
		(( $? != 0 )) && /usr/bin/pkill -9 -z $ZONENAME -x -u 0 nfslogd
        fi

        return 0
}

# cleanup function on all exit
function cleanup {
        [[ -n $DEBUG ]]  && set -x

	ret=$1
	(( $ret != 0 )) && print_nfslogconf

	rm -rf /etc/nfs/nfslogtab
	cd $Test_Log_Dir/; rm -rf defaults/* absolute/* results/*

	# restart nfslogd for next test.
	touch /etc/nfs/nfslogtab
	/usr/lib/nfs/nfslogd > $STF_TMPDIR/$TNAME.nfslogd.$$ 2>&1
	if (( $? != 0 )); then
            echo "$TNAME: ERROR - failed to restart nfslogd"
	    echo "The failure will also cause next test failed."
            cat $STF_TMPDIR/$TNAME.nfslogd.$$
	    ret=1
        fi
	# wait a while and check nfslogd is running
	condition="/usr/bin/pgrep -z $ZONENAME -x -u 0 nfslogd > /dev/null 2>&1"
	wait_now 20 "$condition"
	if (( $? != 0 )); then
	    echo "$TNAME: ERROR - nfslogd is still not running after 20 seconds"
	    echo "The failure will also cause next test failed."
	    ret=1
	fi

	rm -fr $STF_TMPDIR/*.$$
	exit $ret
}

# start test ...

NAME=$(basename $0)

THE_DEFAULTDIR="/var/nfs"
THE_BUFFER="nfslog_workbuffer"
THE_FHPATH="fhtable"
THE_LOGPATH="nfslog"
THE_LOGFORMAT="basic"

# variables gotten from client system:
STF_TMPDIR=STF_TMPDIR_from_client
DEBUG=$7
TNAME=$1
TESTFILE=$2
LOGTAG=$3
ZONENAME=$(zonename)
Test_Log_Dir="/var/nfs/smtest"

. $STF_TMPDIR/srv_config.vars

# Include common STC utility functions
if [[ -s $STC_GENUTILS/include/nfs-util.kshlib ]]; then
	. $STC_GENUTILS/include/nfs-util.kshlib
else
	. $STF_TMPDIR/nfs-util.kshlib
fi

# Turn on debug info, if requested
[[ -n $DEBUG ]] && set -x

# kill nfslogd to flush nfslog
kill_nfslogd
(( $? != 0 )) && cleanup 1

# verify log, sleep 5 mins to refresh log.
wtime=300
while (( $wtime > 0 )); do
	sleep 5
	echo "sleep 5 ... "
	sync
	check_nfslog_files $4 $5 $6 > $STF_TMPDIR/$NAME.chk.$$ 2>&1
	ret=$?
	(( $ret != 0 )) && wtime=$((wtime - 5)) || break
done

if (( $ret != 0 )); then
	echo "$TNAME: check_nfslog_files failed after sleeping 5 minutes."
	cat $STF_TMPDIR/$NAME.chk.$$
fi

cleanup $ret
