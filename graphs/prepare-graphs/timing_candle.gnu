set xlabel "Input size (GB)" font "Helvetica,24"
set ylabel "Wall clock time (seconds)" font "Helvetica,24"
set style data linespoints
set pointsize 2
set style line 1 lt 1 lw 2.5 pt 1 ps 1 lc rgb "red"
set style line 2 lt 1 lw 5 pt 9 ps 2 lc rgb "green"
set style line 22 lt 1 lw 2.5 pt 9 ps 2 lc rgb "green"
set style line 3 lt 1 lw 2.5 pt 7 ps 1 lc rgb "blue"
set style line 4 lt 4 lw 2.5 pt 4 ps 1 lc rgb "black"
set style line 44 lt 1 lw 2.5 pt 4 ps 0.5 lc rgb "black"
set style line 5 lt 4 lw 2.5 pt 5 ps 1 lc rgb "magenta"
set style line 6 lt 3 lw 5 pt 6 ps 0 lc rgb "purple"
set style line 66 lt 1 lw 5 pt 6 ps 0 lc rgb "purple"

set yrange[0:8000]
#set yrange[0:4500]
set xrange[0:256]


set title "Xen checkpointing times"
#set title "Word Count over RandomTextWriter data"
set term postscript enhanced color "Helvetica,22"
set key top right font "Helvetica,24"

set output "./images/wordcount_test.ps"
plot \
"./data_hadoop/wc-hadoop_maxmin.timing" with candlesticks ls 1 notitle whiskerbars, \
"./data_hadoop/wc-hadoop_mean.timing" ls 1 title "Hadoop", \
"./data_seq/wc-seq_maxmin.timing" with candlesticks ls 22 notitle whiskerbars, \
"./data_seq/wc-seq_mean.timing" ls 2 title "Sequential", \
"./data_parallel/wc-parallel_maxmin.timing" with candlesticks ls 3 notitle whiskerbars, \
"./data_parallel/wc-parallel_mean.timing" ls 3 title "Parallelized", \
"./data_dmtcp/wc-dmtcp_maxmin.timing" with candlesticks ls 44 notitle whiskerbars, \
"./data_dmtcp/wc-dmtcp_mean.timing" ls 4 title "Fault Tolerant", \
"./data_storage/wc-storage.timing" ls 5 title "Storage"

#"./data_xen/wc-xen.timing" ls 6 title "Xen, 12GB", \
#"./data_xen/wc-xen_big.timing" ls 66 title "Xen, 256GB", \
#"./data_parallel/wc-parallel_maxmin.timing" with candlesticks ls 3 notitle whiskerbars, \
#"./data_parallel/wc-parallel_mean.timing" ls 3 title "Parallel word count", \
#"./data_hadoop/wc-hadoop_maxmin.timing" with candlesticks ls 1 notitle whiskerbars, \
#"./data_hadoop/wc-hadoop_mean.timing" ls 1 title "Hadoop word count"


#"./data_hadoop/wc-hadoop_maxmin.timing" with candlesticks ls 1 notitle whiskerbars, \
#"./data_hadoop/wc-hadoop_mean.timing" ls 1 title "scale-out (Hadoop)", \
#"./data_seq/wc-seq_maxmin.timing" with candlesticks ls 22 notitle whiskerbars, \
#"./data_seq/wc-seq_mean.timing" ls 2 title "scale-up (sequential)", \
#"./data_parallel/wc-parallel_maxmin.timing" with candlesticks ls 3 notitle whiskerbars, \
#"./data_parallel/wc-parallel_mean.timing" ls 3 title "scale-up (parallelized)"
