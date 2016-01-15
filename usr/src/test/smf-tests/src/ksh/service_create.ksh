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

trap cleanup 0 1 2 15

readonly instance_template=${STF_SUITE}/tests/stress/instance_template.xml

function usage
{
	cat >&2 << EOF
Usage: $PROG -b <depth> | -s <num_of_services> -i <num_of_instances>
	[ -S service prefix ] [ -m ] [-t <service_app>]

Options:
	-b: depth of the B-tree to create, default = $DEPTH
	-s: number of services
	-i: number of instances, default = $NUM_INSTANCES
	-S: service name prefix
	-m: save manifest file used for service creation
	-t: service_app used to create services
EOF
}


function cleanup {

	[[ -z $save_manifest ]] &&  {
		echo "removing manifest"
		rm -f ${manifest_file}
	}
}
	

function create_binary_manifest
{


	cat <<  EOF	>> $manifest_file
<?xml version="1.0" encoding="UTF-8" standalone="no"?>

<!DOCTYPE service_bundle SYSTEM "/usr/share/lib/xml/dtd/service_bundle.dtd.1">

<service_bundle type="manifest" name="robust_testing">
EOF

	nlevel=$DEPTH
	howmany=1
	x=0
	while [[ $x -le $nlevel ]]
	do
		y=1
		while [[ $y -le $howmany ]]
		do
			service_name=${PREFIX}_$$_${x}_${y}

			cat << EOF >> $manifest_file

<service
	name="${service_name}"
	type="service"
	version="1">

EOF
			if [[ $x -ne 0 ]] 
			then
				instance_count=0
				while [[ $instance_count -le $NUM_INSTANCES ]]
				do
					instance_name=instance_${instance_count}

					# Add the dependency on the "parent"
					(( x_p = x - 1 ))
					(( y_p = (y + 1) / 2 ))
					dependent_service=svc:/${PREFIX}_$$_${x_p}_${y_p}:${instance_name}

					/usr/lib/cpp -DDEPENDENT -P ${instance_template} | \
					sed "s?SERVICE_NAME?${service_name}?" | \
					sed "s?TEST_DEPENDENCY?${dependent_service}?" |\
					sed "s?SERVICE_APP?${SERVICE_APP}?" | \
					sed "s?OUTFILE_NAME?${service_name}?" | \
					sed "s?INSTANCE_NAME?${instance_name}?" >> $manifest_file
					(( instance_count = instance_count + 1 ))
				done
			else
				instance_count=0
				while [[ $instance_count -le $NUM_INSTANCES ]]
				do
					instance_name=instance_${instance_count}
					/usr/lib/cpp -P ${instance_template} | \
					sed "s?SERVICE_NAME?${service_name}?" | \
					sed "s?TEST_DEPENDENCY?${dependent_service}?" |\
					sed "s?SERVICE_APP?${SERVICE_APP}?" | \
					sed "s?OUTFILE_NAME?${service_name}?" | \
					sed "s?INSTANCE_NAME?${instance_name}?" >> $manifest_file
					(( instance_count = instance_count + 1 ))
				done


			fi
			echo "</service>" >> $manifest_file
			(( y = y + 1 ))
			(( num_instances_created = num_instances_created + 1 ))
		done


		(( x = x + 1 ))
		(( howmany = howmany * 2 ))
	done
	echo "</service_bundle>" >> $manifest_file

}

# Set defaults - these are overwritten with -s and -i
typeset -i NUM_SERVICES=20
typeset -i NUM_INSTANCES=1
typeset -i DEPTH=5
typeset PREFIX=FOO

typeset -i num_instances_created=0
readonly svccfg_script_file="/var/tmp/svccfg_config_file"

typeset SERVICE_APP=${STF_SUITE}/tests/bin/$(uname -p)/service_app

typeset manifest_file=/var/tmp/manifest.$$.xml


# flags
typeset save_manifest=
typeset btree=
typeset regular=


while getopts ':h?b:S:s:i:mt:' opt
do
	case $opt in

	h) 	usage
	   	exit 0
	   	;;

	m) 	save_manifest=TRUE
		print manifest_file is $manifest_file
		;;
		
	b) 	DEPTH=$OPTARG
		btree=TRUE
		;;

	t)	SERVICE_APP=$OPTARG
		;;

	S) 	PREFIX=$OPTARG
		;;

	s) 	NUM_SERVICES=$OPTARG
		regular=TRUE
		;;

	i) 	NUM_INSTANCES=$OPTARG
		;;

	# undefined option or -? for help
	\?) 	[[ $OPTARG = '?' ]] && {
			usage
			exit 0
		}
		print -u2 -- "-$OPTARG: invalid option or action"
		exit 1
		;;

	# no option argument
	:)	print -u2 -- "-$OPTARG: option argument expected"
		exit 1
		;;
		
	esac

done

[[ ! -f $instance_template ]] && {
	print -u2 "ERROR: could not access $instance_template"
	exit 1
}

# Check that someone didn't specify -b with one or both
# of the options -s.
[[ -n $btree && -n $regular ]] && {
	print -u2 "ERROR: can not specify b-tree -b option AND regular options -s"
	exit 1
}


# Default to regular tree
[[ -z $btree && -z $regular ]] && {
	regular=TRUE
}

# before starting make sure that directories required by test
# service exist.
service_log_dir=/var/tmp/service_logs
statefile_dir=/var/tmp/statefiles
for dir in ${service_log_dir} ${statefile_dir}
do
	[[ ! -d ${dir} ]] && {
		mkdir ${dir}
		[[ $? -ne 0 ]] && {
			print -u2 "ERROR: Could not create ${dir}"
			exit 1
		}
	}
done
# check that test service exists
[[ ! -x  ${SERVICE_APP} ]] && {
	print -u2 "ERROR: test service ${SERVICE_APP} not available"
	exit 1
}


if [[ -n $regular ]] 
then

	print "Creating $NUM_SERVICES services with $NUM_INSTANCES instances each .."
	count=0
	cat <<  EOF	>> $manifest_file
<?xml version="1.0" encoding="UTF-8" standalone="no"?>

<!DOCTYPE service_bundle SYSTEM "/usr/share/lib/xml/dtd/service_bundle.dtd.1">

<service_bundle type="manifest" name="robust_testing">
EOF
	while [[ $count -lt $NUM_SERVICES ]];
	do
		service_name=${PREFIX}_$$_$count
		instance_count=0
		fmrilist=""
		cat << EOF >> $manifest_file
<service
	name="${service_name}"
	type="service"
	version="1">
EOF

		while [[ $instance_count -lt $NUM_INSTANCES ]]
		do
			instance_name="${PREFIX}_$$_instance_$instance_count"
			outfile_name=${service_name}_${instance_name}
			echo outfile_name is $outfile_name
			/usr/lib/cpp -P ${instance_template} | \
			sed "s?SERVICE_NAME?${service_name}?"  | \
			sed "s?SERVICE_APP?${SERVICE_APP}?" | \
			sed "s?OUTFILE_NAME?${outfile_name}?" | \
			sed "s?INSTANCE_NAME?$instance_name?" >> $manifest_file

			instance_count=`expr $instance_count + 1`
		done
		echo "</service>" >> $manifest_file
		count=`expr $count + 1`

		(( num_instances_created = num_instances_created + instance_count ))
	done
	echo "</service_bundle>" >> $manifest_file
else
	create_binary_manifest
fi

svccfg validate ${manifest_file}
ret=$?
[[ $? -ne 0 ]] && {
	print -u2 "ERROR: svccfg validate did not succeed on \
manifest ${manifest_file}"

	exit 1
}


print "Imporing manifest with services"
svccfg import ${manifest_file} 
[[ $? -ne 0 ]] && {
	print -u2 "ERROR: svccfg import did not succeed on \
manifest ${manifest_file}"

	exit 1
}

# Check, as best I can (easily) that the services are online
# Calculate the timeout value to wait based on how many instances create
typeset -i time_to_wait
(( time_to_wait =  5 * ${num_instances_created} ))
echo num_instances_created is $num_instances_created


while [[ $time_to_wait -gt 0 ]]
do
	print "Checking that services are online . . ."
	eval svcs -H -o STATE "${PREFIX}_$$"\* | egrep -s -v online
	ret=$?
	[[ $ret -eq 0 ]] && {
		sleep 5
		(( time_to_wait = time_to_wait - 5 ))
		continue
	}
	print "All services created are online"
	exit 0
done

print -u2 "ERROR: Not all services created online"
exit 1
	


