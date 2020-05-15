#!/bin/bash

MEMORY_DIR=$1		
CPU_DIR=$2		
CONNECTION_DIR=$3		
MEMORY_DIR_OPT=$4		
CPU_DIR_OPT=$5	
CONNECTION_DIR_OPT=$6		
OUTPUT_DIRECTORY=$7
SCRIPT_DIRECTORY="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

type=("cpu_application" "memory_application")
when=("start" "afterallconnected")  

# $1: what directory
# $2: measure "cpu" or "memory" usage 
# $3: what ("function") to be measured
# $4: multiplier 
# $5: title (the ending)
produce_graph() {
	echo > ${OUTPUT_DIRECTORY}/"make_gnuplot_${2}_${5}.dat"

	Rscript ${SCRIPT_DIRECTORY}/convert_to_gnuplot.R ${1} "${1}/${2}_difference_${3}.dat" ${OUTPUT_DIRECTORY} "make_gnuplot_${2}_${5}.dat"

	gnuplot -c ${SCRIPT_DIRECTORY}/plot_graph.plt "${OUTPUT_DIRECTORY}/make_gnuplot_${2}_${5}.dat" "${OUTPUT_DIRECTORY}/${2}_graph_${5}.pdf" "${4}"
}


# $1: directory before optimization
# $2: directory after optimization
# $3: measure "cpu" or "memory" usage 
# $4: what to be compared
# $5: multiplier 
# $6: what to be compared
# $7: title of graph (parts of it - other part is constant)
# $8: what type of comparison: 1 if functions are compared and 0 for connection
produce_graph_compare() {
	gnuplot -c ${SCRIPT_DIRECTORY}/plot_graph_compare.plt "${OUTPUT_DIRECTORY}/make_gnuplot_${3}_${4}.dat" \
		"${OUTPUT_DIRECTORY}/${3}_graph_compare_${7}.pdf" "${5}" "${OUTPUT_DIRECTORY}/make_gnuplot_${3}_${6}.dat" ${8}
}

echo "This script will produce graphs..."

# make results directory if it doesn't exists
if [[ ! -d ${OUTPUT_DIRECTORY} ]]; then
	mkdir ${OUTPUT_DIRECTORY}
fi

# Total JSON usage of all graphs
produce_graph ${CPU_DIR} "cpu" "json" 0.000001 "json_before"
produce_graph ${CPU_DIR_OPT} "cpu" "json" 0.000001 "json_after"
produce_graph_compare ${CPU_DIR} ${CPU_DIR_OPT} "cpu" "json_before" 0.000001 "json_after" "json" 1

produce_graph ${MEMORY_DIR} "memory" "json" 1 "json_before"
produce_graph ${MEMORY_DIR_OPT} "memory" "json" 1 "json_after"
produce_graph_compare ${MEMORY_DIR} ${MEMORY_DIR_OPT} "memory" "json_before" 1 "json_after" "json" 1

# Every sampled function
produce_graph ${MEMORY_DIR} "memory" "jsonloads" 1 "jsonloads_before"
produce_graph ${MEMORY_DIR_OPT} "memory" "jsonloads" 1 "jsonloads_after"
produce_graph ${CPU_DIR} "cpu" "jsonloads" 0.000001 "jsonloads_before"
produce_graph ${CPU_DIR_OPT} "cpu" "jsonloads" 0.000001 "jsonloads_after"

produce_graph ${CPU_DIR} "cpu" "jsonpack" 0.000001 "jsonpack_before"
produce_graph ${CPU_DIR_OPT} "cpu" "jsonpack" 0.000001 "jsonpack_after"
produce_graph ${MEMORY_DIR} "memory" "jsonpack" 1 "jsonpack_before"
produce_graph ${MEMORY_DIR_OPT} "memory" "jsonpack" 1 "jsonpack_after"

produce_graph ${CPU_DIR} "cpu" "jsondumps" 0.000001 "jsondumps_before"
produce_graph ${CPU_DIR_OPT} "cpu" "jsondumps" 0.000001 "jsondumps_after"
produce_graph ${MEMORY_DIR} "memory" "jsondumps" 1 "jsondumps_before"
produce_graph ${MEMORY_DIR_OPT} "memory" "jsondumps" 1 "jsondumps_after"

produce_graph ${CPU_DIR} "cpu" "jsondecref" 0.000001 "jsondecref_before"
produce_graph ${CPU_DIR_OPT} "cpu" "jsondecref" 0.000001 "jsondecref_after"
produce_graph ${MEMORY_DIR} "memory" "jsondecref" 1 "jsondecref_before"
produce_graph ${MEMORY_DIR_OPT} "memory" "jsondecref" 1 "jsondecref_after"

produce_graph_compare ${CPU_DIR} ${CPU_DIR_OPT} "cpu" "jsonloads_before" 0.000001 "jsonloads_after" "jsonloads" 1
produce_graph_compare ${MEMORY_DIR} ${MEMORY_DIR_OPT} "memory" "jsonloads_before" 1 "jsonloads_after" "jsonloads" 1
produce_graph_compare ${CPU_DIR} ${CPU_DIR_OPT} "cpu" "jsonpack_before" 0.000001 "jsonpack_after" "jsonpack" 1
produce_graph_compare ${MEMORY_DIR} ${MEMORY_DIR_OPT} "memory" "jsonpack_before" 1 "jsonpack_after" "jsonpack" 1
produce_graph_compare ${CPU_DIR} ${CPU_DIR_OPT} "cpu" "jsondumps_before" 0.000001 "jsondumps_after" "jsondumps" 1
produce_graph_compare ${MEMORY_DIR} ${MEMORY_DIR_OPT} "memory" "jsondumps_before" 1 "jsondumps_after" "jsondumps" 1
produce_graph_compare ${CPU_DIR} ${CPU_DIR_OPT} "cpu" "jsondecref_before" 0.000001 "jsondecref_after" "jsondecref" 1
produce_graph_compare ${MEMORY_DIR} ${MEMORY_DIR_OPT} "memory" "jsondecref_before" 1 "jsondecref_after" "jsondecref" 1

# Graphs for connection establishment
produce_graph ${CONNECTION_DIR} "cpu" "connection" 0.000001 "connection_before"
produce_graph ${CONNECTION_DIR_OPT} "cpu" "connection" 0.000001 "connection_after"
produce_graph_compare ${CONNECTION_DIR} ${CONNECTION_DIR_OPT} "cpu" "connection_before" 0.000001 "connection_after" "connection" 1

produce_graph ${CONNECTION_DIR} "memory" "connection" 1 "connection_before"
produce_graph ${CONNECTION_DIR_OPT} "memory" "connection" 1 "connection_after"
produce_graph_compare ${CONNECTION_DIR} ${CONNECTION_DIR_OPT} "memory" "connection_before" 1 "connection_after" "connection" 1

# How much the json usage take of the connection establishment before improvements
produce_graph_compare ${CONNECTION_DIR} ${CPU_DIR} "cpu" "connection_before" 0.000001 "json_before" "connection_json" 0
produce_graph_compare ${CONNECTION_DIR} ${MEMORY_DIR} "memory" "connection_before" 1 "json_before" "connection_json" 0

echo "The script is finished!"
echo "The results can be found in ${OUTPUT_DIRECTORY}"