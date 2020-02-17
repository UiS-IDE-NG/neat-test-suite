#!/bin/bash

# script to run the tests --> figure 5.4, 5.5, 5.12, 5.13

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
dir="/home/helena/results-neat-test-suit"
directories=("/home/helena/results-neat-test-suit/tcp" "/home/helena/results-neat-test-suit/sctp") 

number_of_tests=10

flows=(1 2 4 8 16 32 64 128 256)
#flows=(64 128 256)
#flows=(1)

transport=('TCP' 'SCTP' '1')

host="127.0.0.1"
port=8080
log_level=4	
#FILE="options_client"
counter=1 	# used to differentiate between how many flows there are in each file 

echo "This script will calculate the CPU and memory usage"

echo "Executing tests for"
for (( k = 0; k < "${#directories[@]}"; k++ )); do
	counter=1
	echo "${transport[k]}..."
	for (( i = 0; i < "${#flows[@]}"; i++ )); do
		if [[ "${directories[k]}" == "/home/helena/results-neat-test-suit/tcp" ]]; then
				# TCP: the server connects to the double amount of clients
				a="${flows[i]}"
				b="0"
			else
				# SCTP: many flows doesn't work --> 64/64 123/128 148/256 connected - varies (lower 1/...)
				b="${flows[i]}"
				a="0"
		fi
		for (( j = 0; j < "${number_of_tests}"; j++ )); do
			./neat_server -C "$((transport[2] * flows[i]))" -a "${a}" -b "${b}" \
				-M "${transport[k]}" -I "127.0.0.1" -p "${port}" -v "${log_level}" \
				>/dev/null 2>&1 & #&>>/home/helena/Documents/neat-test-suite/build/output_server.txt 2>&1 &

			sleep 1		# to make sure the server is up when the client starts
			
			./neat_client -R "${directories[k]}" -A -s -C "${flows[i]}" -a "${a}" -b "${b}" \
				-M "${transport[k]}" -i "127.0.0.1" -n "${flows[i]}" -v "${log_level}" "${host}" \
				"${port}" "${counter}" &>/dev/null & #&>>/home/helena/Documents/neat-test-suite/build/output_client.txt &

			sleep 1		# wait before the programs are killed

			killall ./neat_server
			killall ./neat_client
		done
		counter=$((counter+1))
	done
done

type=("cpu_application" "memory_application")
when=("firstconnect" "afterallconnected")
files_cpu=("${directories[0]}/cpu_difference.dat" "${directories[1]}/cpu_difference.dat") 
files_memory=("${directories[0]}/memory_difference.dat" "${directories[1]}/memory_difference.dat")   

echo "Calculating the difference in cpu and memory usage..."
for (( k = 0; k < ${#directories[@]}; k++ )); do
	for (( i = 1; i <= 9; i++ )); do
		cpu_before=$(find "${directories[k]}" -type f -name "${i}_${when[0]}_${type[0]}.log")
		cpu_after=$(find "${directories[k]}" -type f -name "${i}_${when[1]}_${type[0]}.log")
		memory_before=$(find "${directories[k]}" -type f -name "${i}_${when[0]}_${type[1]}.log")
		memory_after=$(find "${directories[k]}" -type f -name "${i}_${when[1]}_${type[1]}.log")
		if [[ ! -z ${cpu_after} ]]; then	# added since sctp isn't working properly
			for (( j = 1; j <= 10; j++ )); do
				index="$((j * 2))"	# to avoid RSS text in the files for memory
				line1=$(awk "NR==${j}" ${cpu_before})
				line2=$(awk "NR==${j}" ${cpu_after})
				line3=$(awk "NR==${index}" ${memory_before})
				line4=$(awk "NR==${index}" ${memory_after})
				cpu_diff="$((line2 - line1))"
				memory_diff="$((line4 - line3))"
				printf "${cpu_diff} " >> "${files_cpu[k]}"
				printf "${memory_diff} " >> "${files_memory[k]}"	
			done
			printf "\n" >> "${files_cpu[k]}"
			printf "\n" >> "${files_memory[k]}"
		fi
	done
done

echo "Procucing the graphs..."
# only able to produce graphs for one directory --> cd/ mkdir not working properly
for directory in ${directories}; do
	cd ${directory}
	mkdir gnuplot

	echo > ${directory}/"gnuplot/make_gnuplot.dat"
	echo > ${directory}/"gnuplot/make_gnuplot_memory.dat"

	Rscript ${SCRIPT_DIR}/convert_to_gnuplot.R ${directory} "cpu_difference.dat" "${directory}/gnuplot" "make_gnuplot.dat"
	Rscript ${SCRIPT_DIR}/convert_to_gnuplot.R ${directory} "memory_difference.dat" "${directory}/gnuplot" "make_gnuplot_memory.dat"

	gnuplot -c ${SCRIPT_DIR}/plot_graph.plt "${directory}/gnuplot/make_gnuplot.dat" "${directory}/gnuplot/cpu_graph" 0.000001
	gnuplot -c ${SCRIPT_DIR}/plot_graph.plt "${directory}/gnuplot/make_gnuplot_memory.dat" "${directory}/gnuplot/memory_graph" 1
done

echo "The script is finished"
echo "The results can be found in ${dir}"