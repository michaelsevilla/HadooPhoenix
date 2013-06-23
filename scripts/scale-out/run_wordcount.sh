#! /bin/bash
# Script to run wordcount over MapReduce 
#
# Author: Michael Sevilla (msevilla@ucsc.edu)
# Date: 3/17/2012
#
# Script runs wordcount module in the Hadoop example jar

function usage(){
        echo ""
        echo "Usage: ./run.sh <output directory>"
        echo ""
        echo "Example: "
        echo "   run.sh ./deliverables "
	echo "*Ensure that: "
	echo "   - the hadoop directory is ~/programs/hadoop" 
	echo "   - all input data files must already exist (rwriter-input1, rwriter-input2, ...)"
	echo "   - hadoop is running (bin/start_dfs.sh and bin/start_mapred.sh)"
        exit
}
 

if [ $# -lt 1 ] ; then 
        usage 
fi

# Instantiate directory locations
#    - the grammar parsings strips the trailing '/', it it exists
OUTPUTDIR=`echo "${1}" | sed -e "s/\/*$//"`
#HADOOP_DIR=/users/msevilla/programs/hadoop
HADOOP_DIR=/mnt/vol1/msevilla/hadoop_src
DATA=/user/msevilla
CHUNK=1073741824

# Print job paramaters to a file
echo "Workload: word_count" > $OUTPUTDIR/log.txt
echo "Date: " `date` >> $OUTPUTDIR/log.txt
echo "Output directory: $OUTPUTDIR" >> $OUTPUTDIR/log.txt
echo "Program directory: $PHOENIX_WC" >> $OUTPUTDIR/log.txt
echo "Data directory: $DATA" >> $OUTPUTDIR/log.txt
echo "Chunk size: $CHUNK bytes" >> $OUTPUTDIR/log.txt
echo "--------------------------------------------------" >> $OUTPUTDIR/log.txt
echo "" >> $OUTPUTDIR/log.txt

# Construct directories
if [ -e ${OUTPUTDIR}/symbols ] ; then rm -r ${OUTPUTDIR}/symbols; echo "-- deleted ${OUTPUTDIR}/symbols" >> $OUTPUTDIR/log.txt; fi
if [ -e ${OUTPUTDIR}/images ] ; then rm -r ${OUTPUTDIR}/images; echo "-- deleted ${OUTPUTDIR}/images" >> $OUTPUTDIR/log.txt; fi
if [ -e ${OUTPUTDIR}/timings ] ; then rm -r ${OUTPUTDIR}/timings; echo "-- deleted ${OUTPUTDIR}/timings" >> $OUTPUTDIR/log.txt; fi
mkdir ${OUTPUTDIR}/images
echo "-- created ${OUTPUTDIR}/images" >> $OUTPUTDIR/log.txt
mkdir ${OUTPUTDIR}/symbols
echo "-- created ${OUTPUTDIR}/symbols" >> $OUTPUTDIR/log.txt
echo "" >> $OUTPUTDIR/log.txt
mkdir ${OUTPUTDIR}/timings
echo "-- created ${OUTPUTDIR}/timings" >> $OUTPUTDIR/log.txt
echo "" >> $OUTPUTDIR/log.txt

# Delete the input
${HADOOP_DIR}/bin/hadoop dfs -rmr ${DATA}/randomtextwriter-test
${HADOOP_DIR}/bin/hadoop dfs -mkdir ${DATA}/randomtextwriter-test

j=wordcount
#for i in {240..300}
for i in {1..300}
do
        INPUT_SIZE=$((${CHUNK}*${i}))
	NFILE=$((${i}-1))

	# Copy the data to the test directory
	if [ ${NFILE} -lt 10 ] ; then ${HADOOP_DIR}/bin/hadoop dfs -cp ${DATA}/randomtextwriter-input/part-0000${NFILE} ${DATA}/randomtextwriter-test 2>> ${OUTPUTDIR}/warnings;
	elif [ ${NFILE} -lt 100 ] ; then ${HADOOP_DIR}/bin/hadoop dfs -cp ${DATA}/randomtextwriter-input/part-000${NFILE} ${DATA}/randomtextwriter-test 2>> ${OUTPUTDIR}/warnings;
	else ${HADOOP_DIR}/bin/hadoop dfs -cp ${DATA}/randomtextwriter-input/part-00${NFILE} /user/msevilla/randomtextwriter-test 1>> ${OUTPUTDIR}/warnings; fi

	if [ $((${i}%20)) -eq 0 ] ; then
	#if [ 1 -eq 1 ] ; then
		${HADOOP_DIR}/bin/hadoop dfs -rmr ${DATA}/randomtextwriter-output
		
		echo "--------------------------------------------------" >> $OUTPUTDIR/log.txt
		echo "-- ${j} iteration $i ($CHUNK x $i = $INPUT_SIZE)" >> $OUTPUTDIR/log.txt
		echo "--------------------------------------------------" >> $OUTPUTDIR/log.txt
		echo "--------------------------------------------------" >> $OUTPUTDIR/warnings.out
		echo "-- ${j}iteration $i ($CHUNK x $i = $INPUT_SIZE)" >> $OUTPUTDIR/warnings.out
		echo "--------------------------------------------------" >> $OUTPUTDIR/warnings.out
		echo "Input size: " `${HADOOP_DIR}/bin/hadoop dfs -ls ${DATA}/randomtextwriter-test` >> $OUTPUTDIR/log.txt
	
		echo "-- running benchmark..." >> $OUTPUTDIR/log.txt
		echo "" >> $OUTPUTDIR/timings/timing-${j}.out
		echo "Iteration ${i} (${INPUT_SIZE} bytes)" >> $OUTPUTDIR/timings/timing-${j}.out
		(time ${HADOOP_DIR}/bin/hadoop jar ${HADOOP_DIR}/hadoop-examples-1.0.4.jar wordcount \
			${DATA}/randomtextwriter-test \
			${DATA}/randomtextwriter-output \
		) 1>> $OUTPUTDIR/log.txt 2>> $OUTPUTDIR/timings/timing-${j}.out
		echo "" >> $OUTPUTDIR/timings/timing-${j}.out
	fi
done

echo "" >> $OUTPUTDIR/log.txt
echo "run.sh completed successfully (apparently) and is done!" >> $OUTPUTDIR/log.txt                
