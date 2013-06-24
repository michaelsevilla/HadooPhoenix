#!/usr/bin/python
import sys

if (len(sys.argv) < 3): 
	print "Author: Michael Sevilla (msevilla@ucsc.edu)"
	print "Date: 6-1-2013"
	print "normalized_to: program that normalizes one set of inputs to another"
	print "\tusage: ./normalized_to <toNormalize> <base>"
	print "\tex: ./parse_columns wc-hadoop.timing wc-parallel.timing"
	sys.exit(2)


file1 = sys.argv[1]
file2 = sys.argv[2]
file3 = sys.argv[3]

f1 = open(file1, 'r')
f1Lines = f1.readlines()
f2 = open(file2, 'r')
f2Lines = f2.readlines()
f3 = open(file3, 'r')
f3Lines = f3.readlines()


for i, f1Line in enumerate(f1Lines): 
	values = []
	f1Words = f1Line.split()
	f2Words = f2Lines[i].split()
	f3Words = f3Lines[i].split()

	if (f1Words[0] != f2Words[0] or f1Words[0] != f3Words[0]):
		print "Step values are different, exiting..."
		sys.exit(2)

	values.append(float(f1Words[1]));	
	values.append(float(f2Words[1]));	
	values.append(float(f3Words[1]));	
	values.sort()

	mean = (values[0] + values[1] + values[2]) / 3
	# Uncomment for candlesticks
	#print str(f1Words[0]) + " " + str(mean) + " " + str(values[0]) + " " + str(values[2]) + " " + str(mean)
	# Uncomment for mean
	print str(f1Words[0]) + " " + str(mean) 


f1.close()
f2.close()
f3.close()
