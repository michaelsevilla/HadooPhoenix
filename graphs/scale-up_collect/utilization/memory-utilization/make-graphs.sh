#! /bin/bash
# Author: Michael Sevilla (msevilla@ucsc.edu)
# Date: 4/2/2013

set -e

for i in {1..300}
do
	if [ $((${i}%20)) -eq 0 ] ; then
		sed "s/iteration=0/iteration=$i/g" timing_setup.gnu > timing_setup-temp.gnu
	
		echo "Making graph for iteration ${i}..."	
		gnuplot timing_setup-temp.gnu
	fi
done
