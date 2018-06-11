#!/bin/bash

# This script calculates diffs and aggregates results for a single experiment.
# This script is run for data transfer experiments by running "calculate_diffs_all.sh".

# Arguments:
# $1 - The directory containing all log files for a specific experiment
# $2 - If "1", force calculating diffs and aggregation of all results. Otherwise,
#      only calculate diffs and aggregate results if it has not yet been done

DIRECTORY=$1
FORCE_PARSE=$2
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
VMSTAT_Z_LIST_PATH=${SCRIPT_DIR}/vmstat_z_list
VMSTAT_M_LIST_PATH=${SCRIPT_DIR}/vmstat_m_list

DATA_DIR=data
DIFFS_DIR=diffs
AGGREGATES_DIR=aggregates
TABLES_DIR=tables

#
# The different kinds of accumulated data types
#

CPU_TOTAL_TYPES="application"
MEMORY_TOTAL_TYPES="application"

#
# Check that directory exists
#

if [ ! -d "$DIRECTORY" ]; then
    echo "Directory \"${DIRECTORY}\" does not exist! Skipping..."
    echo ""
    exit 1
fi

#
# Check whether error log is present
#

if [ $(find ${DIRECTORY} -maxdepth 1 -iname "error.log" | wc -w) -eq 1 ]; then
    echo "An error log is present! Experiment data are invalid! Skipping..."
    echo ""
    exit 1
fi

#
# If results have already been handled by this script, skip
#

if [ "${FORCE_PARSE}" == "0" ]; then
    if [ -d "${DIRECTORY}/${TABLES_DIR}" ]; then
	echo "Directory \"${DIRECTORY}\" already parsed! Skipping..."
	echo ""
	exit 1
    fi
fi

#
# Remove previous data produced by running this script
#

cd ${DIRECTORY}
rm -rf ${DIFFS_DIR}
rm -rf ${AGGREGATES_DIR}
rm -rf ${TABLES_DIR}

#
# Check whether experiment data is present
#

if [ $(find ${DIRECTORY} -maxdepth 1 -iname "*.log" | wc -w) -eq 0 ]; then
    echo "No experiment data found! Skipping..."
    exit 1
fi

prefixes=$(find ${DIRECTORY} -maxdepth 1 -type f -name '*.log' -printf '%P\n' | grep "_start_" | awk -F "_start" '{print $1}' | sort | uniq -c | awk -F " " '{print $2}')
experiment_count=$(echo $prefixes | wc -w)

calculate_diffs_delay () {
    if [ $(ls ${DIRECTORY}/${DATA_DIR} | grep "${1}" | wc -w) -eq 0 ]; then
	echo "No results data with pattern \"${1}\""
	return 0
    fi
    cd ${DIRECTORY}
    mkdir -p ${DIFFS_DIR}
    for prefix in ${prefixes}
    do
	relevant_log_files=$(find ${DIRECTORY}/${DATA_DIR} -maxdepth 1 -type f -name "${prefix}*${1}*" -printf '%P\n')
	for filename in ${relevant_log_files}
	do
	    file_length=$(cat ${DATA_DIR}/$filename | wc -l)
	    file_ending=$(echo "$filename" | awk -F "_stats_" '{print $2}')
	    for ((i = 1; i <= file_length; i++))
	    do
		total_seconds=$(cat ${DATA_DIR}/$filename | awk "NR==${i}"'{print $2}')
		total_nseconds=$(cat ${DATA_DIR}/$filename | awk "NR==${i}"'{print $3}')
		total=$((total_seconds*1000000000 + total_nseconds))
		echo "${total}" >> ${DIFFS_DIR}/"${prefix}_connection_${file_ending}"
	    done
	done
    done
}

calculate_diffs_samples () {
    if [ $(ls ${DIRECTORY}/${DATA_DIR} | grep "${1}" | wc -w) -eq 0 ]; then
	echo "No results data with pattern \"${1}\""
	return 0
    fi
    cd ${DIRECTORY}
    mkdir -p ${DIFFS_DIR}
    for prefix in ${prefixes}
    do
	relevant_log_files=$(find ${DIRECTORY}/${DATA_DIR} -maxdepth 1 -type f -name "${prefix}*${1}*" -printf '%P\n')
	newprefix=$(echo "$prefix" | awk -F "_" '{$11 = ""; $12 = ""; print}' | sed -r 's/ +/_/g')
	for filename in ${relevant_log_files}
	do
	    sample_number=$(echo "$filename" | awk -F "_" '{print $14}' | awk -F ":" '{print $2}')
	    newfile=$(echo "$filename" | awk -F "_" '{$11 = ""; $12 = ""; $14 = "samples"; print}' | sed -r 's/ +/_/g')
	    echo $((sample_number * 20)) $(cd ${DIRECTORY}/${DATA_DIR}; cat $filename) >> ${DIFFS_DIR}/${newfile}.tmp
	done
	relevant_log_files=$(find ${DIRECTORY}/${DIFFS_DIR} -maxdepth 1 -type f -name "${newprefix}*samples*.tmp" -printf '%P\n')
	for filename in ${relevant_log_files}
	do
	    lines=$(cd ${DIRECTORY}/${DIFFS_DIR}; cat $filename | wc -l)
	    for ((i = 0; i < $lines; i++))
	    do
		cd ${DIRECTORY}/${DIFFS_DIR}
		x_val=$((i * 20))
		cat $filename | awk "/^${x_val} /"' {print}' >> ${filename}.sorted
	    done
	done
	cd ${DIRECTORY}/${DIFFS_DIR}
	rm -rf *.tmp
	relevant_log_files=$(find ${DIRECTORY}/${DIFFS_DIR} -maxdepth 1 -type f -name "${newprefix}*samples*.tmp.sorted" -printf '%P\n')
	for filename in ${relevant_log_files}
	do
	    metric_type=$(echo $filename | awk -F "_" '{print $13}')
	    metric_specific=$(echo $filename | awk -F "_" '{print $14}' | awk -F "." '{print $1}')
	    newfile=$(echo ${filename::-11})
	    lines=$(cd ${DIRECTORY}/${DIFFS_DIR}; cat $filename | wc -l)

	    if [ "$metric_type" == "cpu" ]; then
		if [ "${metric_specific}" != "globalidle" ] && [ "${metric_specific}" != "globalusage" ]; then
		    cat $filename | awk '{i=i+1; if (i==1) val_prev=($2*1000000+int($3/1000)); if(i>1) {val_curr=($2*1000000+int($3/1000)); printf "%d %d\n", $1, val_curr-val_prev; val_prev=val_curr}}' >> ${newfile}
		else
		    cat $filename | awk '{i=i+1; if (i==1) val_prev=($2*1); if(i>1) {val_curr=($2*1); printf "%d %d\n", $1, val_curr-val_prev; val_prev=val_curr}}' >> ${newfile}
		fi
	    fi
	done
	cd ${DIRECTORY}/${DIFFS_DIR}
	rm -rf *.tmp.sorted
	relevant_log_files=$(find ${DIRECTORY}/${DIFFS_DIR} -maxdepth 1 -type f -name "${newprefix}*samples*" -printf '%P\n')
	for ((i = 1; i < $lines; i++))
	do
	    total_cpu_globalall=0
	    total_cpu_globalidle=0
	    total_cpu_all=0
	    total_cpu_nointr=0
	    total_cpu_nokernel=0
	    total_cpu_nokernelintr=0
	    total_cpu_idle=0
	    total_memory_all=0
	    total_memory_application=0
	    total_memory_network=0
	    x_val=$((i * 20))
	    cd ${DIRECTORY}/${DIFFS_DIR}
	    for filename in ${relevant_log_files}
	    do
		metric_type=$(echo $filename | awk -F "_" '{print $13}')
		metric_specific=$(echo $filename | awk -F "_" '{print $14}' | awk -F "." '{print $1}')
		val=$(cat $filename | awk "/^${x_val} /"' {print $2}')

	    	if [ "$metric_type" == "cpu" ]; then
		    for cpu_total_type in ${CPU_TOTAL_TYPES}
		    do
			if [ "${metric_specific}" == "idle" ]; then
			    total_cpu_idle=$((total_cpu_idle + val))
			fi
			if [ "${metric_specific}" == "globalusage" ]; then
			    total_cpu_globalall=$((total_cpu_globalall + val))
			fi
			if [ "${metric_specific}" == "globalidle" ]; then
			    total_cpu_globalidle=$((total_cpu_globalidle + val))
			fi
			if [ "${cpu_total_type}" == "all" ]; then
			    if [ "${metric_specific}" != "idle" ]; then
			        if [ "${metric_specific}" != "intr" ]; then
				    total_cpu_all=$((total_cpu_all + val))
				fi
			    fi
			elif [ "${cpu_total_type}" == "nointr" ]; then
			    if [ "${metric_specific}" != "idle" ]; then
				if [ "${metric_specific}" != "intr" ]; then
				    if [ "${metric_specific}" != "exp:intrps" ]; then
					total_cpu_nointr=$((total_cpu_nointr + val))
				    fi
				fi
			    fi
			elif [ "${cpu_total_type}" == "nokernel" ]; then
			    if [ "${metric_specific}" != "idle" ]; then
				if [ "${metric_specific}" != "intr" ]; then
				    if [ "${metric_specific}" != "kernel" ]; then
					total_cpu_nokernel=$((total_cpu_nokernel + val))
					if [ "${metric_specific}" != "exp:intrps" ]; then
					    total_cpu_nokernelintr=$((total_cpu_nokernelintr + val))
					fi
				    fi
				fi
			    fi
			else
			    echo "Invalid CPU total type: ${cpu_total_type}. Aborting..."
			    exit 1
			fi
		    done
		elif [ "$metric_type" == "memory" ]; then
		    for memory_total_type in ${MEMORY_TOTAL_TYPES}
		    do
			if [ "${memory_total_type}" == "all" ]; then
			    total_memory_all=$((total_memory_all + val))
			elif [ "${memory_total_type}" == "application" ]; then
			    if [ "${metric_specific}" == "application" ]; then
				total_memory_application=$((total_memory_application + val))
			    fi
			elif [ "${memory_total_type}" == "network" ]; then
			    if [ "${metric_specific}" != "application" ]; then
				total_memory_network=$((total_memory_network + val))
			    fi
			else
			    echo "Invalid CPU total type: ${cpu_total_type}. Aborting..."
			    exit 1
			fi
		    done
		fi
	    done
	    cd ${DIRECTORY}
	    # Global CPU
	    cpu_percentage=$(awk 'BEGIN {printf "%f", '"${total_cpu_globalall} / (${total_cpu_globalall} + ${total_cpu_globalidle})}")
	    echo $x_val ${cpu_percentage} >> ${DIFFS_DIR}/"${newprefix}_samples_cpu_all_global_percent.dat"
	    # CPU all data
	    cpu_percentage=$(awk 'BEGIN {printf "%f", '"${total_cpu_all} / (${total_cpu_all} + ${total_cpu_idle})}")
	    echo "$x_val $total_cpu_all" >> ${DIFFS_DIR}/"${newprefix}_samples_cpu_all_all.dat"
	    echo $x_val ${cpu_percentage} >> ${DIFFS_DIR}/"${newprefix}_samples_cpu_all_all_percent.dat"
	    # CPU ignore intr
	    cpu_percentage=$(awk 'BEGIN {printf "%f", '"${total_cpu_nointr} / (${total_cpu_nointr} + ${total_cpu_idle})}")
	    echo "$x_val $total_cpu_nointr" >> ${DIFFS_DIR}/"${newprefix}_samples_cpu_all_nointr.dat"
	    echo $x_val ${cpu_percentage} >> ${DIFFS_DIR}/"${newprefix}_samples_cpu_all_nointr_percent.dat"
	    # CPU ignore kernel
	    cpu_percentage=$(awk 'BEGIN {printf "%f", '"${total_cpu_nokernel} / (${total_cpu_nokernel} + ${total_cpu_idle})}")
	    echo "$x_val $total_cpu_nokernel" >> ${DIFFS_DIR}/"${newprefix}_samples_cpu_all_nokernel.dat"
	    echo $x_val ${cpu_percentage} >> ${DIFFS_DIR}/"${newprefix}_samples_cpu_all_nokernel_percent.dat"
	    # CPU ignore kernel and intr
	    cpu_percentage=$(awk 'BEGIN {printf "%f", '"${total_cpu_nokernelintr} / (${total_cpu_nokernelintr} + ${total_cpu_idle})}")
	    echo "$x_val $total_cpu_nokernelintr" >> ${DIFFS_DIR}/"${newprefix}_samples_cpu_all_nokernelintr.dat"
	    echo $x_val ${cpu_percentage} >> ${DIFFS_DIR}/"${newprefix}_samples_cpu_all_nokernelintr_percent.dat"
	    awk "BEGIN {print $x_val $total_memory_all/1000}" > ${DIFFS_DIR}/"${newprefix}_samples_memory_all_all.dat"
	    awk "BEGIN {print $x_val $total_memory_application/1000}" > ${DIFFS_DIR}/"${newprefix}_samples_memory_all_application.dat"
	    awk "BEGIN {print $x_val $total_memory_network/1000}" > ${DIFFS_DIR}/"${newprefix}_samples_memory_all_network.dat"
	done
    done
}

# Calculates diffs of two types of results
# $1: The "when" ID for the first point in time for which to calculate diffs
# $2: The "when" ID for the last point in time for which to calculate diffs
# $3: The new "when" ID describing which interval the diff was calculated
calculate_diffs () {
    if [ $(ls ${DIRECTORY}/${DATA_DIR} | grep "_${1}_" | wc -w) -eq 0 ]; then
	echo "No results data with pattern \"${1}\""
	return 0
    fi
    if [ $(ls ${DIRECTORY}/${DATA_DIR} | grep "_${2}_" | wc -w) -eq 0 ]; then
	echo "No results data with pattern \"${2}\""
	return 0
    fi
    cd ${DIRECTORY}
    mkdir -p ${DIFFS_DIR}
    # For all prefixes there is the same number of result types
    for prefix in ${prefixes}
    do
	# Total CPU usage between the two points in time
	total_cpu_all=0

	# Total CPU usage between the two points in time excluding the "intr" process
	total_cpu_nointr=0

	total_cpu_application=0

	# Total memory difference between the two points in time
	total_memory_all=0

	# Total memory difference between the two points in time for the application
	total_memory_application=0

	# Total memory difference between the two points in time for the network
	total_memory_network=0

	# All the sampled data for the first "when" ID
	first_files=$(find ${DIRECTORY}/${DATA_DIR} -maxdepth 1 -type f -name "${prefix}*_${1}_*" -printf '%P\n')

	# Go through the sampled data of the first "when" ID
	for first_file in ${first_files}
	do
	    # Total memory difference between the two points in time for a specific memory sample type
	    subtotal_memory=0

	    # The type of sampled data (e.g. delay, cpu, memory)
	    metric_type=$(echo $first_file | awk -F "_" '{print $17}')

	    # The specific source of sampled data (e.g. application, intr, vmstatz)
	    metric_specific=$(echo $first_file | awk -F "_" '{print $18}' | awk -F "." '{print $1}')

	    # The corresponding sampled data for the last "when" ID
            second_file=$(find ${DIRECTORY}/${DATA_DIR} -maxdepth 1 -type f -name "${prefix}*${2}_${metric_type}_${metric_specific}.dat" -printf '%P\n')

	    # The ending of files to be created (e.g. cpu_application.dat)
	    file_ending=$(echo "$first_file" | awk -F "_${1}_" '{print $2}')

	    if [ "$metric_type" == "memory" ]; then
		lines=$(cd ${DIRECTORY}/${DATA_DIR}; cat $first_file | wc -l)
		for ((i = 1; i <= $lines; i++))
		do
		    tmp=$(cd ${DIRECTORY}/${DATA_DIR}; awk -v line="$i" 'NR==line {print $2}' $first_file)
                    subtotal_memory=$((subtotal_memory + tmp))
		done
		first_val=$subtotal_memory
		subtotal_memory=0
		lines=$(cd ${DIRECTORY}/${DATA_DIR}; cat $second_file | wc -l)
		for ((i = 1; i <= $lines; i++))
		do
		    tmp=$(cd ${DIRECTORY}/${DATA_DIR}; awk -v line="$i" 'NR==line {print $2}' $second_file)
                    subtotal_memory=$((subtotal_memory + tmp))
		done
		second_val=$subtotal_memory
		total=$((second_val - first_val))
	    elif [ "$metric_type" == "cpu" ] || [ "$metric_type" == "delay" ] || [ "$metric_type" == "time" ]; then
		first_seconds=$(cd ${DIRECTORY}/${DATA_DIR}; cat $first_file | awk '{print $1}')
		first_nseconds=$(cd ${DIRECTORY}/${DATA_DIR}; cat $first_file | awk '{print $2}')
		second_seconds=$(cd ${DIRECTORY}/${DATA_DIR}; cat $second_file | awk '{print $1}')
		second_nseconds=$(cd ${DIRECTORY}/${DATA_DIR}; cat $second_file | awk '{print $2}')

		if (( (second_nseconds - first_nseconds) < 0)); then
		    total_seconds=$((second_seconds - first_seconds - 1))
		    total_nseconds=$((second_nseconds - first_nseconds + 1000000000))
		else
		    total_seconds=$((second_seconds - first_seconds))
		    total_nseconds=$((second_nseconds - first_nseconds))
		fi
		total=$((total_seconds*1000000000 + total_nseconds))
	    else
		echo "Invalid sample type: ${metric_type}. Aborting..."
		exit 1
	    fi

	    # Output the difference for the sample type to file
	    echo "$total" > ${DIFFS_DIR}/"${prefix}_${3}_${file_ending}"

	    # Add the CPU usage or memory difference of the current sample type to the total of all samples
	    if [ "$metric_type" == "cpu" ]; then
		if [ "${metric_specific}" != "globalusage" ] && [ "${metric_specific}" != "globalidle" ]; then
		    for cpu_total_type in ${CPU_TOTAL_TYPES}
		    do
			if [ "${cpu_total_type}" == "all" ]; then
			    if [ "${metric_specific}" != "idle" ]; then
				total_cpu_all=$((total_cpu_all + total))
			    fi
			elif [ "${cpu_total_type}" == "nointr" ]; then
			    if [ "${metric_specific}" != "idle" ]; then
				if [ "${metric_specific}" != "intr" ]; then
				    total_cpu_nointr=$((total_cpu_nointr + total))
				fi
			    fi
                        elif [ "${cpu_total_type}" == "application" ]; then
			    if [ "${metric_specific}" == "application" ]; then
				total_cpu_application=$((total_cpu_application + total))
			    fi
			else
			    echo "Invalid CPU total type: ${cpu_total_type}. Aborting..."
			    exit 1
			fi
		    done
		fi
	    elif [ "$metric_type" == "memory" ]; then
		for memory_total_type in ${MEMORY_TOTAL_TYPES}
		do
		    if [ "${memory_total_type}" == "all" ]; then
			total_memory_all=$((total_memory_all + total))
		    elif [ "${memory_total_type}" == "application" ]; then
			if [ "${metric_specific}" == "application" ]; then
			    total_memory_application=$((total_memory_application + total))
			fi
		    elif [ "${memory_total_type}" == "network" ]; then
			if [ "${metric_specific}" != "application" ]; then
			    total_memory_network=$((total_memory_network + total))
			fi
		    else
			echo "Invalid CPU total type: ${cpu_total_type}. Aborting..."
			exit 1
		    fi
		done
	    fi
	done
	echo "$total_cpu_all" > ${DIFFS_DIR}/"${prefix}_${3}_cpu_all_all.dat"
	echo "$total_cpu_nointr" > ${DIFFS_DIR}/"${prefix}_${3}_cpu_all_nointr.dat"
	echo "$total_cpu_application" > ${DIFFS_DIR}/"${prefix}_${3}_cpu_all_application.dat"
	awk "BEGIN {print $total_memory_all/1000}" > ${DIFFS_DIR}/"${prefix}_${3}_memory_all_all.dat"
	awk "BEGIN {print $total_memory_application/1000}" > ${DIFFS_DIR}/"${prefix}_${3}_memory_all_application.dat"
	awk "BEGIN {print $total_memory_network/1000}" > ${DIFFS_DIR}/"${prefix}_${3}_memory_all_network.dat"
    done
}

aggregate_results () {
    if [ $(ls ${DIRECTORY}/${DIFFS_DIR} | grep "_${1}_" | wc -w) -eq 0 ]; then
	echo "No diffs with pattern \"${2}\""
	return 0
    fi
    cd ${DIRECTORY}
    mkdir -p ${AGGREGATES_DIR}
    for prefix in ${prefixes}
    do
	files=$(find ${DIRECTORY}/${DIFFS_DIR} -maxdepth 1 -type f -name "${prefix}*_${1}_*" -printf '%P\n')
	for filename in ${files}
	do
	    # Remove run number from the results filename
	    newfile=$(echo "$filename" | awk -F "_" '{$13 = ""; $14 = ""; print}' | sed -r 's/ +/_/g')
	    cd ${DIRECTORY}
	    cat ${DIFFS_DIR}/${filename} >> ${AGGREGATES_DIR}/${newfile}
	done
    done
}

produce_table_transfer () {
    if [ $(ls ${DIRECTORY}/${AGGREGATES_DIR} | grep "_${1}_" | grep "${2}" | wc -w) -eq 0 ]; then
	echo "No aggregated data with both pattern \"${1}\" and \"${2}\""
	return 0
    fi
    cd ${DIRECTORY}
    mkdir -p ${TABLES_DIR}
    files_to_paste=""
    header=""
    maxlength=0
    flow_numbers=$(find ${DIRECTORY} -maxdepth 1 -type f -name '*.log' -printf '%P\n' | grep "_start_" | awk -F "_" '{print $4}' | sort | uniq -c | awk -F " " '{print $2}')
    flow_numbers_count=$(echo $flow_numbers | wc -w)

    for flow_number in $flow_numbers
    do
	relevant_files=$(find ${DIRECTORY}/${AGGREGATES_DIR} -maxdepth 1 -type f -name "*.dat" -printf '%P\n' | grep "_${1}_" | grep "${2}" | grep "_flows_${flow_number}_")
	for filename in $relevant_files
	do
	    flows=$(echo $filename | awk -F "_" '{print $4}')
	    dsize=$(echo $filename | awk -F "_" '{print $8}')
	    dsize_new=$(awk "BEGIN {printf \"%d\", $dsize / 1000}")
	    files_to_paste=$(echo $files_to_paste ${DIRECTORY}/${AGGREGATES_DIR}/$filename)
	    header=$(echo $header D${dsize_new})
	    filelength=$(cat ${DIRECTORY}/${AGGREGATES_DIR}/${filename} | wc -l)
	    if [ $filelength -gt $maxlength ]; then
		maxlength=$filelength
	    fi
	done
	for filename in $relevant_files
	do
	    filelength=$(cat ${DIRECTORY}/${AGGREGATES_DIR}/${filename} | wc -l)
	    if [ $filelength -lt $maxlength ]; then
		difference=$((maxlength - filelength))
		for ((i = 0; i < difference; i++))
		do
		    echo "NA" >> ${DIRECTORY}/${AGGREGATES_DIR}/${filename}
		done
	    fi
	done
	content=$(paste $files_to_paste)
	echo -e "${header}\n${content}" > ${TABLES_DIR}/${1}_${flow_number}_${2}.tbl
	files_to_paste=""
	header=""
	maxlength=0
    done
}

produce_table () {
    if [ $(ls ${DIRECTORY}/${AGGREGATES_DIR} | grep "_${1}_" | grep "${2}" | wc -w) -eq 0 ]; then
	echo "No aggregated data with both pattern \"${1}\" and \"${2}\""
	return 0
    fi
    relevant_files=$(find ${DIRECTORY}/${AGGREGATES_DIR} -maxdepth 1 -type f -name "*.dat" -printf '%P\n' | grep "_${1}_" | grep "${2}")
    cd ${DIRECTORY}
    mkdir -p ${TABLES_DIR}
    files_to_paste=""
    header=""
    maxlength=0

    for filename in $relevant_files
    do
	flows=$(echo $filename | awk -F "_" '{print $4}')
	dsize=$(echo $filename | awk -F "_" '{print $6}')
	files_to_paste=$(echo $files_to_paste ${DIRECTORY}/${AGGREGATES_DIR}/$filename)
	header=$(echo $header F${flows})
	filelength=$(cat ${DIRECTORY}/${AGGREGATES_DIR}/${filename} | wc -l)
	if [ $filelength -gt $maxlength ]; then
	    maxlength=$filelength
	fi
    done
    for filename in $relevant_files
    do
	filelength=$(cat ${DIRECTORY}/${AGGREGATES_DIR}/${filename} | wc -l)
	if [ $filelength -lt $maxlength ]; then
	    difference=$((maxlength - filelength))
	    for ((i = 0; i < difference; i++))
	    do
		echo "NA" >> ${DIRECTORY}/${AGGREGATES_DIR}/${filename}
	    done
	fi
    done
    content=$(paste $files_to_paste)
    echo -e "${header}\n${content}" > ${TABLES_DIR}/${1}_${2}.tbl
}

echo "Calculating diffs..."
echo "Calculating data sending diffs (if client)..."
calculate_diffs beforefirstwrite afterallwritten transfer
echo "Calculating data receiving diffs (if server)..."
calculate_diffs beforefirstread afterallread transfer

echo "Aggregate results..."
aggregate_results transfer

echo "Producing tables.."
produce_table_transfer transfer cpu_all_application
#produce_table_transfer transfer memory_all_application
#produce_table_transfer transfer time_application
echo ""
