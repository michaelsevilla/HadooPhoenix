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
		sar -f $RESULTSDIR/$PROGRAM.output -S | grep 'AM\|PM' >> ./$PROGRAM.unformatted

		# Delete the file if it already exists
		if [ -e ./kbswpfree_${PROGRAM} ] ; then rm -r ./kbswpfree_${PROGRAM}; fi
		if [ -e ./kbswpused_${PROGRAM} ] ; then rm -r ./kbswpused_${PROGRAM}; fi
		if [ -e ./kbswpcad_${PROGRAM} ] ; then rm -r ./kbswpcad_${PROGRAM}; fi

		# Pull out the correct components
		./parse-columns.py kbswpfree ./${PROGRAM}.unformatted > ./kbswpfree_${PROGRAM}
		./parse-columns.py kbswpused ./${PROGRAM}.unformatted > ./kbswpused_${PROGRAM}
		./parse-columns.py kbswpused ./${PROGRAM}.unformatted > ./kbswpcad_${PROGRAM}
		
		# Remove the temporary file
		rm ./${PROGRAM}.unformatted
	fi
done


