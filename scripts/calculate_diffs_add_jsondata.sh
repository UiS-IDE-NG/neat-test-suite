#!/bin/bash

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
DIRECTORY=$1
flows=(1 2 4 8 16 32 64 128 256)		
number_of_tests=10			

type=("cpu_application" "memory_application")
when=("start" "afterallconnected")  
json_functions=("jsondumps" "jsonpack" "jsonloads" "jsondecref")

# $1: what directory
# $2: starting point
# $3: end point
# $4: file name (cpu)
# $5: file name (memory)
calculate_diffs() {	
	for (( i = 1; i <= "${#flows[@]}"; i++ )); do 	
		cpu_before=$(find "${1}" -type f -name "${i}_${2}_${type[0]}.log")
		cpu_after=$(find "${1}" -type f -name "${i}_${3}_${type[0]}.log")
		memory_before=$(find "${1}" -type f -name "${i}_${2}_${type[1]}.log")
		memory_after=$(find "${1}" -type f -name "${i}_${3}_${type[1]}.log")
		filelength=$(cat ${1}/${i}_${3}_${type[0]}.log | wc -l)
		if [[ ! -z ${cpu_after} ]]; then
			for (( j = 1; j <= "${filelength}"; j++ )); do
				index="$((j * 2))"	# to avoid RSS text in the files for memory
				line1=$(awk "NR==${j}" ${cpu_before})
				line2=$(awk "NR==${j}" ${cpu_after})
				line3=$(awk "NR==${index}" ${memory_before})
				line4=$(awk "NR==${index}" ${memory_after})
				cpu_diff="$((line2 - line1))"
				memory_diff="$((line4 - line3))"
				printf "${cpu_diff} " >> "${4}"
				printf "${memory_diff} " >> "${5}"	
			done
			printf "\n" >> "${4}"
			printf "\n" >> "${5}"
		fi
	done
}

# $1: directory
# $2: function 
# $3: "cpu" or "memory"
data_together_in_new_file() {
	> ${1}/"$3_difference_$2.dat"
	
	for (( i = 1; i <= "${#flows[@]}"; i++ )); do
		file=$(find "${1}" -type f -name "${i}_json_$3_difference_$2.log")
		filelength=$(cat ${file} | wc -l)
		for (( j = 1; j <= ${filelength}; j++ )); do
			value=$(awk "NR==${j}" ${file})
			printf "${value} " >> ${1}/"$3_difference_$2.dat"
		done
		printf "\n" >> ${1}/"$3_difference_$2.dat"
	done
}

# $1: directory
# $2: "cpu" or "memory" usage
add_jsonfunctionsdata_together() {
	files=$(find "${1}" -type f -name "$2_difference_*.dat")
	cpu_time=0

	>> ${1}/"$2_difference_json.dat"

	for (( i = 1; i <= "${#flows[@]}"; i++ )); do 	# number of lines
		for (( j = 0; j < "${number_of_tests}"; j++ )); do
			for file in ${files}; do
				if [[ ${file} != "/home/helena/results-neat-test-suite/client/tcp/$2_difference_connection.dat" && ${file} != "/home/helena/results-neat-test-suite/client/tcp/$2_difference_sampling.dat" ]]; then
					line=$(awk "NR==${i}" ${file})
					numbers=($line)
					cpu_time=$((cpu_time + numbers[j]))
				fi
			done
			printf "${cpu_time} " >> ${1}/"$2_difference_json.dat"
			cpu_time=0
		done
		printf "\n" >> ${1}/"$2_difference_json.dat"
	done
}

# $1: what directory
# $2: measure "cpu" or "memory" usage 
# $3: what ("function") to be measured
# $4: multiplier 
produce_graph() {
	cd ${1}
	if [[ ! -d "gnuplot" ]]; then
		mkdir gnuplot
	fi

	echo > ${1}/"gnuplot/make_gnuplot_${2}_${3}.dat"

	Rscript ${SCRIPT_DIR}/convert_to_gnuplot.R ${1} "${2}_difference_${3}.dat" "${1}/gnuplot" "make_gnuplot_${2}_${3}.dat"

	gnuplot -c ${SCRIPT_DIR}/plot_graph.plt "${1}/gnuplot/make_gnuplot_${2}_${3}.dat" "${1}/gnuplot/${2}_graph_${3}.pdf" "${4}"
}

# $1: what directory
delete_empty_files() {
	files=$(find "${1}" -type f -name "json_*.log")

	cd $1

	for file in ${files}; do
		if [[ ! -s ${file} ]]; then
			rm ${file}
		fi
	done
}

echo "This script will calculate the CPU and memory usage"

# 
for dir in $(find ${DIRECTORY} -mindepth 3 -type d); do
	delete_empty_files ${dir}

	echo "For dir: ${dir}"

	echo "Calculating the difference in cpu and memory usage for connection..."	# not accurate unless only connection sampling
	calculate_diffs "${dir}" "start" "afterallconnected" "${dir}/cpu_difference_connection.dat" "${dir}/memory_difference_connection.dat"

	# if the dir has sampled memory or cpu usage
	if [[ ! -z $(find ${dir} -type f -name "9_json_cpu_*.log") ]]; then
		echo "Add CPU data together..."
		data_together_in_new_file "${dir}" "jsondumps" "cpu"
		data_together_in_new_file "${dir}" "jsonpack" "cpu"
		data_together_in_new_file "${dir}" "jsonloads" "cpu"
		data_together_in_new_file "${dir}" "jsondecref" "cpu"
		add_jsonfunctionsdata_together "${dir}" "cpu"
	elif [[ ! -z $(find ${dir} -type f -name "9_json_memory_*.log") ]]; then
		echo "Add memory data together..."
		data_together_in_new_file "${dir}" "jsondumps" "memory"
		data_together_in_new_file "${dir}" "jsonpack" "memory"
		data_together_in_new_file "${dir}" "jsonloads" "memory"
		data_together_in_new_file "${dir}" "jsondecref" "memory"
		add_jsonfunctionsdata_together "${dir}" "memory"
	else 
		echo "Connection"
	fi

	echo "Procucing the graphs for..."

	if [[ ! -z $(find ${dir} -type f -name "9_json_cpu_*.log") ]]; then
		echo "CPU"
		produce_graph ${dir} "cpu" "jsondumps" 0.000001
		produce_graph ${dir} "cpu" "jsonpack" 0.000001
		produce_graph ${dir} "cpu" "jsonloads" 0.000001
		produce_graph ${dir} "cpu" "jsondecref" 0.000001
		produce_graph ${dir} "cpu" "json" 0.000001
	elif [[ ! -z $(find ${dir} -type f -name "9_json_memory_*.log") ]]; then
		echo "Memory"
		produce_graph ${dir} "memory" "jsondumps" 1
		produce_graph ${dir} "memory" "jsonpack" 1
		produce_graph ${dir} "memory" "jsonloads" 1
		produce_graph ${dir} "memory" "jsondecref" 1
		produce_graph ${dir} "memory" "json" 1
	else 
		echo "Connection"
		produce_graph ${dir} "cpu" "connection" 0.000001
		produce_graph ${dir} "memory" "connection" 1
	fi
done

echo "The script is finished"
echo "The results can be found in ${DIRECTORY}"