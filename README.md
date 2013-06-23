HadooPhoenix
============

Author: Michael Sevilla  
Date: 6-22-2013  
Institution: UC Santa Cruz  
Email: msevilla@ucsc.edu  
README

HadooPhoneix is a series of programs that implement scale-out algorithms
on a scale-up system. It also includes sequential, 'dumbest thing possible'
implementations to ensure that we compare scaling architectures, not the
applications themselves. We implement Hadoop algorithms using the Phoenix++
API/runtime.

Files
- ./README.md: 	this file  
- ./phoenix++:	scale-out programs on scale-up  
	- edited files: sort, wordcount, kmeans  
- ./scripts:	scripts for running scalability tests  
- ./graphs: 	scripts that prepare output for gnuplot   
- ./dmtcp:	tool for achieving fault tolerance with checkpointing 

Edited on ike
