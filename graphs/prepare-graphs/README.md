prepare-graphss README
============

Author: Michael Sevilla  
Date: 6-22-2013  
Institution: UC Santa Cruz  
Email: msevilla@ucsc.edu  

These scripts collect, format, and convert system activity report (SAR), timing, and oprofile output into graph input.

Files
- ./README.md:              this file  
- ./prepare_gnuplot.sh:     (call directly) extracts relevant output lines and calls parse_columns.py and convert_to_seconds.py
- ./parse_columns.py:       parses prepare_gnuplot.sh output and puts the data into columns
- ./convert_to_seconds.py:  parses parse_column.py output and changes minutes to seconds
- ./create_candlesticks.py: finds the mean and max/min values from the prepare_gnuplot.sh output for 3 different runs
- ./timing_candle.gnu:      gnuplot configuration file for create_candlesticks.py output
- ./timing_setup.gnu:       gnuplot configuration file for prepare_gnuplot.sh output

Example usage
$ /prepare_gnuplot.sh ~/Results/word_count/dexter/nockpt/run1/timings word_count
$ mv word_count.timing data_parallel/wc-parallel_v0.timing 
$ ./prepare_gnuplot.sh ~/Results/word_count/dexter/nockpt/run2/timings word_count
$ mv word_count.timing data_parallel/wc-parallel_v1.timing 
$ ./prepare_gnuplot.sh ~/Results/word_count/dexter/nockpt/run3/timings word_count
$ mv word_count.timing data_parallel/wc-parallel_v2.timing 
$ ../create_candlesticks.py wc-parallel_v0.timing wc-parallel_v1.timing wc-parallel_v2.timing > wc-parallel_mean.timing
$ vim ../create_candlesticks.py 
$ ../create_candlesticks.py wc-parallel_v0.timing wc-parallel_v1.timing wc-parallel_v2.timing > wc-parallel_maxmin.timing
$ gnuplot timing_candle.gnu 

End file
