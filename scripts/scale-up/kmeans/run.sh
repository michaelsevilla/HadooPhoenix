#! /bin/bash
# Script to run 3 versions of the same word_count applications (sequential, mapreduce, and pthreads)
# 
# Author: Michael Sevilla (msevilla@ucsc.edu)
# Date: 2/24/2012
# Updated: 5/15/2013

# Instantiate directory locations and constants
OUTPUTDIR=`echo "${1}" | sed -e "s/\/*$//"`
VMLINUX=/usr/lib/debug/boot/vmlinux-3.2.0-33-generic
PHOENIX_KMEANS=/home/msevilla/Programs/Phoenix++/tests/kmeans
DMTCP=/home/msevilla/dmtcp-trunk
CKPTDIR=/data2/Checkpoints/run2
CKPTINTERVAL=300

# Enable scale-out properties
FAULTOLERANCE_ENABLED=0
PORTABILITY_ENABLED=1
MONITORING_ENABLED=1

function usage(){
	echo ""
	echo "Script that runs the word_count modules in the Phoenix MapReduce distribution"
	echo "   Usage: ./run.sh <output directory>"
	echo ""
	echo "Example: "
	echo "   run.sh ./deliverables "
	exit
}
function start_oprofile(){
	sudo opcontrol --deinit
	sudo opcontrol --init
	sudo opcontrol --vmlinux=$VMLINUX
	sudo opcontrol --reset
	sudo opcontrol --event=CPU_CLK_UNHALTED:500000 --event=RESOURCE_STALLS:6000000
	sudo opcontrol --start
}
function stop_oprofile(){
	sudo opcontrol --stop
	sudo opcontrol --dump
	sudo opcontrol --stop
	sudo opcontrol -h
}

# Start the script
if [ $# -lt 1 ] ; then usage; fi

# Print job paramaters to a file
echo "Workload: word_count" > $OUTPUTDIR/log.txt
echo "Date: " `date` >> $OUTPUTDIR/log.txt
echo "Output directory: $OUTPUTDIR" >> $OUTPUTDIR/log.txt
echo "Program directory: $PHOENIX_KMEANS" >> $OUTPUTDIR/log.txt
echo "Data directory: $DATA" >> $OUTPUTDIR/log.txt
echo "Checkpoint directory: ${CKPTDIR}" >> $OUTPUTDIR/log.txt
echo "Chunk size: $CHUNK bytes" >> $OUTPUTDIR/log.txt
echo "--------------------------------------------------" >> $OUTPUTDIR/log.txt
echo "" >> $OUTPUTDIR/log.txt

# Construct directories
if [ -e ${DATA}/input ] ; then rm -r ${DATA}/input; echo "-- deleted ${DATA}/input" >> $OUTPUTDIR/log.txt; fi
if [ -e ${OUTPUTDIR}/ ] ; then rm -r ${OUTPUTDIR}/*; echo "-- deleted ${OUTPUTDIR}/*" >> $OUTPUTDIR/log.txt; fi
mkdir ${OUTPUTDIR}/images ${OUTPUTDIR}/symbols ${OUTPUTDIR}/timings ${OUTPUTDIR}/sar
echo "-- created ${OUTPUTDIR}/{images, symbols, timings, sar}" >> $OUTPUTDIR/log.txt
echo "" >> $OUTPUTDIR/log.txt

NUMPOINTS=0
INTERVALS=10000000
for i in {1..300}
do
	NUMPOINTS=$((${NUMPOINTS}+${INTERVALS}))	

	# Perform benchmark every 5th cycle
	for j in kmeans-seq
	do
		echo "--------------------------------------------------" >> $OUTPUTDIR/log.txt
		echo "-- $j iteration $i (Points: ${NUMPOINTS})" >> $OUTPUTDIR/log.txt
		echo "--------------------------------------------------" >> $OUTPUTDIR/log.txt
		echo "${NUMPOINTS} points" >> $OUTPUTDIR/timings/timing-${j}.out

		if [ $MONITORING_ENABLED == 1 ] ; then 
			echo "-- starting SAR..." >> $OUTPUTDIR/log.txt
			sar -o $OUTPUTDIR/sar/sar-${i}-${j}.output 2 >/dev/null 2>&1 &
			sar_pid=$!;
		fi
		if [ $FAULTOLERANCE_ENABLED == 1 ] ; then 
			echo "-- clearing checkpoint directory.." >> $OUTPUTDIR/log.txt
			echo "\tbefore: " `ls -alh ${CKPTDIR}` >> $OUTPUTDIR/log.txt
			rm -r ${CKPTDIR}/* 2>> $OUTPUTDIR/log.txt

			echo "-- running benchmark..." >> $OUTPUTDIR/log.txt
			(time \
			${DMTCP}/bin/dmtcp_checkpoint \
			--quiet \
			--no-gzip \
			--ckptdir ${CKPTDIR} \
			--interval ${CKPTINTERVAL} \
			${PHOENIX_KMEANS}/${j} -p ${NUMPOINTS}) 1>> $OUTPUTDIR/log.txt 2>> $OUTPUTDIR/timings/timing-${j}.out
		else
			echo "-- running benchmark..." >> $OUTPUTDIR/log.txt
			(time ${PHOENIX_KMEANS}/${j} -p ${NUMPOINTS}) 1>> $OUTPUTDIR/log.txt 2>> $OUTPUTDIR/timings/timing-${j}.out
		fi

		# Cleaning up
		if [ $MONITORING_ENABLED == 1 ] ; then kill $sar_pid 1>> $OUTPUTDIR/log.txt; fi
	done
done
echo "" >> $OUTPUTDIR/log.txt
echo "run.sh completed successfully (apparently) and is done!" >> $OUTPUTDIR/log.txt
