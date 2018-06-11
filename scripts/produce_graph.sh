#!/bin/bash

# This script produces a single connection graph given a specified table file.
# This script is run from the script "produce_graphs_all.sh".

# Arguments:
# $1 - The tables directory
# $2 - The name of the table
# $3 - The output directory for the graph
# $4 - The output name of the graph
# $5 - The directory of the graph description
# $6 - Multiplier

TABLES_PATH=$1
TABLE_NAME=$2
GRAPH_PATH=$3
GRAPH_NAME=$4
DESCRIPTION=$5
MULTIPLIER=$6

echo "${DESCRIPTION}"

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
GNUPLOT_DIR="gnuplot"

results_path=${TABLES_PATH}
if [ "${results_path: -1}" != "/" ]; then
    results_path="${results_path}/"
fi

mkdir -p ${results_path}../${GNUPLOT_DIR}
mkdir -p ${GRAPH_PATH}

# Convert table to GNUPlot format
Rscript ${SCRIPT_DIR}/convert_to_gnuplot.R ${results_path} ${TABLE_NAME} ${results_path}../${GNUPLOT_DIR} ${TABLE_NAME}

# Default graph configuration
title="*"
xlabel="Undefined"
ylabel="Undefined"
y_min="*"
y_max="*"

# Change default graph configurations based on values in the description file if
# it is available.
if [ -f "${DESCRIPTION}/info.txt" ]; then
    title=$(cat ${DESCRIPTION}/info.txt | awk 'NR==1 {print}')
    xlabel=$(cat ${DESCRIPTION}/info.txt | awk 'NR==2 {print}')
    ylabel=$(cat ${DESCRIPTION}/info.txt | awk 'NR==3 {print}')
    y_min=$(cat ${DESCRIPTION}/info.txt | awk 'NR==4 {print}')
    y_max=$(cat ${DESCRIPTION}/info.txt | awk 'NR==5 {print}')
fi

gnuplot -e "v_title='${title}'; v_xlabel='${xlabel}'; v_ylabel='${ylabel}'; v_ymin='${y_min}'; v_ymax='${y_max}'" -c plot_graph.plt ${results_path}../${GNUPLOT_DIR}/${TABLE_NAME} ${GRAPH_PATH}/${GRAPH_NAME} ${MULTIPLIER}
