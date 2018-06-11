#!/bin/bash

# This script iterates through all the results directories and runs
# a graph production script for each of the directories.
#
# Arguments:
# $1 - The base directory containing the tree of results directories
#
# Requirements:
# - Graphs will only be produced for a specific results directory,
#   if there exists a "table" directory within the results directory
#   which contains the parsed, diffed and aggregated results. The
#   table directory is created after running the "calculate_diffs_all.sh"
#   for a results directory that has already been parsed with "parse_all.sh".

# The base directory
DIRECTORY=$1

# The directory of this script
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

# The name of the tables directory produced by "calculate_diffs_all.sh"
TABLES_DIR=tables

# The name of the directory containing tables converted to gnuplot format
GNUPLOT_DIR="gnuplot"

#
# Directory tree structure
#

# Common
LEVEL_1="neat kqueue libuv"

# NEAT
LEVEL_2_NEAT="no-mult"
LEVEL_3_NEAT_NOMULT="connection transfer"
LEVEL_4_NEAT_NOMULT_CONNECTION="tcp sctp he"
LEVEL_4_NEAT_NOMULT_TRANSFER="upload"
LEVEL_5_NEAT_NOMULT_CONNECTION_HE="listen-tcp listen-sctp listen-sctptcp"
LEVEL_5_NEAT_NOMULT_TRANSFER_UPLOAD="tcp"
LEVEL_6_NEAT_NOMULT_CONNECTION_HE_LISTENSCTPTCP="default_he_delay minimal_he_delay"

# The number of flows open during data transfer
NEAT_TRANSFER_FLOWS="1 2 4 8"

# libuv
LEVEL_2_LIBUV="connection transfer"
LEVEL_3_LIBUV_CONNECTION="tcp sctp"
LEVEL_3_LIBUV_TRANSFER="upload"
LEVEL_4_LIBUV_TRANSFER_UPLOAD="tcp"

# The number of flows open during data transfer
LIBUV_TRANSFER_FLOWS="1 2 4 8"

# kqueue
LEVEL_2_KQUEUE="connection"
LEVEL_3_KQUEUE_CONNECTION="tcp sctp"

#
# Graph types
#

# Which metrics to produce graphs for
METRICS="memory cpu time"

# Specific submetrics within each metric to produce graphs for
METRIC_SPECIFICS="memory_all_application cpu_all_application time_application time_establishment"

# Different time intervals within the program execution to produce graphs for
WHEN="startconnection forloop transfer"

# Multiply the data in the graph based on metric type
multiplier=1


#
# Add "/" at the end of path if it is not already present
#
results_path=${DIRECTORY}
if [ "${results_path: -1}" != "/" ]; then
    results_path="${results_path}/"
fi

# The name of the directory the results are located in
results_folder_name=$(echo $results_path | awk -F"/" '{NF=NF-1; print $NF}')

# Will be set to "server" or "client" based on the name of the directory name
host_name="${results_folder_name}"

if [ "${results_folder_name}" == "testhost2-freebsd-tables" ]; then
    host_name="server"
fi
if [ "${results_folder_name}" == "testhost5-freebsd-tables" ]; then
    host_name="client"
fi

for v_when in $WHEN
do
    for v_metric in $METRICS
    do
	for v_metric_specific in $METRIC_SPECIFICS
	do
	    first_word=$(echo $v_metric_specific | awk -F "_" '{print $1}')

	    if [ "$v_metric" != "$first_word" ]; then
		continue
	    fi

	    if [ "$v_when" == "forloop" ]; then
		if [ "$v_metric_specific" != "time_application" ]; then
		    continue
		fi
	    fi

	    table_name="${v_when}_${v_metric_specific}.tbl"
	    output_name="${v_when}_${v_metric_specific}.pdf"
	    metric_specific_short=$(echo ${v_metric_specific} | awk -F "_" '{print $NF}')

	    if [ "$v_metric" == "memory" ]; then
		# Data in kilobytes
		multiplier=1
	    elif [ "$v_metric" == "cpu" ]; then
		# Data in milliseconds
		multiplier=0.000001
	    elif [ "$v_metric" == "time" ]; then
		# Data in milliseconds
		multiplier=0.000001
	    else
		continue
	    fi

	    #
	    # Iterate the directories tree
	    #
	    for v_application in $LEVEL_1
	    do
		# NEAT
		if [ "${v_application}" == "neat" ]; then
		    for v_mode in $LEVEL_2_NEAT
		    do
			for v_type in $LEVEL_3_NEAT_NOMULT
			do
			    # NEAT Connection
			    if [ "${v_type}" == "connection" ]; then
				for v_protocol in $LEVEL_4_NEAT_NOMULT_CONNECTION
				do
				    current_branch="${v_application}/${v_mode}/${v_type}/${v_protocol}"
				    if [ "${v_protocol}" == "he" ]; then
					for v_he_type in $LEVEL_5_NEAT_NOMULT_CONNECTION_HE
					do
					    if [ "${v_he_type}" == "listen-sctptcp" ]; then
						for v_listen_both_type in $LEVEL_6_NEAT_NOMULT_CONNECTION_HE_LISTENSCTPTCP
						do
						    tables_path="${results_path}${current_branch}/${v_he_type}/${v_listen_both_type}/${TABLES_DIR}"
						    description_path="${SCRIPT_DIR}/${current_branch}/${v_he_type}/${v_listen_both_type}/${v_metric}/${v_when}/${metric_specific_short}/${results_folder_name}"
						    output_path="${SCRIPT_DIR}/graphs"
						    output_name="${v_application}_${v_mode}_${v_type}_${v_protocol}_${v_he_type}_${v_listen_both_type}_${v_when}_${v_metric_specific}_${host_name}.pdf"
						    if [ ! -f "${tables_path}/${table_name}" ]; then
							echo "No table named \"${table_name}\" in \"${tables_path}\""
							continue
						    fi
						    ${SCRIPT_DIR}/produce_graph.sh ${tables_path} ${table_name} ${output_path} ${output_name} ${description_path} ${multiplier}
						done
					    else
						tables_path="${results_path}${current_branch}/${v_he_type}/${TABLES_DIR}"
						description_path="${SCRIPT_DIR}/${current_branch}/${v_he_type}/${v_metric}/${v_when}/${metric_specific_short}/${results_folder_name}"
						output_path="${SCRIPT_DIR}/graphs"
						output_name="${v_application}_${v_mode}_${v_type}_${v_protocol}_${v_he_type}_${v_when}_${v_metric_specific}_${host_name}.pdf"
						if [ ! -f "${tables_path}/${table_name}" ]; then
						    echo "No table named \"${table_name}\" in \"${tables_path}\""
						    continue
						fi
						${SCRIPT_DIR}/produce_graph.sh ${tables_path} ${table_name} ${output_path} ${output_name} ${description_path} ${multiplier}
					    fi
					done
				    else
					tables_path="${results_path}${current_branch}/${TABLES_DIR}"
					description_path="${SCRIPT_DIR}/${current_branch}/${v_metric}/${v_when}/${metric_specific_short}/${results_folder_name}"
					output_path="${SCRIPT_DIR}/graphs"
					output_name="${v_application}_${v_mode}_${v_type}_${v_protocol}_${v_when}_${v_metric_specific}_${host_name}.pdf"
					if [ ! -f "${tables_path}/${table_name}" ]; then
					    echo "No table named \"${table_name}\" in \"${tables_path}\""
					    continue
					fi
					${SCRIPT_DIR}/produce_graph.sh ${tables_path} ${table_name} ${output_path} ${output_name} ${description_path} ${multiplier}
				    fi
				done
			    fi # End NEAT connection
			    
			    # NEAT transfer
			    if [ "${v_type}" == "transfer" ]; then
				for v_transfer_mode in $LEVEL_4_NEAT_NOMULT_TRANSFER
				do
				    for v_protocol in $LEVEL_5_NEAT_NOMULT_TRANSFER_UPLOAD
				    do
					current_branch="${v_application}/${v_mode}/${v_type}/${v_transfer_mode}/${v_protocol}"
					tables_path="${results_path}${current_branch}/${TABLES_DIR}"
					description_path="${SCRIPT_DIR}/${current_branch}/${v_metric}/${v_when}/${metric_specific_short}/${results_folder_name}"
					output_path="${SCRIPT_DIR}/graphs"
					output_name="${v_application}_${v_mode}_${v_type}_${v_transfer_mode}_${v_protocol}_${v_when}_${v_metric_specific}_${host_name}.pdf"
					table_not_found=0
					for v_transfer_flows in $NEAT_TRANSFER_FLOWS
					do
					    table_name="${v_when}_${v_transfer_flows}_${v_metric_specific}.tbl"
					    if [ ! -f "${tables_path}/${table_name}" ]; then
						echo "No table named \"${table_name}\" in \"${tables_path}\""
						table_not_found=1
						continue
					    fi
					    mkdir -p ${tables_path}/../${GNUPLOT_DIR}
					    Rscript ${SCRIPT_DIR}/convert_to_gnuplot.R ${tables_path} ${table_name} ${tables_path}/../${GNUPLOT_DIR} ${table_name}
					    cat ${tables_path}/../${GNUPLOT_DIR}/${table_name}
					done
					if [ "${table_not_found}" -eq "1" ]; then
					    table_name="${v_when}_${v_metric_specific}.tbl"
					    continue
					fi
					${SCRIPT_DIR}/produce_graph_transfer.sh ${tables_path}/../${GNUPLOT_DIR} ${v_metric_specific} ${output_path} ${output_name} ${description_path} ${multiplier}
					table_name="${v_when}_${v_metric_specific}.tbl"
				    done
				done
			    fi # End NEAT transfer
			done
		    done
		fi # End NEAT

		# libuv
		if [ "${v_application}" == "libuv" ]; then
		    for v_type in $LEVEL_2_LIBUV
		    do
			# libuv connection
			if [ "${v_type}" == "connection" ]; then
			    for v_protocol in $LEVEL_3_LIBUV_CONNECTION
			    do
				current_branch="${v_application}/${v_type}/${v_protocol}"
				tables_path="${results_path}${current_branch}/${TABLES_DIR}"
				description_path="${SCRIPT_DIR}/${current_branch}/${v_metric}/${v_when}/${metric_specific_short}/${results_folder_name}"
				output_path="${SCRIPT_DIR}/graphs"
				output_name="${v_application}_${v_type}_${v_protocol}_${v_when}_${v_metric_specific}_${host_name}.pdf"
				if [ ! -f "${tables_path}/${table_name}" ]; then
				    echo "No table named \"${table_name}\" in \"${tables_path}\""
				    continue
				fi
				${SCRIPT_DIR}/produce_graph.sh ${tables_path} ${table_name} ${output_path} ${output_name} ${description_path} ${multiplier}
			    done
			fi # End libuv connection

			# libuv transfer
			if [ "${v_type}" == "transfer" ]; then
			    for v_transfer_mode in $LEVEL_3_LIBUV_TRANSFER
			    do
				for v_protocol in $LEVEL_4_LIBUV_TRANSFER_UPLOAD
				do
				    current_branch="${v_application}/${v_type}/${v_transfer_mode}/${v_protocol}"
				    tables_path="${results_path}${current_branch}/${TABLES_DIR}"
				    description_path="${SCRIPT_DIR}/${current_branch}/${v_metric}/${v_when}/${metric_specific_short}/${results_folder_name}"
				    output_path="${SCRIPT_DIR}/graphs"
				    output_name="${v_application}_${v_type}_${v_transfer_mode}_${v_protocol}_${v_when}_${v_metric_specific}_${host_name}.pdf"
				    table_not_found=0
				    for v_transfer_flows in $LIBUV_TRANSFER_FLOWS
				    do
					table_name="${v_when}_${v_transfer_flows}_${v_metric_specific}.tbl"
					if [ ! -f "${tables_path}/${table_name}" ]; then
					    echo "No table named \"${table_name}\" in \"${tables_path}\""
					    table_not_found=1
					    continue
					fi
					mkdir -p ${tables_path}/../${GNUPLOT_DIR}
					Rscript ${SCRIPT_DIR}/convert_to_gnuplot.R ${tables_path} ${table_name} ${tables_path}/../${GNUPLOT_DIR} ${table_name}
				    done
				    if [ "${table_not_found}" -eq "1" ]; then
					table_name="${v_when}_${v_metric_specific}.tbl"
					continue
				    fi
				    ${SCRIPT_DIR}/produce_graph_transfer.sh ${tables_path}/../${GNUPLOT_DIR} ${v_metric_specific} ${output_path} ${output_name} ${description_path} ${multiplier}
				    table_name="${v_when}_${v_metric_specific}.tbl"
				done
			    done
			fi # End libuv transfer
		    done
		fi # End libuv
		
		# kqueue
		if [ "${v_application}" == "kqueue" ]; then
		    for v_type in $LEVEL_2_KQUEUE
		    do
			for v_protocol in $LEVEL_3_KQUEUE_CONNECTION
			do
			    current_branch="${v_application}/${v_type}/${v_protocol}"
			    tables_path="${results_path}${current_branch}/${TABLES_DIR}"
			    description_path="${SCRIPT_DIR}/${current_branch}/${v_metric}/${v_when}/${metric_specific_short}/${results_folder_name}"
			    output_path="${SCRIPT_DIR}/graphs"
			    output_name="${v_application}_${v_type}_${v_protocol}_${v_when}_${v_metric_specific}_${host_name}.pdf"
			    if [ ! -f "${tables_path}/${table_name}" ]; then
				echo "No table named \"${table_name}\" in \"${tables_path}\""
				continue
			    fi
			    ${SCRIPT_DIR}/produce_graph.sh ${tables_path} ${table_name} ${output_path} ${output_name} ${description_path} ${multiplier}
			done
		    done
		fi # End kqueue
	    done
	done
    done
done
