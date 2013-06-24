#! /bin/bash

echo "Dynamically querying the hardware to enhance portability"

usage() {
	echo "USAGE: ./setenvs_dynamic.sh --<option> <arg1> [--<option> <arg2> ...]"
	echo "Options:"
	echo -e "\t--l1_size=ENVAR\tSet the environment variable ENVAR to the L1 data cache size"
	echo -e "\t--l2_size=ENVAR\tSet the environment variable ENVAR L2 cache size"
	echo -e "\t--l3_size=ENVAR\tSet the environment variable ENVAR L3 cache size"
}

get_size() {
	# Get the multipier value
	MULTIPLIER=`echo $VALUE | grep -o 'K\|k\|M\|m\|G\|g\|'`
	
	# Get the raw number value
	if [ $MULTIPLIER="K" ] || [ $MULTIPLIER="k" ] ; then
		NUM_VALUE=`echo $VALUE | sed 's/k//'`
		NUM_VALUE=`echo $VALUE | sed 's/K//'`
		BYTE_VALUE=$((${NUM_VALUE}*1024))
	elif [ $MULTIPLIER="M" ] || [ $MULTIPLIER="m" ] ; then
		NUM_VALUE=`echo $VALUE | sed 's/m//'`
		NUM_VALUE=`echo $VALUE | sed 's/M//'`
		BYTE_VALUE=$((${NUM_VALUE}*1024*1024))
	elif [ $MULTIPLIER="G" ] || [ $MULTIPLIER="g" ] ; then
		NUM_VALUE=`echo $VALUE | sed 's/g//'`
		NUM_VALUE=`echo $VALUE | sed 's/G//'`
		BYTE_VALUE=$((${NUM_VALUE}*1024*1024*1024))
	fi

	# Print out some results	
	echo -n -e "\tVALUE is " 
	echo -n $VALUE
	echo -n -e ", BYTE_VALUE is "
	echo $BYTE_VALUE
	
	# Set the environment variable
	export ${1}=${BYTE_VALUE}
	echo -n "printenv $1: "
	printenv $1

	return 
}

get_l1() {
	echo "Setting the environment variable $1 to the L1 data cache size"
	VALUE=`lscpu | grep "L1d cache" | gawk -F: '{print $2}' | tr -d ' '`
	get_size $1 $VALUE	
	return
}

get_l2() {
	echo "Setting the environment variable $1 to the L2 cache size"
	VALUE=`lscpu | grep "L2 cache" | gawk -F: '{print $2}' | tr -d ' '`
	get_size $1 $VALUE	
	return
}

get_l3() {
	echo "Setting the environment variable $1 to the L3 cache size"
	VALUE=`lscpu | grep "L3 cache" | gawk -F: '{print $2}' | tr -d ' '`
	get_size $1 $VALUE	
	return
}

while true; do
	case $1 in
		-h|--help)
			usage
			exit 0
			;;
		--l1_size)
			get_l1 $2	
			shift
			;;
		--l2_size)
			get_l2 $2
			shift
			;;
		--l3_size)
			get_l3 $2
			shift
			;;
		--)
			shift
			break
			;;
		*)	
			shift
			break
			;;
	esac
	shift
done
