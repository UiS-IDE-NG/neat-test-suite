#!/bin/bash

# This script produces a single data transfer graph given a specified table file.
# This script is run from the script "produce_graphs_all.sh".

# Arguments:
# $1 - The path of the gnuplot directory with the graph data
# $2 - A substring of the files for which to create a graph from
# $3 - The output directory for the graph
# $4 - The output name of the graph
# $5 - The directory of the graph description
# $6 - Multiplier

GNUPLOT_PATH=$1
METRIC_SPECIFIC=$2
GRAPH_PATH=$3
GRAPH_NAME=$4
DESCRIPTION=$5
MULTIPLIER=$6

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

results_path=${GNUPLOT_PATH}
if [ "${results_path: -1}" != "/" ]; then
    results_path="${results_path}/"
fi

relevant_files=$(ls $results_path | grep $METRIC_SPECIFIC)
number_of_files=$(echo $relevant_files | wc -w)

if [ "${number_of_files}" -ne "4" ]; then
    echo "produce_graph_transfer: Wrong number of flows"
    exit 1
fi

# Hard-code the number of flows to consider.
file_1=$(echo $relevant_files | tr ' ' '\n' | grep "_1_")
file_2=$(echo $relevant_files | tr ' ' '\n' | grep "_2_")
file_3=$(echo $relevant_files | tr ' ' '\n' | grep "_4_")
file_4=$(echo $relevant_files | tr ' ' '\n' | grep "_8_")

mkdir -p ${GRAPH_PATH}

# Default graph configuration.
title="*"
xlabel="Undefined"
ylabel="Undefined"
y_min="*"
y_max="*"

# If a description file is available, enable changing the default graph configuraiton.
if [ -f "${DESCRIPTION}/info.txt" ]; then
    title=$(cat ${DESCRIPTION}/info.txt | awk 'NR==1 {print}')
    xlabel=$(cat ${DESCRIPTION}/info.txt | awk 'NR==2 {print}')
    ylabel=$(cat ${DESCRIPTION}/info.txt | awk 'NR==3 {print}')
    y_min=$(cat ${DESCRIPTION}/info.txt | awk 'NR==4 {print}')
    y_max=$(cat ${DESCRIPTION}/info.txt | awk 'NR==5 {print}')
fi

echo ""

gnuplot -e "v_title='${title}'; v_xlabel='${xlabel}'; v_ylabel='${ylabel}'; v_ymin='${y_min}'; v_ymax='${y_max}'" -c plot_graph_transfer.plt ${results_path}${file_1} ${results_path}${file_2} ${results_path}${file_3} ${results_path}${file_4} ${GRAPH_PATH}/${GRAPH_NAME} ${MULTIPLIER}
