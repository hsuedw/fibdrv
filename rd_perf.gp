set title "read performance analysis"
set xlabel "Fk"
set ylabel "time (ns)"
set terminal png font " Times_New_Roman,12 "
set output "read_perf.png"
set xtics 0 ,5000 ,23000
set key below
set key under

plot \
"it_rd.time" using 1:2 with linespoints linewidth 2 title "iteration", \
"fd_rd.time" using 1:2 with linespoints linewidth 2 title "fast doubling", \
"fd_clz_rd.time" using 1:2 with linespoints linewidth 2 title "fast doubling", \
