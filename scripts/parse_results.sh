#!/bin/bash

# This script parses the experiment results for one experiment.
# This script is run by the "parse_all" script for all experiments.

# Arguments:
# $1 - The directory containing all log files for a specific experiment
# $2 - If "1", force parsing of results. Otherwise, parse only if data is unparsed

DIRECTORY=$1
FORCE_PARSE=$2
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
#VMSTAT_Z_LIST_PATH=${SCRIPT_DIR}/vmstat_z_list
#VMSTAT_M_LIST_PATH=${SCRIPT_DIR}/vmstat_m_list

DATA_DIR=data

if [ ! -d "$DIRECTORY" ]; then
    echo "Directory \"${DIRECTORY}\" does not exist! Skipping..."
    echo ""
    exit 1
fi

if [ "${FORCE_PARSE}" == "0" ]; then
    if [ -d "${DIRECTORY}/${DATA_DIR}" ]; then
	echo "Directory \"${DIRECTORY}\" already parsed! Skipping..."
	echo ""
	exit 1
    fi
fi

if [ $(find ${DIRECTORY} -maxdepth 1 -iname "error.log" | wc -w) -eq 1 ]; then
    echo "An error log is present in directory \"${DIRECTORY}\"! Experiment data are invalid! Skipping..."
    echo ""
    exit 1
fi

cd ${DIRECTORY}
rm -rf ${DATA_DIR}

if [ $(find ${DIRECTORY} -maxdepth 1 -iname "*.log" | wc -w) -eq 0 ]; then
    echo "No experiment data found in directory \"${DIRECTORY}\"! Skipping..."
    echo ""
    exit 1
fi

prefixes=$(cd ${DIRECTORY}; find -type f -name '*.log' | grep "_start_" | awk -F "_start" '{print $1}' | uniq -c | awk -F " " '{print $2}')
experiment_count=$(echo $prefixes | wc -w)

parse_delay() {
    if [ $(find -maxdepth 1 -type f -name '*.log' | grep "$1" | wc -w) -eq 0 ]; then
	echo "No files with pattern \"${1}\""
	return 0
    fi
    relevant_log_files=$(cd ${DIRECTORY}; find -maxdepth 1 -type f -name '*.log' | grep "$1")
    cd ${DIRECTORY}
    mkdir -p ${DATA_DIR}
    for filename in ${relevant_log_files}
    do
	result_file=$(echo "${filename//.log}.dat")
	cp ${filename} ${DATA_DIR}/${result_file}
    done
} 

parse_cp_time () {
    if [ $(find -maxdepth 1 -type f -name '*.log' | grep "$1" | wc -w) -eq 0 ]; then
	echo "No files with pattern \"${1}\""
	return 0
    fi
    relevant_log_files=$(cd ${DIRECTORY}; find -maxdepth 1 -type f -name '*.log' | grep "$1")
    cd ${DIRECTORY}
    mkdir -p ${DATA_DIR}
    echo "debug: parsing cp_time"
    for filename in ${relevant_log_files}
    do
	usage_file=$(echo "${filename//${1}.log}cpu_globalusage.dat")
	idle_file=$(echo "${filename//${1}.log}cpu_globalidle.dat")
	v_user=$(cat $filename | awk '{print $2}')
	v_nice=$(cat $filename | awk '{print $3}')
	v_sys=$(cat $filename | awk '{print $4}')
	v_interrupt=$(cat $filename | awk '{print $5}')
	v_idle=$(cat $filename | awk '{print $6}')
	echo "${filename}"
	echo "user: ${v_user} nice: ${v_nice} sys: ${v_sys} intr: ${v_interrupt} idle: ${v_idle}"

	echo $((v_user + v_nice + v_sys + v_interrupt)) > ${DATA_DIR}/${usage_file}
	echo $((v_idle)) > ${DATA_DIR}/${idle_file}
    done
}

parse_intrps () {
    if [ $(find -maxdepth 1 -type f -name '*.log' | grep "$1" | wc -w) -eq 0 ]; then
	echo "No files with pattern \"${1}\""
	return 0
    fi
    relevant_log_files=$(cd ${DIRECTORY}; find -maxdepth 1 -type f -name '*.log' | grep "$1")
    cd ${DIRECTORY}
    mkdir -p ${DATA_DIR}
    for filename in ${relevant_log_files}
    do
	result_file=$(echo "${filename//.log}.dat")
	minsecs=$(cat $filename | tail -n 1 | awk '{print $5}' | awk -F ":" '{print $1}' | awk '{print $1 * 60}')
	secs=$(cat $filename | tail -n 1 | awk '{print $5}' | awk -F ":" '{print $2}' | awk -F "." '{print $1 * 1}')
	nsecs=$(cat $filename | tail -n 1 | awk '{print $5}' | awk -F ":" '{print $2}' | awk -F "." '{print $2 * 10000000}')
	echo $((minsecs + secs)) ${nsecs} > ${DATA_DIR}/${result_file}
    done
}

parse_gettime () {
    if [ $(find -maxdepth 1 -type f -name '*.log' | grep "$1" | wc -w) -eq 0 ]; then
	echo "No files with pattern \"${1}\""
	return 0
    fi
    relevant_log_files=$(cd ${DIRECTORY}; find -maxdepth 1 -type f -name '*.log' | grep "$1")
    cd ${DIRECTORY}
    mkdir -p ${DATA_DIR}
    for filename in ${relevant_log_files}
    do
	result_file=$(echo "${filename//.log}.dat")
	cp ${filename} ${DATA_DIR}/${result_file}
    done
}

parse_procstat () {
    if [ $(find -maxdepth 1 -type f -name '*.log' | grep "$1" | wc -w) -eq 0 ]; then
	echo "No files with pattern \"${1}\""
	return 0
    fi
    relevant_log_files=$(cd ${DIRECTORY}; find -maxdepth 1 -type f -name '*.log' | grep "$1")
    cd ${DIRECTORY}
    mkdir -p ${DATA_DIR}
    for filename in ${relevant_log_files}
    do
	result_file=$(echo "${filename//.log}.dat")
	system_field_count=$(cat "${filename}" | awk '/system time/ {print NF}')
	system_time_nsec=$(cat "${filename}" | awk "/system time/ {print \$${system_field_count}}" | awk -F'[:.]' '{value=($4*1000); printf "%d\n", value}')
	system_time_sec=$(cat "${filename}" | awk "/system time/ {print \$${system_field_count}}" | awk -F'[:.]' '{value=($1*3600+$2*60+$3); printf "%d\n", value}')
	user_field_count=$(cat "${filename}" | awk '/user time/ {print NF}')
	user_time_nsec=$(cat "${filename}" | awk "/user time/ {print \$${user_field_count}}" | awk -F'[:.]' '{value=($4*1000); printf "%d\n", value}')
	user_time_sec=$(cat "${filename}" | awk "/user time/ {print \$${user_field_count}}" | awk -F'[:.]' '{value=($1*3600+$2*60+$3); printf "%d\n", value}')
	total_sec=$(awk 'BEGIN {printf "%d\n",'"$system_time_sec + $user_time_sec}")
	total_nsec=$(awk 'BEGIN {printf "%d\n",'"$system_time_nsec + $user_time_nsec}")
	echo "$total_sec $total_nsec" > ${DATA_DIR}/${result_file}
    done
}

parse_memory_application () {
    if [ $(find -maxdepth 1 -type f -name '*.log' | grep "$1" | wc -w) -eq 0 ]; then
	echo "No files with pattern \"${1}\""
	return 0
    fi
    relevant_log_files=$(cd ${DIRECTORY}; find -maxdepth 1 -type f -name '*.log' | grep "$1")
    cd ${DIRECTORY}
    mkdir -p ${DATA_DIR}
    for filename in ${relevant_log_files}
    do
	result_file=$(echo "${filename//.log}.dat")
	mem=$(cat ${filename} | tail -n 1)
	echo "application" $((mem * 1024)) > ${DATA_DIR}/${result_file}
    done
}

parse_vmstatz () {
    if [ $(find -maxdepth 1 -type f -name '*.log' | grep "$1" | wc -w) -eq 0 ]; then
	echo "No files with pattern \"${1}\""
	return 0
    fi
    relevant_log_files=$(cd ${DIRECTORY}; find -maxdepth 1 -type f -name '*.log' | grep "$1")
    cd ${DIRECTORY}
    mkdir -p ${DATA_DIR}
    for filename in ${relevant_log_files}
    do
	result_file=$(echo "${filename//.log}.dat")
	while read line; do
	    mem=$(cat "${filename}" | awk '/'"${line}"'/ {print $2" "$4}' | awk -F, '{print $1*$2}')
	    echo $line $mem >> ${DATA_DIR}/${result_file}
	done <${VMSTAT_Z_LIST_PATH}
    done
}

parse_vmstatm () {
    if [ $(find -maxdepth 1 -type f -name '*.log' | grep "$1" | wc -w) -eq 0 ]; then
	echo "No files with pattern \"${1}\""
	return 0
    fi
    relevant_log_files=$(cd ${DIRECTORY}; find -maxdepth 1 -type f -name '*.log' | grep "$1")
    cd ${DIRECTORY}
    mkdir -p ${DATA_DIR}
    for filename in ${relevant_log_files}
    do
	result_file=$(echo "${filename//.log}.dat")
	while read line; do
	    mem=$(cat "${filename}" | awk  '/'" ${line}"'/ {print $3}' | tr -d K)
	    if [ -z "${mem}" ]; then mem=0; fi
	    echo ${line} $((mem * 1024)) >> ${DATA_DIR}/${result_file}
	done <${VMSTAT_M_LIST_PATH}
    done
}

parse_netstatm () {
    if [ $(find -maxdepth 1 -type f -name '*.log' | grep "$1" | wc -w) -eq 0 ]; then
	echo "No files with pattern \"${1}\""
	return 0
    fi
    relevant_log_files=$(cd ${DIRECTORY}; find -maxdepth 1 -type f -name '*.log' | grep "$1")
    cd ${DIRECTORY}
    mkdir -p ${DATA_DIR}
    for filename in ${relevant_log_files}
    do
	result_file=$(echo "${filename//.log}.dat")
	tmp=$(grep "bytes allocated" ${filename} | awk -F'[/ ]+' '{print $1}')
	mem=$(echo "$tmp" | tr -d K)
	echo mbufs $(($mem * 1024)) > ${DATA_DIR}/${result_file}
    done
}

echo "Parsing directory ${DIRECTORY}..."
echo "Parsing connection establishment delay logs (if available)..."
parse_delay "time_establishment"
echo "Parsing delay logs in application..."
parse_delay "time_application"
echo "Parsing application CPU logs..."
parse_gettime "cpu_application"
echo "Parsing application memory logs..."
parse_memory_application "memory_application"

echo ""
