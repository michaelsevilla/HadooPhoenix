#! /bin/bash
# Author: Michael Sevilla (msevilla@ucsc.edu)

# Instantiate directory locations and constants
OUTPUTDIR=`echo "${1}" | sed -e "s/\/*$//"`
PHOENIX_WC=/home/msevilla/Programs/Scalability/sop1_parallelism/tests/word_count
DATA=/data1/Data/randomtextwriter-input
CHUNK=1073741824

# Enable scale-out properties

function usage(){
	echo ""
	echo "Script that runs the word_count modules in the Phoenix MapReduce distribution"
	echo "   Usage: ./run.sh <output directory>"
	echo ""
	echo "Example: "
	echo "   run.sh ./deliverables "
	exit
}

# Start the script
if [ $# -lt 1 ] ; then usage; fi

# Construct directories
if [ -e ${OUTPUTDIR}/ ] ; then rm -r ${OUTPUTDIR}/*; echo "-- deleted ${OUTPUTDIR}/*" >> $OUTPUTDIR/log.txt; fi
mkdir ${OUTPUTDIR}/images ${OUTPUTDIR}/symbols ${OUTPUTDIR}/timings ${OUTPUTDIR}/sar
echo "-- created ${OUTPUTDIR}/{images, symbols, timings, sar}" >> $OUTPUTDIR/log.txt
echo "" >> $OUTPUTDIR/log.txt

for i in 4 8 16 32 64
do
	export MAPRED_NPROCESSORS=${i}
	echo "-- running benchmark... with ${i} processors" >> $OUTPUTDIR/log.txt
	(time ${PHOENIX_WC}/word_count ${DATA}/total_input 10) 1>> $OUTPUTDIR/log.txt 2>> $OUTPUTDIR/timings/timing-${i}-threads.out
done
echo "" >> $OUTPUTDIR/log.txt
echo "run.sh completed successfully (apparently) and is done!" >> $OUTPUTDIR/log.txt
