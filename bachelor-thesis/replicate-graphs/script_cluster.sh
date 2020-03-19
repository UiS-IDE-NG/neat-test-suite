#!/bin/bash

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
DIRECTORY="/root/results-neat-test-suite"
directories=("${DIRECTORY}/client/tcp" "${DIRECTORY}/server/tcp") #"${DIRECTORY}/client/sctp" "${DIRECTORY}/server/sctp"
number_of_tests=10

flows=(1 2 4 8 16 32 64 128 256)
#flows=(256)

transport=("TCP" "1" "SCTP" "1")

host="192.168.10.2"
port=8080
log_level=1	
counter=1 	# used to differentiate between how many flows there are in each file 

server=pi4host2
client=pi4host3

type=("cpu_application" "memory_application")
when=("start" "afterallconnected")
files_cpu=("${directories[0]}/cpu_difference_connection.dat" "${directories[1]}/cpu_difference_connection.dat") 
files_memory=("${directories[0]}/memory_difference_connection.dat" "${directories[1]}/memory_difference_connection.dat")   
json_functions=("jsondumps" "jsonpack" "jsonloads" "jsondecref" "jsonobjectset" "jsonobjectget") 

# may make one calculte_diffs for memory and one for cpu instead of having them in the same function as memory is not always sampled --> pnly really needed if 
# $1: what directory
# $2: starting point
# $3: end point
# $4: file name (cpu)
# $5: file name (memory)
calculate_diffs() {	
	#correct_cpu_file=${SCRIPT_DIR}/"cpu_difference_connection.dat"	# can /should be an argument
	for (( i = 1; i <= "${#flows[@]}"; i++ )); do 	
		cpu_before=$(find "${1}" -type f -name "${i}_${2}_${type[0]}.log")
		cpu_after=$(find "${1}" -type f -name "${i}_${3}_${type[0]}.log")
		memory_before=$(find "${1}" -type f -name "${i}_${2}_${type[1]}.log")
		memory_after=$(find "${1}" -type f -name "${i}_${3}_${type[1]}.log")
		filelength=$(cat ${1}/${i}_${3}_${type[0]}.log | wc -l)
		#line=$(awk "NR==${i}" ${correct_cpu_file})
		#correct_cpu_values=($line)
		if [[ ! -z ${cpu_after} ]]; then
			for (( j = 1; j <= "${filelength}"; j++ )); do
				index="$((j * 2))"	# to avoid RSS text in the files for memory
				line1=$(awk "NR==${j}" ${cpu_before})
				line2=$(awk "NR==${j}" ${cpu_after})
				line3=$(awk "NR==${index}" ${memory_before})
				line4=$(awk "NR==${index}" ${memory_after})
				#all_diff="$((line2 - line1))"	
				#cpu_diff="$((all_diff - correct_cpu_values[j-1]))"
				cpu_diff="$((line2 - line1))"
				memory_diff="$((line4 - line3))"
				#printf "${cpu_diff} " >> "${directories[0]}/cpu_difference_sampling.dat" 
				#printf "${correct_cpu_values[j-1]} " >> "${4}"
				printf "${cpu_diff} " >> "${4}"
				printf "${memory_diff} " >> "${5}"	
				#printf "${all_diff} " >> "${directories[0]}/cpu_all_difference.dat" 
			done
			printf "\n" >> "${4}"
			printf "\n" >> "${5}"
			#printf "\n" >> "${directories[0]}/cpu_difference_sampling.dat" 
			#printf "\n" >> "${directories[0]}/cpu_all_difference.dat" 
		fi
	done
}

# $1: what directory
# $2: "how many flows" number
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

# may make a new subdirectory in ${1} where all the data is stored in its entirety - at the moment the values are added together - don't know how many samplings there are of each function
# $1: directory
# $2: "how many flows" number
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

# $1: directory
# $2: function 
data_together_in_new_file() {
	> ${1}/"cpu_difference_$2.dat"
	
	for (( i = 1; i <= "${#flows[@]}"; i++ )); do
		file=$(find "${1}" -type f -name "${i}_*_$2.log")
		filelength=$(cat ${file} | wc -l)
		for (( j = 1; j <= ${filelength}; j++ )); do
			value=$(awk "NR==${j}" ${file})
			printf "${value} " >> ${1}/"cpu_difference_$2.dat"
		done
		printf "\n" >> ${1}/"cpu_difference_$2.dat"
	done
}

# $1: directory
# $2: "cpu" or "memory" usage
add_jsonfunctionsdata_together() {
	files=$(find "${1}" -type f -name "$2_difference_*.dat")
	cpu_time=0

	>> ${1}/"cpu_difference_json.dat"

	for (( i = 1; i <= "${#flows[@]}"; i++ )); do 	# number of lines
		for (( j = 0; j < "${number_of_tests}"; j++ )); do
			for file in ${files}; do
				if [[ ${file} != "${DIRECTORY}/client/tcp/$2_difference_connection.dat" && ${file} != "${DIRECTORY}/client/tcp/$2_difference_sampling.dat" ]]; then
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

	gnuplot -c ${SCRIPT_DIR}/plot_graph.plt "${1}/gnuplot/make_gnuplot_${2}_${3}.dat" "${1}/gnuplot/${2}_graph_${3}" "${4}"
}

# $1: what directory
# $2: measure "cpu" or "memory" usage 
# $3: what ("function") to be measured
# $4: multiplier 
# $5 - 10: what json functions to be measured
produce_graph_json() {
	gnuplot -c ${SCRIPT_DIR}/plot_graph_json.plt "${1}/gnuplot/make_gnuplot_${2}_${3}.dat" "${1}/gnuplot/${2}_graph_all" "${4}" "${1}/gnuplot/make_gnuplot_${2}_${5}.dat" "${1}/gnuplot/make_gnuplot_${2}_${6}.dat" \
		"${1}/gnuplot/make_gnuplot_${2}_${7}.dat" "${1}/gnuplot/make_gnuplot_${2}_${8}.dat" "${1}/gnuplot/make_gnuplot_${2}_${9}.dat" "${1}/gnuplot/make_gnuplot_${2}_${10}.dat" #"${1}/gnuplot/make_gnuplot_${2}_${11}.dat" #\
		"${1}/gnuplot/make_gnuplot_${2}_${12}.dat" "${1}/gnuplot/make_gnuplot_${2}_${13}.dat"
} 

# $1: what directory
# $2: measure "cpu" or "memory" usage 
# $3: what to be compared
# $4: multiplier 
# $5: what to be compared
# $6: what to be compared
produce_graph_compare() {
	gnuplot -c ${SCRIPT_DIR}/plot_graph_compare.plt "${1}/gnuplot/make_gnuplot_${2}_${3}.dat" \
		"${1}/gnuplot/${2}_graph_compare" "${4}" "${1}/gnuplot/make_gnuplot_${2}_${5}.dat" "${1}/gnuplot/make_gnuplot_${2}_${6}.dat"
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

echo "Executing tests for"
for (( k = 0; k < "${#directories[@]}"; k+=2 )); do 	# tcp and sctp
	counter=1
	echo "${transport[k]}"
	for (( i = 0; i < "${#flows[@]}"; i++ )); do
		if [[ "${directories[k]}" == "${DIRECTORY}/client/tcp" ]]; then
				# TCP:
				a="${flows[i]}"
				b="0"
			else
				# SCTP: many flows doesn't work + get invalid encoding all over the place
				b="${flows[i]}"
				a="0"
		fi
		for (( j = 0; j < "${number_of_tests}"; j++ )); do
			echo "number of flow(s) ${flows[i]}, test $((j + 1))..."
			# -R "${directories[k+1]}" -A -s 		"${counter}

			ssh ${server} "cd neat-test-suite/build ; ./neat_server -C $((transport[k+1] * flows[i])) -a ${a} -b ${b} \
				-M ${transport[k]} -I '192.168.10.2' -p ${port} -v ${log_level}" \
				&>>/root/output_server.txt 2>&1 & #2>&1 &

			sleep 1 # not sure if this is necessary

			ssh ${client} "cd neat-test-suite/build ; ./neat_client -s -R ${directories[k]} -A -C ${flows[i]} -a ${a} -b ${b} \
				-M ${transport[k]} -i '192.168.10.3' -n ${flows[i]} -v ${log_level} ${host} \
				${port} ${counter}" &>>/root/output_client.txt & #&

			#wait before the programs are killed 
			if [[ ${flows[i]} -eq 1 ]]; then
				sleep 1
			elif [[ ${flows[i]} -eq 2 || ${flows[i]} -eq 4 ]]; then
				sleep 10
			elif [[ ${flows[i]} -eq 8 || ${flows[i]} -eq 16 ]]; then
				sleep 30
			elif [[ ${flows[i]} -eq 32 ]]; then
				sleep 1m
			elif [[ ${flows[i]} -eq 64 || ${flows[i]} -eq 128 ]]; then
				sleep 2m
			else
				sleep 10m
			fi

			ssh ${server} "cd neat-test-suite/build ; killall ./neat_server"
			ssh ${client} "cd neat-test-suite/build ; killall ./neat_client"
			
			echo "Add jsondata together and relocate it to a new file..."
			add_jsondata_in_new_file "${directories[k]}" "$((i + 1))"
			calculate_diffs_memory "${directories[k]}" "$((i + 1))"
		done
		counter="$((counter+1))"
	done
done

delete_empty_files ${directories[0]}

#echo "Calculating the difference in cpu and memory usage..."
#calculate_diffs "${directories[0]}" "start" "afterallconnected" ${files_cpu[0]} ${files_memory[0]}

#echo "Calculating the difference in cpu usage for json functions..."
#data_together_in_new_file "${directories[0]}" "jsondumps"
#data_together_in_new_file "${directories[0]}" "jsonpack"
#data_together_in_new_file "${directories[0]}" "jsonloads"
#data_together_in_new_file "${directories[0]}" "jsondecref"
#data_together_in_new_file "${directories[0]}" "jsonobjectset"
#data_together_in_new_file "${directories[0]}" "jsonobjectget"

#data_together_in_new_file "${directories[0]}" "jsondumps" "memory"
#data_together_in_new_file "${directories[0]}" "jsonpack" "memory"
#data_together_in_new_file "${directories[0]}" "jsonloads" "memory"
#data_together_in_new_file "${directories[0]}" "jsondecref" "memory"
#data_together_in_new_file "${directories[0]}" "jsonobjectset" "memory"
#data_together_in_new_file "${directories[0]}" "jsonobjectget" "memory"

#add_jsonfunctionsdata_together "${directories[0]}" "cpu"
#add_jsonfunctionsdata_together "${directories[0]}" "memory"

#echo "Procucing the graphs..."

#produce_graph ${directories[0]} "cpu" "connection" 0.000001
#produce_graph ${directories[0]} "memory" "connection" 1
#produce_graph ${directories[0]} "cpu" "sampling" 0.000001

#produce_graph ${directories[0]} "cpu" "jsondumps" 0.000001
#produce_graph ${directories[0]} "cpu" "jsonpack" 0.000001
#produce_graph ${directories[0]} "cpu" "jsonloads" 0.000001
#produce_graph ${directories[0]} "cpu" "jsondecref" 0.000001
#produce_graph ${directories[0]} "cpu" "jsonobjectset" 0.000001
#produce_graph ${directories[0]} "cpu" "jsonobjectget" 0.000001

#produce_graph ${directories[0]} "memory" "jsondumps" 1
#produce_graph ${directories[0]} "memory" "jsonpack" 1
#produce_graph ${directories[0]} "memory" "jsonloads" 1
#produce_graph ${directories[0]} "memory" "jsondecref" 1
#produce_graph ${directories[0]} "memory" "jsonobjectset" 1
#produce_graph ${directories[0]} "memory" "jsonobjectget" 1

#produce_graph ${directories[0]} "cpu" "json" 0.000001
#produce_graph ${directories[0]} "memory" "json" 1

#produce_graph_compare ${directories[0]} "cpu" "connection" 0.000001 "sampling" "json"
#produce_graph_compare ${directories[0]} "memory" "connection" 1 "sampling" "json"

echo "The script is finished"
#echo "The results can be found in ${DIRECTORY}"
