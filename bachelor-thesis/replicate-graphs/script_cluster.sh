#!/bin/bash

TEST_MODE=$1 #"connection" "memory" "cpu"

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
DIRECTORY="/root/results-neat-test-suite"
directories=("${DIRECTORY}/client/tcp" "${DIRECTORY}/server/tcp")
number_of_tests=10
flows=(1 2 4 8 16 32 64 128 256)
transport=("TCP" "1" "SCTP" "1")

host="192.168.10.2"
client_address="192.168.10.3"
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

echo "This script will sample the CPU and memory usage"

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
				# SCTP:
				b="${flows[i]}"
				a="0"
		fi
		for (( j = 0; j < "${number_of_tests}"; j++ )); do
			echo "number of flows: ${flows[i]}, test $((j + 1))..."
		
			ssh ${server} "cd neat-test-suite/build ; ./neat_server -C $((transport[k+1] * flows[i])) -a ${a} -b ${b} \
				-M ${transport[k]} -I ${host} -p ${port} -v ${log_level} -u ${log_level} \
				&>/dev/null 2>&1" & #&>>/root/output_server.txt 2>&1 & #2>&1 &

			sleep 1

			ssh ${client} "cd neat-test-suite/build ; ./neat_client -s -R ${directories[k]} -A -C ${flows[i]} -a ${a} -b ${b} \
				-M ${transport[k]} -i ${client_address} -n ${flows[i]} -v ${log_level} -u ${log_level} ${host} \
				${port} ${counter} &>/dev/null" & #&>>/root/output_client.txt & #&

			#wait before the programs are killed 
			if [[ ${TEST_MODE} == "connection" || ${TEST_MODE} == "cpu" ]]; then
				sleep 5
			else
				if (( ${flows[i]} <= 8 )); then
					sleep 5
				elif [[ ${flows[i]} -eq 16 || ${flows[i]} -eq 32 ]]; then
					sleep 20 
				elif [[ ${flows[i]} -eq 64 ]]; then
					sleep 30  
				elif [[ ${flows[i]} -eq 128 ]]; then
					sleep 1m 
				else
					sleep 4m 
				fi
			fi

			ssh ${server} "cd neat-test-suite/build ; killall ./neat_server"
			ssh ${client} "cd neat-test-suite/build ; killall ./neat_client"
			
			if [[ ${TEST_MODE} != "connection" ]]; then
				echo "Add jsondata together and relocate it to a new file..."
				ssh ${client} "cd neat-test-suite/build ; ./add_jsondata.sh "${directories[k]}" "$((i + 1))" "${TEST_MODE}""
			fi
		done
		counter="$((counter+1))"
	done
done

echo "The script is finished"
echo "The results can be found in ${DIRECTORY}"
