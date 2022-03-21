set title "Fibonancci driver performance analysis"
set xlabel "Fk"
set ylabel "time (ns)"
set terminal png font " Times_New_Roman,12 "
set output "fibdrv_perf2.png"
set xtics 0 ,5000 ,23000
set key below
set key under

plot \
"it_fib.time" using 1:2 with linespoints linewidth 2 title "iteration", \
"fd_fib.time" using 1:2 with linespoints linewidth 2 title "fast doubling", \
"fd_clz_fib.time" using 1:2 with linespoints linewidth 2 title "fast doubling with clz", \
