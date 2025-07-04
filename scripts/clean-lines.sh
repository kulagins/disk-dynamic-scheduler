#!/bin/bash

# Check if a directory path is provided as an argument
if [ -z "$1" ]; then
    echo "Please provide a path to the directory."
    exit 1
fi

cd "$1" || exit
pwd

sed -i 's/\t/ /g' *.txt
sed -i 's/![^ ]* //' *.txt
sed -i 's/  / /g' *.txt

sed -i ':a;N;$!ba;s/[[:space:]]*\n[[:space:]]*\(duration_of_algorithm\)/ \1/g' *.txt
sed -Ei 's/.*(algo_nr)/\1/' *.txt

sed -i 's/ input_size//g' *.txt
sed -i 's/ makespan_1//g' *.txt
sed -i 's/ makespan_2//g' *.txt
sed -i 's/ duration_of_algorithm//g' *.txt
sed -i 's/ ms perceived//g' *.txt
sed -i 's/algo_nr //g' *.txt


sed -i '1i algo_nr wf_name inp_size dur_alg1 ms_1 ms_perc dur_alg2 ms_2' *.txt
sed -i 's/  \+/ /g' *.txt
