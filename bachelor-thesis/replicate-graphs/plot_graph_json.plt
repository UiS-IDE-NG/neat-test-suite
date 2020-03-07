set terminal pdf
set output ARG2

#set title "... TCP on client side"

set xlabel "Number of flows" font ", 20" offset -1
set xrange [0:10]
if (ARG3 == 1) {
	set ylabel "Memory usage (kb)" font ", 20" offset -1
	#set yrange [0:13000]
} else {
	set ylabel "CPU time (ms)" font ", 20" offset -1
	#set yrange [0:180]
}
set yrange [*:*]
set tics font ", 20"

set style line 12 lt 1 dt 2 lw 1 lc rgb "#bbbbbb"
set style line 13 lt 1 dt 2 lw 1 lc rgb "#333333"
#set style line 12 lc rgb 'blue' lt 1 lw 1
#set mytics 10
set grid ytics ls 13
#set grid
set boxwidth 0.8
set bars 4.0
set style fill empty

plot ARG1 using 1:($3 * ARG3):($2 * ARG3):($6 * ARG3):($5 * ARG3):xticlabels(7) with candlesticks lt rgb "#ff0000" fs solid 0.5 border lc rgb "#000066" notitle whiskerbars,\
     '' using 1:($4 * ARG3):($4 * ARG3):($4 * ARG3):($4 * ARG3) with candlesticks lt -1 notitle',\
	ARG4 using 1:($3 * ARG3):($2 * ARG3):($6 * ARG3):($5 * ARG3):xticlabels(7) with candlesticks lt rgb "#00ff00" fs solid 0.5 border lc rgb "#000066" notitle whiskerbars,\
     '' using 1:($4 * ARG3):($4 * ARG3):($4 * ARG3):($4 * ARG3) with candlesticks lt -1 notitle',\
	ARG5 using 1:($3 * ARG3):($2 * ARG3):($6 * ARG3):($5 * ARG3):xticlabels(7) with candlesticks lt rgb "#0000ff" fs solid 0.5 border lc rgb "#000066" notitle whiskerbars,\
     '' using 1:($4 * ARG3):($4 * ARG3):($4 * ARG3):($4 * ARG3) with candlesticks lt -1 notitle',\
	ARG6 using 1:($3 * ARG3):($2 * ARG3):($6 * ARG3):($5 * ARG3):xticlabels(7) with candlesticks lt rgb "#000000" fs solid 0.5 border lc rgb "#000066" notitle whiskerbars,\
     '' using 1:($4 * ARG3):($4 * ARG3):($4 * ARG3):($4 * ARG3) with candlesticks lt -1 notitle',\
	ARG7 using 1:($3 * ARG3):($2 * ARG3):($6 * ARG3):($5 * ARG3):xticlabels(7) with candlesticks lt rgb "#ffffff" fs solid 0.5 border lc rgb "#000066" notitle whiskerbars,\
     '' using 1:($4 * ARG3):($4 * ARG3):($4 * ARG3):($4 * ARG3) with candlesticks lt -1 notitle',\
	ARG8 using 1:($3 * ARG3):($2 * ARG3):($6 * ARG3):($5 * ARG3):xticlabels(7) with candlesticks lt rgb "#ffff00" fs solid 0.5 border lc rgb "#000066" notitle whiskerbars,\
     '' using 1:($4 * ARG3):($4 * ARG3):($4 * ARG3):($4 * ARG3) with candlesticks lt -1 notitle',\
	ARG7 using 1:($3 * ARG3):($2 * ARG3):($6 * ARG3):($5 * ARG3):xticlabels(7) with candlesticks lt rgb "#ff00ff" fs solid 0.5 border lc rgb "#000066" notitle whiskerbars,\
     '' using 1:($4 * ARG3):($4 * ARG3):($4 * ARG3):($4 * ARG3) with candlesticks lt -1 notitle',\
	ARG8 using 1:($3 * ARG3):($2 * ARG3):($6 * ARG3):($5 * ARG3):xticlabels(7) with candlesticks lt rgb "#00ffff" fs solid 0.5 border lc rgb "#000066" notitle whiskerbars,\
     '' using 1:($4 * ARG3):($4 * ARG3):($4 * ARG3):($4 * ARG3) with candlesticks lt -1 notitle',\
	ARG9 using 1:($3 * ARG3):($2 * ARG3):($6 * ARG3):($5 * ARG3):xticlabels(7) with candlesticks lt rgb "#8000ff" fs solid 0.5 border lc rgb "#000066" notitle whiskerbars,\
     '' using 1:($4 * ARG3):($4 * ARG3):($4 * ARG3):($4 * ARG3) with candlesticks lt -1 notitle',\
	ARG10 using 1:($3 * ARG3):($2 * ARG3):($6 * ARG3):($5 * ARG3):xticlabels(7) with candlesticks lt rgb "#ff8000" fs solid 0.5 border lc rgb "#000066" notitle whiskerbars,\
     '' using 1:($4 * ARG3):($4 * ARG3):($4 * ARG3):($4 * ARG3) with candlesticks lt -1 notitle',\
	ARG1 using 1:($4 * ARG3) with lines lt rgb "#ff0000" lw 5 title "connection",\
	ARG4 using 1:($4 * ARG3) with lines lt rgb "#00ff00" lw 5 title "json dumps",\
	ARG5 using 1:($4 * ARG3) with lines lt rgb "#0000ff" lw 5 title "json pack",\
	ARG6 using 1:($4 * ARG3) with lines lt rgb "#000000" lw 5 title "json loads",\
	ARG7 using 1:($4 * ARG3) with lines lt rgb "#ffffff" lw 5 title "calloc",\
	ARG8 using 1:($4 * ARG3) with lines lt rgb "#ffff00" lw 5 title "get name info",\
	ARG7 using 1:($4 * ARG3) with lines lt rgb "#ff00ff" lw 5 title "json decref",\
	ARG8 using 1:($4 * ARG3) with lines lt rgb "#00ffff" lw 5 title "json object set",\
	ARG9 using 1:($4 * ARG3) with lines lt rgb "#8000ff" lw 5 title "json copy",\
	ARG10 using 1:($4 * ARG3) with lines lt rgb "#ff8000" lw 5 title "json object get"



