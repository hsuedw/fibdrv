set title "Fibonancci driver performance analysis"
set xlabel "Fk"
set ylabel "time (ns)"
set terminal png font " Times_New_Roman,12 "
set output "fibdrv_perf.png"
set xtics 0 ,10 ,92
set key left 

plot \
"it_fib.time" using 1:2 with linespoints linewidth 2 title "iteration", \
"fd_fib.time" using 1:2 with linespoints linewidth 2 title "fast doubling", \
