HadooPhoenix README
============

Author: Michael Sevilla  
Date: 6-22-2013  
Institution: UC Santa Cruz  
Email: msevilla@ucsc.edu  
  
HadooPhoenix is a collection of tools that implement scale-out algorithms on a scale-up system. The goal of this project is to compare the two most popular scaling architectures. The scale-out algorithms are modeled off the Hadoop implementatations and are implemented using Phoenix++. HadooPhoenix also includes sequential, 'dumbest thing possible' implementations. Using these two techniques, we cover a wide range of implementations, ensuring that we compare scaling architectures, not the applications themselves. A better introduction and references for all the tools and techniques we used can be found in the msevilla_masters_v0.pdf document in the ./docs folder of this distribution.
	
Files  
*** Note - 'sop' stands for 'scale-out property' ***
- ./README.md:            this file  
- ./docs:                 supporting documentation with motivation, results, and references
- ./graphs:               scripts that prepare output for gnuplot   
- ./scripts:              scripts for running scalability tests  
- ./sop1_parallelism:     uses Phoenix++ to achieve scale-out parallelism on scale-up 
- ./sop2_fault-tolerance: uses DMTCP to achieve fault tolerance with checkpointing
- ./sop3_portability:     library and shell script that queries hardware for optimal configurations
- ./sop4_storage:         script that benchmarks the HDFS transfer speed (-copyToLocal)

We have reproduced the Phoenix++, DMTCP, and Phoenix2 source code so that the benchmarks for sop1_parallelism, sop2_fault-tolerance, sop3_portability, respectively, can be run as is. 

End file
