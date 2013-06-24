.Graphs README
============

Author: Michael Sevilla  
Date: 6-22-2013  
Institution: UC Santa Cruz  
Email: msevilla@ucsc.edu  

These scripts collect, format, and convert system activity report (SAR), timing, and oprofile output into graph input.

Files
- ./README.md:            this file  
- ./prepare_gnuplot.sh

/prepare_gnuplot.sh ~/Results/word_count/dexter/nockpt/run1/timings word_count
mv word_count.timing data_parallel/wc-parallel_v0.timing 
./prepare_gnuplot.sh ~/Results/word_count/dexter/nockpt/run2/timings word_count
mv word_count.timing data_parallel/wc-parallel_v1.timing 
./prepare_gnuplot.sh ~/Results/word_count/dexter/nockpt/run3/timings word_count
mv word_count.timing data_parallel/wc-parallel_v2.timing 
cd data_parallel/
vim wc-parallel_v0.timing 
vim wc-parallel_v1.timing 
vim wc-parallel_v2.timing 
vim ../create_candlesticks.py 
../create_candlesticks.py wc-parallel_v0.timing wc-parallel_v1.timing wc-parallel_v2.timing > wc-parallel_mean.timing
vim ../create_candlesticks.py 
../create_candlesticks.py wc-parallel_v0.timing wc-parallel_v1.timing wc-parallel_v2.timing > wc-parallel_maxmin.timing
cd ..
vim timing_candle.gnu 
gnuplot timing_candle.gnu 

