#! /bin/bash
# Script to run 3 versions of the same word_count applications (sequential, mapreduce, and pthreads)
# 
# Author: Michael Sevilla (msevilla@ucsc.edu)
# Date: 2/24/2012
# Updated: 5/15/2013

# Instantiate directory locations and constants
OUTPUTDIR=`echo "${1}" | sed -e "s/\/*$//"`
#VMLINUX=/usr/lib/debug/boot/vmlinux-3.2.0-33-generic
VMLINUX=/usr/lib/debug/lib/modules/2.6.18-128.1.1.el5/vmlinux
#PHOENIX_WC=/home/msevilla/Programs/Phoenix++/tests/word_count
PHOENIX_WC=/home/msevilla/Programs/Phoenix2/tests/word_count
DATA=/data1/Data/randomtextwriter-input
#DMTCP=/home/msevilla/Programs/DMTCP
DMTCP=/home/msevilla/dmtcp-trunk
CKPTDIR=/data1/Checkpoints/run1
CKPTINTERVAL=300
#CHUNK=1073741824
CHUNK=268435456

# Enable scale-out properties
FAULTOLERANCE_ENABLED=0
PORTABILITY_ENABLED=0
MONITORING_ENABLED=0
OPROFILE_ENABLED=1

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
	#sudo opcontrol --event=CPU_CLK_UNHALTED:600000 --event=l1d:2000000:0x01 --event=l1d:2000000:0x02 --event=l1d:2000000:0x04 --event=l1d:2000000:0x08
	sudo opcontrol --event=CPU_CLK_UNHALTED:600000 --event=l1d:2000000
	#--event=l2_rqsts:200000 --event=l2_trans:200000
	#sudo opcontrol --event=CPU_CLK_UNHALTED:600000 --event=l1d:2000000 --event=l2_rqsts:200000 --event=l2_trans:200000 
	#--event=dtlb_load_misses:200000
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
echo "Program directory: $PHOENIX_WC" >> $OUTPUTDIR/log.txt
echo "Data directory: $DATA" >> $OUTPUTDIR/log.txt
echo "Checkpoint directory: ${CKPTDIR}" >> $OUTPUTDIR/log.txt
echo "Chunk size: $CHUNK bytes" >> $OUTPUTDIR/log.txt
echo "--------------------------------------------------" >> $OUTPUTDIR/log.txt
echo "" >> $OUTPUTDIR/log.txt

# Construct directories
#if [ -e ${DATA}/input ] ; then rm -r ${DATA}/input; echo "-- deleted ${DATA}/input" >> $OUTPUTDIR/log.txt; fi
if [ -e ${OUTPUTDIR}/ ] ; then rm -r ${OUTPUTDIR}/*; echo "-- deleted ${OUTPUTDIR}/*" >> $OUTPUTDIR/log.txt; fi
mkdir ${OUTPUTDIR}/images ${OUTPUTDIR}/symbols ${OUTPUTDIR}/timings ${OUTPUTDIR}/sar
echo "-- created ${OUTPUTDIR}/{images, symbols, timings, sar}" >> $OUTPUTDIR/log.txt
echo "" >> $OUTPUTDIR/log.txt

for i in 7
do
	INPUT_SIZE=$((${CHUNK}*${i}))
	# Append the last chunk to the input
	#head -c ${INPUT_SIZE} ${DATA}/total_input | tail -c ${CHUNK} >> ${DATA}/input
	NFILE=$((${i}-1))

	# Perform benchmark every 5th cycle
	#if [ ${i} -gt 238 ] ; then 
	if [ $((${i}%1)) -eq 0 ] ; then 
		for j in word_count word_count-setenvs
		do
			echo "--------------------------------------------------" >> $OUTPUTDIR/log.txt
			echo "-- $j iteration $i ($CHUNK x $i = $INPUT_SIZE)" >> $OUTPUTDIR/log.txt
			echo "--------------------------------------------------" >> $OUTPUTDIR/log.txt
			#echo "Input size: " `ls -alh ${DATA}/input` >> $OUTPUTDIR/log.txt

			if [ $MONITORING_ENABLED == 1 ] ; then 
				echo "-- starting SAR..." >> $OUTPUTDIR/log.txt
				sar -o $OUTPUTDIR/sar/sar-${i}-${j}.output 2 >/dev/null 2>&1 &
				sar_pid=$!;
			fi
			if [ $OPROFILE_ENABLED == 1 ] ; then 
				echo "-- starting OProfile..." >> $OUTPUTDIR/log.txt
				start_oprofile
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
				${PHOENIX_WC}/${j} ${DATA}/input 10) 1>> $OUTPUTDIR/log.txt 2>> $OUTPUTDIR/timings/timing-${j}.out
			else
				echo "-- running benchmark..." >> $OUTPUTDIR/log.txt
				echo "input0 ${INPUT_SIZE} bytes" >> $OUTPUTDIR/timings/timing-${j}.out
				(time ${PHOENIX_WC}/${j} ${DATA}/input0 10) 1>> $OUTPUTDIR/log.txt 2>> $OUTPUTDIR/timings/timing-${j}.out
				(time ${PHOENIX_WC}/${j} ${DATA}/part-00000 10) 1>> $OUTPUTDIR/log.txt 2>> $OUTPUTDIR/timings/timing-${j}.out
				(time ${PHOENIX_WC}/${j} ${DATA}/part-00014 10) 1>> $OUTPUTDIR/log.txt 2>> $OUTPUTDIR/timings/timing-${j}.out
				(time ${PHOENIX_WC}/${j} ${DATA}/part-00151 10) 1>> $OUTPUTDIR/log.txt 2>> $OUTPUTDIR/timings/timing-${j}.out
				echo "input1 ${INPUT_SIZE} bytes" >> $OUTPUTDIR/timings/timing-${j}.out
				(time ${PHOENIX_WC}/${j} ${DATA}/input1 10) 1>> $OUTPUTDIR/log.txt 2>> $OUTPUTDIR/timings/timing-${j}.out
				(time ${PHOENIX_WC}/${j} ${DATA}/part-00012 10) 1>> $OUTPUTDIR/log.txt 2>> $OUTPUTDIR/timings/timing-${j}.out
				(time ${PHOENIX_WC}/${j} ${DATA}/part-00001 10) 1>> $OUTPUTDIR/log.txt 2>> $OUTPUTDIR/timings/timing-${j}.out
				(time ${PHOENIX_WC}/${j} ${DATA}/part-00129 10) 1>> $OUTPUTDIR/log.txt 2>> $OUTPUTDIR/timings/timing-${j}.out
				echo "input2 ${INPUT_SIZE} bytes" >> $OUTPUTDIR/timings/timing-${j}.out
				(time ${PHOENIX_WC}/${j} ${DATA}/input2 10) 1>> $OUTPUTDIR/log.txt 2>> $OUTPUTDIR/timings/timing-${j}.out
				(time ${PHOENIX_WC}/${j} ${DATA}/part-00142 10) 1>> $OUTPUTDIR/log.txt 2>> $OUTPUTDIR/timings/timing-${j}.out
				(time ${PHOENIX_WC}/${j} ${DATA}/part-00091 10) 1>> $OUTPUTDIR/log.txt 2>> $OUTPUTDIR/timings/timing-${j}.out
				(time ${PHOENIX_WC}/${j} ${DATA}/part-00144 10) 1>> $OUTPUTDIR/log.txt 2>> $OUTPUTDIR/timings/timing-${j}.out
				echo "input3 ${INPUT_SIZE} bytes" >> $OUTPUTDIR/timings/timing-${j}.out
				(time ${PHOENIX_WC}/${j} ${DATA}/input3 10) 1>> $OUTPUTDIR/log.txt 2>> $OUTPUTDIR/timings/timing-${j}.out
				(time ${PHOENIX_WC}/${j} ${DATA}/part-00010 10) 1>> $OUTPUTDIR/log.txt 2>> $OUTPUTDIR/timings/timing-${j}.out
				(time ${PHOENIX_WC}/${j} ${DATA}/part-00041 10) 1>> $OUTPUTDIR/log.txt 2>> $OUTPUTDIR/timings/timing-${j}.out
				(time ${PHOENIX_WC}/${j} ${DATA}/part-00002 10) 1>> $OUTPUTDIR/log.txt 2>> $OUTPUTDIR/timings/timing-${j}.out
				echo "input4 ${INPUT_SIZE} bytes" >> $OUTPUTDIR/timings/timing-${j}.out
				(time ${PHOENIX_WC}/${j} ${DATA}/input4 10) 1>> $OUTPUTDIR/log.txt 2>> $OUTPUTDIR/timings/timing-${j}.out
				(time ${PHOENIX_WC}/${j} ${DATA}/part-00091 10) 1>> $OUTPUTDIR/log.txt 2>> $OUTPUTDIR/timings/timing-${j}.out
				(time ${PHOENIX_WC}/${j} ${DATA}/part-00024 10) 1>> $OUTPUTDIR/log.txt 2>> $OUTPUTDIR/timings/timing-${j}.out
				(time ${PHOENIX_WC}/${j} ${DATA}/part-00051 10) 1>> $OUTPUTDIR/log.txt 2>> $OUTPUTDIR/timings/timing-${j}.out
				echo "input0 ${INPUT_SIZE} bytes" >> $OUTPUTDIR/timings/timing-${j}.out
				(time ${PHOENIX_WC}/${j} ${DATA}/input0 10) 1>> $OUTPUTDIR/log.txt 2>> $OUTPUTDIR/timings/timing-${j}.out
				(time ${PHOENIX_WC}/${j} ${DATA}/part-00000 10) 1>> $OUTPUTDIR/log.txt 2>> $OUTPUTDIR/timings/timing-${j}.out
				(time ${PHOENIX_WC}/${j} ${DATA}/part-00014 10) 1>> $OUTPUTDIR/log.txt 2>> $OUTPUTDIR/timings/timing-${j}.out
				(time ${PHOENIX_WC}/${j} ${DATA}/part-00151 10) 1>> $OUTPUTDIR/log.txt 2>> $OUTPUTDIR/timings/timing-${j}.out
				echo "input1 ${INPUT_SIZE} bytes" >> $OUTPUTDIR/timings/timing-${j}.out
				(time ${PHOENIX_WC}/${j} ${DATA}/input1 10) 1>> $OUTPUTDIR/log.txt 2>> $OUTPUTDIR/timings/timing-${j}.out
				(time ${PHOENIX_WC}/${j} ${DATA}/part-00012 10) 1>> $OUTPUTDIR/log.txt 2>> $OUTPUTDIR/timings/timing-${j}.out
				(time ${PHOENIX_WC}/${j} ${DATA}/part-00001 10) 1>> $OUTPUTDIR/log.txt 2>> $OUTPUTDIR/timings/timing-${j}.out
				(time ${PHOENIX_WC}/${j} ${DATA}/part-00129 10) 1>> $OUTPUTDIR/log.txt 2>> $OUTPUTDIR/timings/timing-${j}.out
				echo "input2 ${INPUT_SIZE} bytes" >> $OUTPUTDIR/timings/timing-${j}.out
				(time ${PHOENIX_WC}/${j} ${DATA}/input2 10) 1>> $OUTPUTDIR/log.txt 2>> $OUTPUTDIR/timings/timing-${j}.out
				(time ${PHOENIX_WC}/${j} ${DATA}/part-00142 10) 1>> $OUTPUTDIR/log.txt 2>> $OUTPUTDIR/timings/timing-${j}.out
				(time ${PHOENIX_WC}/${j} ${DATA}/part-00091 10) 1>> $OUTPUTDIR/log.txt 2>> $OUTPUTDIR/timings/timing-${j}.out
				(time ${PHOENIX_WC}/${j} ${DATA}/part-00144 10) 1>> $OUTPUTDIR/log.txt 2>> $OUTPUTDIR/timings/timing-${j}.out
				echo "input3 ${INPUT_SIZE} bytes" >> $OUTPUTDIR/timings/timing-${j}.out
				(time ${PHOENIX_WC}/${j} ${DATA}/input3 10) 1>> $OUTPUTDIR/log.txt 2>> $OUTPUTDIR/timings/timing-${j}.out
				(time ${PHOENIX_WC}/${j} ${DATA}/part-00010 10) 1>> $OUTPUTDIR/log.txt 2>> $OUTPUTDIR/timings/timing-${j}.out
				(time ${PHOENIX_WC}/${j} ${DATA}/part-00041 10) 1>> $OUTPUTDIR/log.txt 2>> $OUTPUTDIR/timings/timing-${j}.out
				(time ${PHOENIX_WC}/${j} ${DATA}/part-00002 10) 1>> $OUTPUTDIR/log.txt 2>> $OUTPUTDIR/timings/timing-${j}.out
				echo "input4 ${INPUT_SIZE} bytes" >> $OUTPUTDIR/timings/timing-${j}.out
				(time ${PHOENIX_WC}/${j} ${DATA}/input4 10) 1>> $OUTPUTDIR/log.txt 2>> $OUTPUTDIR/timings/timing-${j}.out
				(time ${PHOENIX_WC}/${j} ${DATA}/part-00091 10) 1>> $OUTPUTDIR/log.txt 2>> $OUTPUTDIR/timings/timing-${j}.out
				(time ${PHOENIX_WC}/${j} ${DATA}/part-00024 10) 1>> $OUTPUTDIR/log.txt 2>> $OUTPUTDIR/timings/timing-${j}.out
				(time ${PHOENIX_WC}/${j} ${DATA}/part-00051 10) 1>> $OUTPUTDIR/log.txt 2>> $OUTPUTDIR/timings/timing-${j}.out
			fi

			# Cleaning up
			if [ $OPROFILE_ENABLED == 1 ] ; then 
				stop_oprofile
				echo "-- dumping OProfile report..." >> $OUTPUTDIR/log.txt
				opreport -l --image-path=/lib/modules/$(uname -r)/kernel 1>$OUTPUTDIR/symbols/${j}-iteration${i}.out 2>> $OUTPUTDIR/warnings.out
				opreport 1>$OUTPUTDIR/images/${j}-iteration${i}.out 2>> $OUTPUTDIR/warnings.out
			fi
			if [ $MONITORING_ENABLED == 1 ] ; then kill $sar_pid 1>> $OUTPUTDIR/log.txt; fi
		done
	fi
done
echo "" >> $OUTPUTDIR/log.txt
echo "run.sh completed successfully (apparently) and is done!" >> $OUTPUTDIR/log.txt
