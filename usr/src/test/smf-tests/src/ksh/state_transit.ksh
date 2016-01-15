#!/bin/ksh -p
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
#This script is a tool which applies various service states to either
#system services (or) test services according to usage.
#

bname=`basename $0`

###############################################################################
#This section is configurable section. If you want you can add some more
#validstates to variable "test_state". Also make sure you are going to add
#matching 'output_state' for each test_state. Increase lib_wait_time
#if default value is not sufficient.
###############################################################################

lib_wait_time=${DEFAULT_WAIT_TIME:-30}
test_state="enable disable refresh restart maintenance" # Add more valid states if needed
output_state="online disabled online online maintenance" # Add more valid output states 
					     # if needed

###############################################################################
#	Initialize variables
###############################################################################


typeset child_process="/var/tmp/child_script.$$"
set -A rarray $output_state
typeset pid=$$
typeset ignore_not_online=0
typeset continue_after_fails=0
typeset filename=
typeset time=
typeset global_service_identifier=
typeset bstime=			# service STIME before restart
typeset astime=			# service STIME after restart
typeset bppid=			# process STIME before restart
typeset appid=			# process STIME after restart
typeset fmri=

###############################################################################
#	Usage of this script
###############################################################################

function usage {

	cat >&2 << EOF
Usage: $PROG [-M <instance_FMRI>] [-f <filename>] [-t <timeout_in_seconds>] [-i] [-n <num_of_iter>]
	[-c]

Options:
	-M: Instance FMRI
        -f: filename contains list of online services
	-t: timeout in seconds to restrict runtime of this script.
	-i: to ignore services which are not online
	-n: to iterate the whole process given number of times
	-c: continue the transition even if some services fails
EOF
}

###############################################################################
#	Generic cleanup; called when exit 0, 1, 2 and 15.
###############################################################################

function cleanup {
	print "$bname: Bring back pending service state transition to online"
	rm -f $child_process
	bring_service_back_to_online $global_service_identifier
	if [ $? -ne 0 ]; then
		return 1
	fi
}

###############################################################################
#	Cleanup_usr2 called when current process gets killed by signal
#	"SIGUSR2". SIGUSR2 is sent after <time> seconds if <-t time> is set 
#	when starting this script.
###############################################################################

function cleanup_usr2 {
	print "$bname: This script ran for \"$time\" seconds; Now aborting due
			to SIGUSR2 as per requirement"
	rm -f $child_process
	bring_service_back_to_online $global_service_identifier
	if [ $? -ne 0 ]; then
		return 1
	fi
	exit 0
}

###############################################################################
#	Validate the given file
#		- Check if file size > 0
#		- Check if it is readable
###############################################################################

function validate_file {
	typeset fname="$bname: function - validate_file:"

        [[ $# -ne 1 ]] && {
		print -u2 "$fname: requires one argument - $# passed"
		return 1
	}
	input_file="$1"

	if [[ ! -s $input_file || ! -r $input_file ]]; then
		print -u2 "$fname: $input_file is not readable (or) zero size"
		return 1
	fi

	return 0
}

###############################################################################
#	Validate each service
#		- Verify that service state is "online"
###############################################################################

function validate_eachservice {
	typeset fname="$bname: function - validate_eachservice:"

        [[ $# -ne 1 ]] && {
		print -u2 "$fname: requires one argument - $# passed"
		return 1
	}

	service=$1
	state=`svcprop -p restarter/state $service 2>/dev/null`
	if [ "$state" != "online" ]; then
		return 1
	fi
	return 0
}

###############################################################################
#	Verify that given service exists in the system
###############################################################################
	
function service_exists {
	typeset fname="$bname: function - service_exists:"

	[[ $# -ne 1 ]] && {
		print "$fname : function requires one argument $# passed"
		return 1
	}
	typeset service=$1

	/usr/sbin/svccfg select $service > /dev/null 2>&1
	ret=$?
	return $ret
}

###############################################################################
#	This function checks service state
#		Two arguments : 1. Service name 2. Service state
#	Return value:
#		Return 1 if Service's state is not equal to expected state
###############################################################################


function service_check_state {
	typeset fname="$bname: function - service_check_state:"
	typeset quiet=


	if [ -n "$1" -a "$1" = "-q" ]; then
		quiet=1
		shift
	fi

        [[ $# -ne 2 ]] && {
		print -u2 "$fname: function requires two arguments - $# passed"
		return 2
	}

        typeset service=$1
	typeset statetocheck=$2

	typeset state=
	typeset nstate=

	service_exists $service || {
		print -u2 "$fname: entity $service does not exist"
		return 2
	} 

	state=`svcprop -p restarter/state $service 2>/dev/null`
	[[ -z $state ]] && {
		print -u2 "$fname: svcs did not return a state for service \
			$state"
		return 2
	}

	nstate=`svcprop -p restarter/state $service 2>/dev/null`
	[[ -z $nstate ]] && {
		print -u2 "$fname: svcs did not return have nstate = -"
		return 2
	}

        if [[ "$state" != "$statetocheck" || "$nstate" != "-" ]]; then
		[ -z "$quiet" ] && \
		print -u2 "$fname: service $service returned state $state, \
			not $statetocheck"

		return 1
	fi
        return 0
}

###############################################################################
#	This function waits until service gets transited to given state.
#		Two arguments : 1. Service name 2. Service state
#	Return value:
#		Return 1 if Service's state didn't transit to given state in
#			wait_time
###############################################################################

function service_wait_state {
	typeset fname="$bname: function - service_wait_state:"

	[[ $# -ne 2 && $# -ne 3 ]] && {
		print -u2 "$fname: function requires two or three arguments - $# passed"
		return 2
	}


        typeset service=$1
	typeset state=$2
	typeset wait_time=${3:-$lib_wait_time}
	typeset nsec=0

	while [ $nsec -le $wait_time ]; do
		service_check_state -q $service $state
		[[ $? -eq 0 ]] && {
			print -u2 "$fname: $service transitioned to $state"
			return 0
		}
                sleep 1
		nsec=$((nsec + 1))
	done


	print -u2 "$fname: service did not transition to state $state within
		$wait_time seconds"
	return 1
}

###############################################################################
#	Function name : stop_after_signaled
#
#	Description :	This function is called if this script invoked
#			with <-t time>. Basically, this function forks
#			one more process, which sleeps for given time
#			and sends SIGUSR2 to parent process. This is
#			to restrict the run time of script if user opts.
#
#	Argument : 2 arguments ; 1. Time (in seconds) 2. pid (pid of parent)
###############################################################################
			 

function stop_after_signaled {
	typeset fname="$bname: function - stop_after_signaled:"

        [[ $# -ne 2 ]] && {
		print -u2 "$fname: requires two argument - $# passed"
		return 1
	}

	typeset wait_time="$1"
	typeset parent_pid="$2"

	typeset child_pid=

cat > /var/tmp/child_script.$$ << EOF
	#!/bin/ksh -p
	sleep $wait_time
	ps -p $parent_pid >/dev/null 2>&1
	if [ $? -eq 0 ]; then
		kill -17 $parent_pid >/dev/null 2>&1
	fi
	exit 0
EOF

	chmod 755 /var/tmp/child_script.$$ >/dev/null 2>&1
	if [ $? -ne 0 ]; then
		print -u2 "$fname: chmod 755 /var/tmp/child_script.$$ failed"
		return 1
	fi

	# Call the script

	/var/tmp/child_script.$$ &
	child_pid=$!
	if [ -z $child_pid ]; then
		print -u2 "$fname: Unable to run $childprocess"
		return 1
	fi
	return 0
}

###############################################################################
#	Function name : transit_state
#
#	Description :	This function is called over each service to
#			apply various states and expect the transition
#			to happen.
#
#	Argument : 1 argument ; 1. Servicename
###############################################################################

function transit_state {
	typeset fname="$bname: function - transit_state:"
	typeset atime=
	typeset btime=

	bstime=""
	astime=""
	bppid=""
	appid=""

        [[ $# -ne 1 ]] && {
		print -u2 "$fname: requires one argument - $# passed"
		return 1
	}

	typeset count=0
	typeset service=$1
	global_service_identifier=$service

	print "\n\nFMRI: $service"

	for eachstate in $test_state
	do

		print "		Attempted command: $eachstate"
		expected_state=${rarray[$count]}
		print "		Expected state: $expected_state"
		# If state = restart, then calculate stime of the
		# service before restart

		if [ "$eachstate" = "restart" ]; then
			calculate_pid "before" $service
			if [ $? -ne 0 ]; then
				return 1
			fi
		fi

		# Could use 'svcadm enable -r' here, perhaps
		if [ "$eachstate" = "enable" ]; then
			/usr/sbin/svcadm enable $service > /dev/null 2>&1
		elif [ "$eachstate" = "maintenance" ]; then
			/usr/sbin/svcadm mark maintenance $service > /dev/null 2>&1
		else
			/usr/sbin/svcadm $eachstate $service >/dev/null 2>&1
		fi

		if [ $? -ne 0 ]; then
			print -u2 "$fname: svcadm $eachstate $service failed"
			return 1
		fi

		service_wait_state $service $expected_state >/dev/null 2>&1
		if [ $? -ne 0 ]; then
			tstate=`svcprop -p restarter/state $service 2>/dev/null`
			print "		Actual state: $tstate"
			print -u2 "$fname: Service transition to $expected_state failed"
			return 1
		fi
		print "		Actual state: $expected_state"
		print "		-----------------------------"


		#Verify state transition of dependencies
		check_for_dependencies $service $expected_state
		if [ $? -ne 0 ]; then
			return 1
		fi

		# If state = restart, then calculate stime of the
		# service after restart

		if [ "$eachstate" = "restart" ]; then
			calculate_pid "after" $service
			if [ $? -ne 0 ]; then
				return 1
			fi
		fi


		if [ "$expected_state" = "maintenance" ]; then
			print "		Attempted command: clear"

			svcadm clear $service >/dev/null 2>&1
			if [ $? -ne 0 ]; then
				print -u2 "$fname: svcadm clear $service failed"
				return 1
			fi
			print "		Expected state: online"

			service_wait_state $service "online" >/dev/null 2>&1
			if [ $? -ne 0 ]; then
				tstate=`svcprop -p restarter/state $service 2>/dev/null`
				print "		Actual state: $tstate"
				print -u2 "$fname: Service transition to online failed"
				return 1
			fi

			check_for_dependencies $service "online"
			if [ $? -ne 0 ]; then
				return 1
			fi
			print "		Actual state: online"
			print "		-----------------------------"
		fi

		#If following states attempted, bring service to
		#online


		if [[ $expected_state = "degraded" || \
			$expected_state = "disabled" ]]
		then
			bring_service_back_to_online $service
			if [ $? -ne 0 ]; then
				return 1
			fi
		fi
		count=`expr $count + 1`
	done
	return 0
}

###############################################################################
#	Function name : bring_service_back_to_online
#
#	Description :	This function is called if the service state is
#				- disabled
#				- maintenance
#				- degraded
#	Expected : To bring services back to online.
#
#	Argument : 1 argument : Servicename
###############################################################################

function bring_service_back_to_online 
{
	typeset fname="$bname: function - transit_state:"
        [[ $# -ne 1 ]] && {
		print -u2 "$fname: requires one argument - $# passed"
		return 1
	}

	typeset service="$1"

	tmp_state=`svcprop -p restarter/state $service 2>/dev/null`
	if [ "$tmp_state" = "maintenance" ]; then
		/usr/sbin/svcadm clear $service >/dev/null 2>&1
		if [ $? -ne 0 ]; then
			print -u2 "$fname: svcadm clear $service failed"
			return 1
		fi
	else
		/usr/sbin/svcadm enable $service >/dev/null 2>&1
		if [ $? -ne 0 ]; then
			print -u2 "$fname: svcadm enable $service failed"
			return 1
		fi
	fi

	service_wait_state $service "online" >/dev/null 2>&1
	if [ $? -ne 0 ]; then
		print -u2 "$fname: service_wait_state to online failed"
		return 1
	fi

	return 0
}

###############################################################################
#	Function name : calculate_pid
#
#	Description :	This function is called to verify the different
#			in stime of process and service before restart
#			and after restart
#	Argument : 1 argument : mode and Servicename
###############################################################################


function calculate_pid {
	typeset fname="$bname: function - calculate_pid:"
	typeset mode=$1
	typeset service=$2
	typeset stime=
	typeset ppid=
	typeset line_count=

	stime=`svcs -Ho STIME $service 2>/dev/null`
	if [[ $? -ne 0 || -z $stime ]]; then
		print -u2 "$fname: Error in calculating \
			STIME of $service"
		return 1
	fi
	line_count=`svcs -Hp $service | wc -l 2>/dev/null`
	if [ $? -ne 0 ]; then
		print -u2 "$fname: Error in calculating svcs -Hp $service"
		return 1
	fi

	if [ $line_count -ge 2 ]; then
		ppid=`svcs -Hp $service | grep -v $service | \
			awk '{print $1}' 2>/dev/null`
		if [[ $? -ne 0 || -z $ppid ]]; then
			print -u2 "$fname: Error calculating \
				pid of $service's process"
			return 1
		fi
	fi

	if [ "$mode" = "before" ]; then
		bstime=$stime
		if [ ! -z $ppid ]; then
			bppid=$ppid
		fi
	fi

	if [ "$mode" = "after" ]; then
		astime=$stime
		if [ ! -z $ppid ]; then
			appid=$ppid
		fi

		if [ $bstime = $astime ]; then
			print -u2 "$fname: ERROR Service time unchanged upon \
					restart of $service $bstime $astime"
			return 1
		fi

		if [[ ! -z $appid && ! -z $bppid ]]; then
			set -A array $appid
			typeset tcount=0
			for eachbppid in $bppid
			do
				if [ "$eachbppid" = "${array[$tcount]}" ]; then
					print -u2 "$fname: ERROR: Process id\
						unchanged upon restart \
						of service $service $appid $bppid"
					return 1
				fi
				tcount=`expr $tcount + 1`
			done
		fi
	fi
	return 0
}


# aditya: This function is incorrect, *dependent*
# instances may be disabled by default in which case
# they will never come online.
###############################################################################
#	Function name : check_for_dependencies
#
#	Description :   This function verifies corresponding states for
#			dependencies also.
#
#	Argument : 2:  1. Servicename 2. State
###############################################################################

function check_for_dependencies {

	return 0

	typeset service=$1
	typeset state=$2
	typeset fname="$bname: function - check_for_dependencies"
	typeset dependencies=

	dependencies=`svcs -HD -o FMRI $service 2>/dev/null`
	if [ $? -ne 0 ]; then
		print -u2 "$fname: Error: Calculating dependencies for $service"
		return 1
	fi
	if [ -z $dependencies ]; then
		return 0
	fi

	if [[ "$state" = "disabled" || "$state" = "maintenance" ]]; then
		state="offline"
	fi

	for eachdependencies in $dependencies
	do
		service_wait_state $eachdependencies $state >/dev/null 2>&1
		if [ $? -ne 0 ]; then
			astate=`svcprop -p restarter/state $eachdependencies 2>/dev/null`
			print -u2 "$fname: For dependency $eachdependencies"
			print -u2 "Expected state : $state"
			print -u2 "Actual state : $astate"
			return 1
		fi
	done

	return 0
}


##############################################################################
#main
##############################################################################

# Make sure we run as root
if ! /usr/bin/id | grep "uid=0(root)" > /dev/null 2>&1
then
    	print -u2 "$bname: This script must be run as root."
	exit $STF_UNRESOLVED
fi

# Make sure /usr/bin is first in our path
export PATH=/usr/bin:$PATH

typeset max=1
while getopts :M:f:t:icn: opt
do
	case $opt in
	M)
		# Use supplied argument as instance FMRI.
		fmri=$OPTARG
		;;
	f)
          	#Validate input filename
		filename=$OPTARG
		validate_file $filename
		if [ $? -ne 0 ]; then
			exit 1
		fi
		;;
	t)
          	#Stop the process after time "t" with signal SIGUSR2
		time=$OPTARG
		stop_after_signaled $time $pid
		if [ $? -ne 0 ]; then
			exit 1
		fi
		;;
	i)
          	# to ignore services which are not online
		ignore_not_online=1
		;;
	c)
          	# to ignore services which are not online
		continue_after_fails=1
		;;
	n)
          	#Number of iterations.
		max=$OPTARG
		;;
	*)
          	usage
		exit 2
		;;
	esac
done

# Handle signals
trap cleanup_usr2 USR2
trap cleanup 0 1 2 15

typeset -i PASS=0
typeset -i UNRESOLVED=1
typeset -i FAIL=2

typeset -i result=$PASS

typeset iter=0

while [ $iter -lt $max ]
do

	# If filename is given then get services from file
	if [ ! -z $filename ]; then
	    	for eachservice in `cat $filename`
		do
			validate_eachservice $eachservice
			if [ $? -ne 0 ]; then
				if [ $ignore_not_online -eq 1 ]; then
					print -u2 "\n\n$bname: $eachservice is not online, continuing ..."
					result=$UNRESOLVED
					print -u2 "\nResult: UNRESOLVED\n"
					continue;
				else
					print -u2 "\n$bname: $eachservice is not online\n"
					bring_service_back_to_online $eachservice
					if [ $? -ne 0 ]; then
						print -u2 "\n$bname: Couldnt bring $eachservice online, continuing on ...\n"
						result=$FAIL
						print -u2 "\nResult: FAIL\n"
						if [ $continue_after_fails -eq 1 ]; then
						    continue
						fi
						exit 1
					fi
				fi
			fi

			transit_state $eachservice
			if [ $? -ne 0 ]; then
				result=$FAIL
				print -u2 "\nResult: FAIL\n"
				if [ $continue_after_fails -eq 1 ]; then
					continue
				fi
				exit 1
			fi
			print -u2 "\nResult: PASS\n"
		done
	elif [ ! -z $fmri ]
	then
			validate_eachservice $fmri
			if [ $? -ne 0 ]; then
				if [ $ignore_not_online -eq 1 ]; then
					print -u2 "\n\n$bname: $fmri is not online, continuing ..."
					result=$UNRESOLVED
					print -u2 "\nResult: UNRESOLVED\n"
					exit 1
				else
					print -u2 "\n$bname: $fmri is not online\n"
					bring_service_back_to_online $fmri
					if [ $? -ne 0 ]; then
						print -u2 "\n$bname: Couldnt bring $fmri online, continuing on ...\n"
						result=$FAIL
						print -u2 "\nResult: FAIL\n"
						exit 1
					fi
				fi
			fi

			transit_state $fmri
			if [ $? -ne 0 ]; then
				result=$FAIL
				print -u2 "\nResult: FAIL\n"
				exit 1
			fi
			print -u2 "\nResult: PASS\n"
	else
		# If filename is not given, then get system's online service
		# and play with them
		for eachservice in `svcs -Ho STATE,FMRI | grep online | awk '{print $2}'`
		do
			transit_state $eachservice
			if [ $? -ne 0 ]; then
				result=$FAIL
				print -u2 "\nResult: FAIL\n"
			else
			    	print -u2 "\nResult: PASS\n"
				continue;
			fi
			if [ $continue_after_fails -eq 1 ]; then
				continue;
			else
			    exit 1
			fi
		done
	fi

	iter=`expr $iter + 1`
done

exit 0
