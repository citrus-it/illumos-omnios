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

#This script is to inject error by killing given entities


bname=`basename $0`

#############################################################################
#
#	This tool is an Error-Injector. It kills given entities
#	and verify whether services continue in the same state
#	after injecting error.
#	
#############################################################################

#############################################################################
#	Usage of this script
#############################################################################

function usage
{
        cat >&2 << EOF
Usage: $PROG [-n <num_of_times>] [-f <service-list-file>] [-s] [-c] [-i]
	[-l <list of process(es) names>] [-t <timeout in seconds>]

Options:
        -n: number of attempts entities are killed
        -f: text file contains list of online services
        -s: Kill svc.startd
        -c: Kill svc.configd
        -i: Kill init
	-l: Input list of process(es) associated with services
	-t: timeout(in seconds) to wait for process restart (default = 10)
EOF
}

###############################################################################
#       Validate the given file
#               - Check if file size > 0
#               - Check if it is readable
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

##############################################################################
#main
##############################################################################

#
#
#Assign list of entities in an array; Please add any test service processes
#in this array to kill and verify them
#

##############################################################################
typeset list_of_entities=""	#set by -l option
typeset process=		#set by -s (or) -i (or) -c options
typeset timeout=		#in seconds
##############################################################################



##############################################################################
#For each entity do following
#	Get the state of all services in the system
#	kill the entity
#	Make sure entity is restarted
#	Get the state of all sevices in the system after killing entity
#	Make sure state of service remains unchanged before and after killing
##############################################################################

typeset max=1
typeset num_of_attempts=1
while getopts :f:n:scil:t: opt
do
        case $opt in
        f)
                #Set the build flag
                filename=$OPTARG
                validate_file $filename
                if [ $? -ne 0 ]; then
                        exit 1
                fi
                ;;
        n)
                # number of attempts entities are killed
                num_of_attempts=$OPTARG
                ;;
	s)
		process="$process svc.startd"
		;;
	c)
		process="$process svc.configd"
		;;
	i)
		process="$process init"
		;;
	l)
		list_of_entities="$process $OPTARG"
		;;
	t)
		timeout=$OPTARG
		;;
        *)
                usage
                exit 2
                ;;
        esac
done

if [ -z $timeout ]; then
	timeout=10	#in seconds
fi

if [ -z $list_of_entities ]; then
	if [ ! -z $process ]; then
		list_of_entities=$process
	fi
fi

if [ -z $list_of_entities ]; then
	print -u2 "$bname: No process(es) or entities to inject error"
	usage
	exit 2
fi


typeset count=0
while [ $num_of_attempts -gt 0 ]
do

	for each_entity in $list_of_entities
	do

		print "$bname: Kill Entity: $each_entity"
		print "$bname: ========================="

		#Kill the given entity; Attempt for two times

		try_again=2

		num_of_process=0 #To get initial number of given process
			         #This is to verify test services and processes
				 #which has their test processes in same name

		while [ $try_again -gt 0 ]; do
			num_of_process=`pgrep $each_entity | wc -l 2>/dev/null`
			if [ $? -ne 0 ]; then
				sleep 1
			fi
			try_again=`expr $try_again - 1`
		done

		if [ `zonename` = "global" ]; then


			if [ "$each_entity" = "init" ]; then
				pkill -RTMAX -z 0 $each_entity >/dev/null 2>&1
			else
				pkill -z 0 $each_entity >/dev/null 2>&1
			fi
		else
			if [ "$each_entity" = "init" ]; then
				pkill -RTMAX $each_entity >/dev/null 2>&1
			else
				pkill $each_entity >/dev/null 2>&1
			fi
		fi
		ret=$?
		if [ $ret -ne 0 ]; then
			print -u2 "$bname: ERROR: $each_entity is not running/unable to kill: exit status = $ret"
			exit 1
		fi

		#Wait for given timeout seconds until entity restarts
		started=0
		start_time=0
		while [[ $start_time -lt $timeout ]]
		do
			process_count=`pgrep $each_entity | wc -l 2>/dev/null`
			if [[ $? -ne 0 || -z $process_count ]]; then
				sleep 1
				start_time=`expr $start_time + 1`
				continue
			else
				if [ $num_of_process -ne $process_count ]; then
					sleep 1
					start_time=`expr $start_time + 1`
					continue
				else
					print "$bname: $each_entity restarted "
					started=1
					break
				fi
			fi
		done

		if [ $started -ne 1 ]; then
			print -u2 "$each_entity failed to restart (or)"
			print -u2 "$bname: ERROR: Process count before and after killing the process doesn't match"
			print -u2 "start time = $start_time"
			print -u2 "Before killing : $num_of_process"
			print -u2 "After killing: $process_count"
			exit 1
		fi

		print "$bname: ========================="
		sleep 10 #Interval between each kill.
	done

	count=`expr $count + 1`
	print "$bname: \t Number of attempts entities are killed : $count"
	num_of_attempts=`expr $num_of_attempts - 1`
	sleep 10	#Interval between each kill.
done

exit 0
