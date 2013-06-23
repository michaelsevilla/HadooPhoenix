#! /bin/bash
# Author: Michael Sevilla (msevilla@ucsc.edu)
# Date: 1/25/2012

function usage(){
	echo ""
	echo "Usage: ./prepare_gnuplot <results dir> <program>"
	echo ""
	echo "Example: "
	echo "   ./prepare_gnuplot.sh ~/data word_count"
	exit
}

if [ $# -lt 2 ] ; then 
	usage 
fi

RESULTSDIR=`echo "${1}" | sed -e "s/\/*$//"`
PROGRAM=`echo "${2}" | sed -e "s/\/*$//"`

if [ -e ./${PROGRAM}.timing ] ; then rm ${PROGRAM}.timing; fi

# Uncomment for single node
cat ${RESULTSDIR}/timing-${PROGRAM}.out | grep 'real\|bytes\|terminate' >> ${PROGRAM}
#cat ${RESULTSDIR}/timing-${PROGRAM}.out | grep 'sys\|bytes\|terminate' >> ${PROGRAM}
#cat ${RESULTSDIR}/timing-${PROGRAM}.out | grep 'user\|bytes\|terminate' >> ${PROGRAM}
# Uncomment for Hadoop
#cat ${RESULTSDIR}/timing-${PROGRAM}.out | grep 'real\|Iteration\|terminate' >> ${PROGRAM}
./parse_columns.py ${PROGRAM} > ${PROGRAM}.unformatted
./convert_to_seconds.py ${PROGRAM}.unformatted 1 > ${PROGRAM}.timing

rm ${PROGRAM}
rm ${PROGRAM}.unformatted
