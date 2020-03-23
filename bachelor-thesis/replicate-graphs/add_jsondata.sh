#!/bin/bash

DIRECTORY=$1
number_of_flows=$2
sample_cpu=$3
sample_memory=$4

json_functions=("jsondumps" "jsonpack" "jsonloads" "jsondecref" "jsonobjectset" "jsonobjectget")

add_jsondata_in_new_file() {
	files=$(find "${1}" -type f -name "json_cpu_difference_*.log")
	for file in ${files}; do
		filename=$(echo ${file:41})
		if [[ ! -f ${1}/"${2}_${filename}" ]]; then
			> ${1}/"${2}_${filename}"
		fi
		filelength=$(cat ${file} | wc -l)
		for (( l = 1; l <= ${filelength}; l++ )); do
			value=$(awk "NR==${l}" ${file})
			cpu_time=$((cpu_time + value))
		done
		printf "${cpu_time}\n" >> ${1}/"${2}_${filename}"
		cpu_time=0
		> ${file}
	done
}

calculate_diffs_memory() {
	for function in "${json_functions[@]}"; do
		if [[ ! -f ${1}/"${2}_json_memory_difference_${function}" ]]; then
			> ${1}/"${2}_json_memory_difference_${function}"
		fi
		memory_before=$(find "${1}" -type f -name "json_memory_${function}_before.log")
		memory_after=$(find "${1}" -type f -name "json_memory_${function}_after.log")
		filelength=$(cat ${1}/json_memory_${function}_after.log | wc -l)
		for (( m = 2; m <= "${filelength}"; m+=2 )); do
			value_before=$(awk "NR==${m}" ${memory_before})
			value_after=$(awk "NR==${m}" ${memory_after})
			memory_diff="$((memory_diff + value_after - value_before))"
		done
		if [[ ${memory_diff} -lt 0 ]]; then # to avoid negative numbers
			memory_diff=0
		fi
		printf "${memory_diff}\n" >> ${1}/"${2}_json_memory_difference_${function}"
		> ${1}/"json_memory_${function}_before.log"
		> ${1}/"json_memory_${function}_after.log"
		memory_diff=0
	done
}

# can replace $1 and $2 in the functions with ${DIRECTORY} and ${number_of_flows} directly
if [[ ${sample_cpu} == "cpu" ]]; then
	add_jsondata_in_new_file ${DIRECTORY} ${number_of_flows}
fi

if [[ ${sample_memory} == "memory" ]]; then
	calculate_diffs_memory ${DIRECTORY} ${number_of_flows}
fi
