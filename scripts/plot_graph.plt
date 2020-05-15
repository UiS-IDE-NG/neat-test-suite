set terminal pdf
set output ARG2

set xlabel "Number of flows" font ", 15"
set xrange [0:10]
if (ARG3 == 1) {
	set ylabel "Memory usage (kb)" font ", 15"
} else {
	set ylabel "CPU time (ms)" font ", 15"
}
set yrange[*:*]
set tics font ", 15"

set style line 12 lt 1 dt 2 lw 1 lc rgb "#bbbbbb"
set style line 13 lt 1 dt 2 lw 1 lc rgb "#333333"
#set mytics 10
set grid ytics ls 13
#set grid
set boxwidth 0.8
set bars 4.0
set style fill empty

plot ARG1 using 1:($3 * ARG3):($2 * ARG3):($6 * ARG3):($5 * ARG3):xticlabels(7) with candlesticks lt rgb "#550077" fs solid 0.5 border lc rgb "#000066" notitle whiskerbars,\
     '' using 1:($4 * ARG3):($4 * ARG3):($4 * ARG3):($4 * ARG3) with candlesticks lt -1 notitle'

