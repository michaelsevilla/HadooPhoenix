set xlabel "Input size (GB)"
set ylabel "Wall clock time (seconds)"
set style data linespoints
set pointsize 2
set style line 1 lt 1 lw 2.5 pt 1 ps 1 lc rgb "red"
set style line 2 lt 1 lw 5 pt 9 ps 2 lc rgb "green"
set style line 3 lt 1 lw 2.5 pt 7 ps 1 lc rgb "blue"
set style line 4 lt 4 lw 2.5 pt 4 ps 1 lc rgb "black"
set style line 5 lt 4 lw 2.5 pt 5 ps 1 lc rgb "magenta"
set style line 6 lt 5 lw 2.5 pt 6 ps 1 lc rgb "purple"


#set yrange[0:17]
#set xrange[0:3]
set yrange[0:6000]
set xrange[0:300]


set title "Word Count execution times"
#set title "Word Count over RandomTextWriter data"
set term postscript enhanced color "Helvetica,22"
set key top right font "Helvetica,14"

set output "./images/wordcount_initial.ps"
plot \
"wc-parallel.timing" ls 3 title "Parallelized", \




"wc-seq.timing" ls 2 title "Sequential (scale\-up)", \
"wc-hadoop_v0.timing" ls 1 title "Hadoop (scale\-out)", \
"wc-parallel.timing" ls 3 title "Parallelized (scale\-up)"
#"wc-seq.timing" ls 2 title "Sequential", \
#"wc-parallel.timing" ls 3 title "Parallelism", \
#"wc-dmtcp.timing" ls 4 title "Fault Tolerance", \
#"wc-storage.timing" ls 5 title "Storage", \
#"wc-hadoop_v0.timing" ls 1 title "Hadoop (scale\-out)"
