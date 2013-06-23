#!/usr/bin/python
import sys

if (len(sys.argv) < 2): 
	print "Author: Michael Sevilla (msevilla@ucsc.edu)"
	print "Date: 1-26-2013"
	print "parse_columns: program that filters out the ith column of an input file"
	print "\tusage: ./parse_columns <image>"
	print "\tex: ./parse_columns timing"
	sys.exit(2)


image = sys.argv[1]
# Unit of measurement of hte x axis
#unit = 1048576				# 1MB
unit = 1073741824			# GB
image_file = open(image, 'r')

for line in image_file: 
	words = line.split()
	if (words[0] == "terminate"):
		break
	# Uncomment for single-node
	elif (words[1] == "bytes"):
		size = float(words[0])
	elif (words[0] == "real"):
		print str(size) + " " + words[1]
	elif (words[0] == "sys"):
		print str(size) + " " + words[1]
	elif (words[0] == "user"):
		print str(size) + " " + words[1]
	# Uncomment for hadoop
	#elif (words[0] == "Iteration"):
	#	strsize = words[2].split("(")
	#	size = float(strsize[1]) 
image_file.close()
