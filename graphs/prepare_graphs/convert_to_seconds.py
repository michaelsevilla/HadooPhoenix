#!/usr/bin/python
import sys

if (len(sys.argv) < 3): 
	print "Author: Michael Sevilla (msevilla@ucsc.edu)"
	print "Date: 1-26-2013"
	print "convert_to_seconds: program that converts all minute values to seconds"
	print "\tusage: ./convert_to_seconds <timing file> <column>"
	print "\tex: ./convert_to_seconds timing 1"
	sys.exit(2)

timing = sys.argv[1]
timing_file = open(timing, 'r')
column = sys.argv[2]
iteration = 1

unit = 1073741824			# GB

for line in timing_file: 
	words = line.split()
	temp = words[int(column)].split("m")
	minutes = temp[0]
	temp = temp[1].split("s")
	seconds = temp[0]
	total_seconds = float(minutes)*60 + float(seconds)
	#print words[0] + " " + str(total_seconds) + " " + str(iteration)
	print str(float(words[0])/float(unit)) + " " + str(total_seconds) + " " + str(iteration)
	iteration = iteration + 1

timing_file.close()
