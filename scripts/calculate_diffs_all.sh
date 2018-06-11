#!/bin/bash

# This script calculates diffs and aggregates results of the
# parsed data. By running this script, different result tables
# are made that summarizes the results and can be used to
# produce graphs.

# Arguments:
# $1 - The root directory containing all log files for all experiments
# $2 - If "1", force calculating diffs and aggregation of all results. Otherwise,
#      only calculate diffs and aggregate results for data that this script
#      has not yet been run with

ARG1=$1
ARG2=$2

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
ROOT_DIR=${ARG1}
if [ "${ROOT_DIR: -1}" != "/" ]; then
    ROOT_DIR="${ROOT_DIR}/"
fi
FORCE_PARSE="0"
if [ "${ARG2}" == "1" ]; then
    FORCE_PARSE="1"
fi

#
# Directory tree structure
#

# Common
LEVEL_1="neat libuv kqueue"

# NEAT
LEVEL_2_NEAT="no-mult"
LEVEL_3_NEAT_NOMULT="connection transfer"
LEVEL_4_NEAT_NOMULT_CONNECTION="tcp sctp he"
LEVEL_4_NEAT_NOMULT_TRANSFER="upload"
LEVEL_5_NEAT_NOMULT_CONNECTION_HE="listen-tcp listen-sctp listen-sctptcp"
LEVEL_5_NEAT_NOMULT_TRANSFER_UPLOAD="tcp"
LEVEL_6_NEAT_NOMULT_CONNECTION_HE_LISTENSCTPTCP="default_he_delay minimal_he_delay"

# libuv
LEVEL_2_LIBUV="connection transfer"
LEVEL_3_LIBUV_CONNECTION="tcp sctp"
LEVEL_3_LIBUV_TRANSFER="upload"
LEVEL_4_LIBUV_TRANSFER_UPLOAD="tcp"

# kqueue
LEVEL_2_KQUEUE="connection"
LEVEL_3_KQUEUE_CONNECTION="tcp sctp"

#
# Print general information
#

echo "Welcome to the neat-test-suite diff generator and aggregator!"
echo ""
echo "This script will calculate diffs of the parsed data"
echo "and then aggregate all experiment data for different experiment"
echo "runs into a single file."
echo ""
echo "This script calculates the total CPU usage, memory usage, and delay"
echo "of the application."
echo ""
echo "After the script has calculated diffs and aggregated the results,"
echo "the results are put into a tabular format into the \"tables\""
echo "subdirectory of each experiment directory"
echo ""
echo "This script is located in: ${SCRIPT_DIR}"
echo "Results root directory:    ${ROOT_DIR}"
echo "Force parse results:       ${FORCE_PARSE}"
echo ""

#
# Iterate the results directory tree, calculating diffs and aggregate data for all experiments
#

for v_application in $LEVEL_1
do
    # NEAT
    if [ "${v_application}" == "neat" ]; then
	for v_mode in $LEVEL_2_NEAT
	do
	    for v_type in $LEVEL_3_NEAT_NOMULT
	    do
		# NEAT connection
		if [ "${v_type}" == "connection" ]; then
		    echo "Calculating diffs and aggregating NEAT connection results..."
		    echo "############################################################"
		    echo ""
		    for v_protocol in $LEVEL_4_NEAT_NOMULT_CONNECTION
		    do
			current_branch="${v_application}/${v_mode}/${v_type}/${v_protocol}"
			if [ "${v_protocol}" == "he" ]; then
			    for v_he_type in $LEVEL_5_NEAT_NOMULT_CONNECTION_HE
			    do
				if [ "${v_he_type}" == "listen-sctptcp" ]; then
				    for v_listen_both_type in $LEVEL_6_NEAT_NOMULT_CONNECTION_HE_LISTENSCTPTCP
				    do
					${SCRIPT_DIR}/calculate_diffs.sh "${ROOT_DIR}${current_branch}/${v_he_type}/${v_listen_both_type}" ${FORCE_PARSE}
				    done
				else
				    ${SCRIPT_DIR}/calculate_diffs.sh "${ROOT_DIR}${current_branch}/${v_he_type}" ${FORCE_PARSE}
				fi
			    done
			else
			    ${SCRIPT_DIR}/calculate_diffs.sh "${ROOT_DIR}${current_branch}" ${FORCE_PARSE}
			fi
		    done
		fi # End NEAT connection

		# NEAT transfer
		if [ "${v_type}" == "transfer" ]; then
		    echo "Calculating diffs and aggregating NEAT transfer results..."
		    echo "##########################################################"
		    echo ""
		    for v_transfer_mode in $LEVEL_4_NEAT_NOMULT_TRANSFER
		    do
			for v_protocol in $LEVEL_5_NEAT_NOMULT_TRANSFER_UPLOAD
			do
			    current_branch="${v_application}/${v_mode}/${v_type}/${v_transfer_mode}/${v_protocol}"
			    ${SCRIPT_DIR}/calculate_diffs_transfer.sh "${ROOT_DIR}${current_branch}" ${FORCE_PARSE}
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
		echo "Calculating diffs and aggregating libuv connection results..."
		echo "#############################################################"
		echo ""
		for v_protocol in $LEVEL_3_LIBUV_CONNECTION
		do
		    current_branch="${v_application}/${v_type}/${v_protocol}"
		    ${SCRIPT_DIR}/calculate_diffs.sh "${ROOT_DIR}${current_branch}" ${FORCE_PARSE}
		done
	    fi # End libuv connection

	    # libuv transfer
	    if [ "${v_type}" == "transfer" ]; then
		echo "Calculating diffs and aggregating libuv transfer results..."
		echo "#############################################################"
		echo ""
		for v_transfer_mode in $LEVEL_3_LIBUV_TRANSFER
		do
		    for v_protocol in $LEVEL_4_LIBUV_TRANSFER_UPLOAD
		    do
			current_branch="${v_application}/${v_type}/${v_transfer_mode}/${v_protocol}"
			${SCRIPT_DIR}/calculate_diffs_transfer.sh "${ROOT_DIR}${current_branch}" ${FORCE_PARSE}
		    done
		done
	    fi # End libuv transfer
	done
    fi # End libuv

    # kqueue
    if [ "${v_application}" == "kqueue" ]; then
	for v_type in $LEVEL_2_KQUEUE
	do
	    echo "Calculating diffs and aggregating kqueue connection results..."
	    echo "##############################################################"
	    echo ""
	    for v_protocol in $LEVEL_3_KQUEUE_CONNECTION
	    do
		current_branch="${v_application}/${v_type}/${v_protocol}"
		${SCRIPT_DIR}/calculate_diffs.sh "${ROOT_DIR}${current_branch}" ${FORCE_PARSE}
	    done
	done
    fi # End kqueue
done

echo "Done!"
