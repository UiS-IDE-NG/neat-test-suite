set terminal pdf
set output ARG5

if (v_title ne "*") {
    set title v_title
}

set xlabel v_xlabel font ", 20"
set ylabel v_ylabel font ", 20" offset -1
set key font ", 16"
set xrange [0:6]
#set yrange [*:*]
set tics font ", 20"

if (v_ymax eq "*") {
    set yrange [*:*]
} else {
    set yrange [v_ymin:v_ymax]
}

set logscale y 10

set style line 12 lt 1 dt 2 lw 1 lc rgb "#bbbbbb"
set style line 13 lt 1 dt 2 lw 1 lc rgb "#333333"

#set style line 12 lc rgb 'blue' lt 1 lw 1
#set mytics 10
set grid ytics ls 13
#set grid
set boxwidth 0.8
set bars 4.0
set style fill empty

plot ARG1 using 1:($3 * ARG6):($2 * ARG6):($6 * ARG6):($5 * ARG6):xticlabels(7) with candlesticks lt rgb "#ff0000" fs solid 0.5 border lc rgb "#000066" notitle whiskerbars,\
     '' using 1:($4 * ARG6):($4 * ARG6):($4 * ARG6):($4 * ARG6) with candlesticks lt -1 notitle,\
     ARG2 using 1:($3 * ARG6):($2 * ARG6):($6 * ARG6):($5 * ARG6):xticlabels(7) with candlesticks lt rgb "#00ff00" fs solid 0.5 border lc rgb "#000066" notitle whiskerbars,\
     '' using 1:($4 * ARG6):($4 * ARG6):($4 * ARG6):($4 * ARG6) with candlesticks lt -1 notitle,\
     ARG3 using 1:($3 * ARG6):($2 * ARG6):($6 * ARG6):($5 * ARG6):xticlabels(7) with candlesticks lt rgb "#0000ff" fs solid 0.5 border lc rgb "#000066" notitle whiskerbars,\
     '' using 1:($4 * ARG6):($4 * ARG6):($4 * ARG6):($4 * ARG6) with candlesticks lt -1 notitle,\
     ARG4 using 1:($3 * ARG6):($2 * ARG6):($6 * ARG6):($5 * ARG6):xticlabels(7) with candlesticks lt rgb "#008000" fs solid 0.5 border lc rgb "#000066" notitle whiskerbars,\
     '' using 1:($4 * ARG6):($4 * ARG6):($4 * ARG6):($4 * ARG6) with candlesticks lt -1 notitle,\
     ARG1 using 1:($4 * ARG6) with lines lt rgb "#ff0000" lw 5 title "1 flow",\
     ARG2 using 1:($4 * ARG6) with lines lt rgb "#00ff00" lw 5 title "2 flows",\
     ARG3 using 1:($4 * ARG6) with lines lt rgb "#0000ff" lw 5 title "4 flows",\
     ARG4 using 1:($4 * ARG6) with lines lt rgb "#008000" lw 5 title "8 flows"