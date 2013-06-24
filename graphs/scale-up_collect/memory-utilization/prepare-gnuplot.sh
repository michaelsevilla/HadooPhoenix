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
		sar -f $RESULTSDIR/$PROGRAM.output -r | grep 'AM\|PM' >> ./$PROGRAM.unformatted
	
		# Delete the file if it already exists
		if [ -e ./kbmemfree_${PROGRAM} ] ; then rm -r ./kbmemfree_${PROGRAM}; fi
		if [ -e ./kbbuffers_${PROGRAM} ] ; then rm -r ./kbbuffers_${PROGRAM}; fi
		if [ -e ./kbcommitted_${PROGRAM} ] ; then rm -r ./kbcommitted_${PROGRAM}; fi
		if [ -e ./kbcached_${PROGRAM} ] ; then rm -r ./kbcached_${PROGRAM}; fi
		if [ -e ./kbmemused_${PROGRAM} ] ; then rm -r ./kbmemused_${PROGRAM}; fi
	
		# Pull out the correct components
		./parse-columns.py kbmemfree ./${PROGRAM}.unformatted > ./kbmemfree_${PROGRAM}
		./parse-columns.py kbbuffers ./${PROGRAM}.unformatted > ./kbbuffers_${PROGRAM}
		./parse-columns.py kbcommitted ./${PROGRAM}.unformatted > ./kbcommitted_${PROGRAM}
		./parse-columns.py kbcached ./${PROGRAM}.unformatted > ./kbcached_${PROGRAM}
		./parse-columns.py kbmemused ./${PROGRAM}.unformatted > ./kbmemused_${PROGRAM}
		
		# Remove the temporary file
		rm ./${PROGRAM}.unformatted
	fi
done


