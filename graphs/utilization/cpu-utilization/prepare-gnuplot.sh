#! /bin/bash
# Author: Michael Sevilla (msevilla@ucsc.edu)
# Date: 1/25/2012

function usage(){
	echo ""
	echo "Usage: ./prepare_gnuplot <results dir> <program>"
	echo ""
	echo "Example: "
	echo "   ./prepare_gnuplot.sh ~/Results/sar word_count-seq"
	exit
}

set -e

if [ $# -lt 2 ] ; then 
	usage 
fi

RESULTSDIR=`echo "${1}" | sed -e "s/\/*$//"`
PROGNAME=`echo "${2}" | sed -e "s/\/*$//"`

if [ -e ./${PROGRAM}.timing ] ; then rm ${PROGRAM}.timing; fi

for i in {1..300}
do
	if [ $((${i}%1)) -eq 0 ] ; then 
		PROGRAM=sar-${i}-${PROGNAME}
		echo "Trying $PROGRAM"
		sar -f $RESULTSDIR/$PROGRAM.output | grep 'AM\|PM' | grep 'all' >> ./$PROGRAM.unformatted

		# Delete the file if it already exists
		if [ -e ./user_${PROGRAM} ] ; then rm -r ./user_${PROGRAM}; fi
		if [ -e ./sys_${PROGRAM} ] ; then rm -r ./sys_${PROGRAM}; fi
		if [ -e ./io_wait_${PROGRAM} ] ; then rm -r ./io_wait_${PROGRAM}; fi

		# Pull out the correct components
		./parse_columns.py user ./${PROGRAM}.unformatted > ./user_${PROGRAM}
		./parse_columns.py sys ./${PROGRAM}.unformatted > ./sys_${PROGRAM}
		./parse_columns.py io_wait ./${PROGRAM}.unformatted > ./io_wait_${PROGRAM}
		
		# Remove the temporary file
		rm ./${PROGRAM}.unformatted
	fi
done


