OUTPUTDIR=`echo "${1}" | sed -e "s/\/*$//"`
VMLINUX=/usr/lib/debug/boot/vmlinux-3.2.0-33-generic
PHOENIX_WC=/home/msevilla/Programs/Phoenix++/tests/word_count
DATA=/data1/Data/randomwriter-data
CHUNK=1073741824

DMTCP=/home/msevilla/Programs/DMTCP
CKPTDIR=/data1/Checkpoints/run1

# Print job paramaters to a file
echo "" 
echo "" 
echo "Workload: word_count"
echo "Date: " `date` 
echo "Output directory: $OUTPUTDIR" 
echo "Program directory: $PHOENIX_WC"
echo "Data directory: $DATA"
echo "Checkpoint directory: ${CKPTDIR}" 
echo "Chunk size: $CHUNK bytes"
echo "--------------------------------------------------" 
echo "" 

if [ 1 == 1 ] ; then
	echo "Inside if statement"
fi


# Fork off the DMTCP listener
#${DMTCP}/bin/dmtcp_coordinator --interval 1 2> warnings &
#dmtcpcoord_pid=$!;

# Start the job
(time \
${DMTCP}/bin/dmtcp_checkpoint \
--quiet \
--no-gzip \
--interval 2 \
--ckptdir ${CKPTDIR} \
${PHOENIX_WC}/word_count ${DATA}/test) 2>> timing.out

#kill ${dmtcpcoord_pid}

