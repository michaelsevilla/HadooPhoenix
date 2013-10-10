#!/bin/bash

# Program locations
ITERATIONS=181

# Directories
HDFS_DIR=/hdfs_data/randomtextwriter
DATA_DIR=/data1/data
OUTPUT_DIR=./results3


run_program() {
	echo "Running timing benchmark for $ITERATIONS GB"				> $LOG_OUT
	echo ""										> $LOG_OUT
	echo "Running timing benchmark for $ITERATIONS GB"				> $TIMING_OUT
	echo ""										> $TIMING_OUT
	echo "Running timing benchmark for $ITERATIONS GB"
	
	for i in {1..3}
	do
		SIZE=$((${ITERATIONS}*1181116006))
		echo "running: $SIZE bytes"						
		echo "$SIZE bytes"							>> $LOG_OUT
		echo ""									>> $LOG_OUT
		echo "$SIZE bytes"							>> $TIMING_OUT
		echo ""									>> $TIMING_OUT
	
		echo "clearing caches"
		echo 3 > /proc/sys/vm/drop_caches
		if [ ${USE_HDFS} -eq 1 ]; then
			echo "(time $PROGRAM -q $ITERATIONS $HDFS_DIR)"			>> $LOG_OUT
			(time $PROGRAM -q $ITERATIONS $HDFS_DIR)			2>> $TIMING_OUT 1>> $LOG_OUT
			echo ""								>> $TIMING_OUT
		else
			echo "(time $PROGRAM -d $ITERATIONS $DATA_DIR)"			>> $LOG_OUT
			(time $PROGRAM -d $ITERATIONS $DATA_DIR) 			2>> $TIMING_OUT 1>> $LOG_OUT
			echo ""								>> $TIMING_OUT
		fi
	done
}

set -e

## Original Phoenix; HDFS
PROGRAM=/home/msevilla/Scratch/phoenix++-1.0/tests/word_count/word_count
TIMING_OUT=$OUTPUT_DIR/timing-${ITERATIONS}GB-orig_hdfs
LOG_OUT=$OUTPUT_DIR/log-${ITERATIONS}GB-orig_hdfs
USE_HDFS=1

run_program

# Stream Phoenix; HDFS
PROGRAM=./word_count
TIMING_OUT=$OUTPUT_DIR/timing-${ITERATIONS}GB-stream_hdfs
LOG_OUT=$OUTPUT_DIR/log-${ITERATIONS}GB-stream_hdfs
USE_HDFS=1

run_program

# Original Phoenix; no HDFS
#kPROGRAM=/home/msevilla/Scratch/phoenix++-1.0/tests/word_count/word_count
#kTIMING_OUT=$OUTPUT_DIR/timing-${ITERATIONS}GB-orig
#kLOG_OUT=$OUTPUT_DIR/log-${ITERATIONS}GB-orig
#kUSE_HDFS=1
#k
#krun_program
#k
#k# Stream Phoenix; no HDFS
#kPROGRAM=./word_count
#kTIMING_OUT=$OUTPUT_DIR/timing-${ITERATIONS}GB-stream
#kLOG_OUT=$OUTPUT_DIR/log-${ITERATIONS}GB-stream
#kUSE_HDFS=0
#k
#krun_program
