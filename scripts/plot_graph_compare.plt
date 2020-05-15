set terminal pdf
set output ARG2

set xlabel "Number of flows" font ", 15" 
set xrange [0:10]
if (ARG3 == 1) {
	set ylabel "Memory usage (kb)" font ", 15" 
} else {
	set ylabel "CPU time (ms)" font ", 15" 
}

if (ARG5 == 1) {
	title1 = "before"
	title2 = "after"
} else {
	title1 = "connection"
	title2 = "json"
}
set yrange [*:*]
set tics font ", 15"

set style line 12 lt 1 dt 2 lw 1 lc rgb "#bbbbbb"
set style line 13 lt 1 dt 2 lw 1 lc rgb "#333333"
#set mytics 10
set grid ytics ls 13
#set grid
set boxwidth 0.8
set bars 4.0
set style fill empty

set key left

plot ARG1 using 1:($3 * ARG3):($2 * ARG3):($6 * ARG3):($5 * ARG3):xticlabels(7) with candlesticks lt rgb "#ff0000" fs solid 0.5 border lc rgb "#000066" title title1 whiskerbars, \
	'' using 1:($4 * ARG3):($4 * ARG3):($4 * ARG3):($4 * ARG3) with candlesticks lt -1 notitle', \
	#ARG4 using 1:($3 * ARG3):($2 * ARG3):($6 * ARG3):($5 * ARG3):xticlabels(7) with candlesticks lt rgb "#00ff00" fs solid 0.5 border lc rgb "#000066" title "after" whiskerbars, \
	#'' using 1:($4 * ARG3):($4 * ARG3):($4 * ARG3):($4 * ARG3) with candlesticks lt -1 notitle' , \
	ARG4 using 1:($3 * ARG3):($2 * ARG3):($6 * ARG3):($5 * ARG3):xticlabels(7) with candlesticks lt rgb "#900000ff" fs solid 0.5 border lc rgb "#0000ff" title title2 whiskerbars, \
	'' using 1:($4 * ARG3):($4 * ARG3):($4 * ARG3):($4 * ARG3) with candlesticks lt -1 notitle'
