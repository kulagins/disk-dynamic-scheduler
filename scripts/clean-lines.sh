#!/bin/bash

# Check if a directory path is provided as an argument
if [ -z "$1" ]; then
    echo "Please provide a path to the directory."
    exit 1
fi

cd "$1" || exit
pwd

sed -i '/^\.\/build/d' *.txt
sed -i '/^\.\/build/d' *.txt
sed -i ':a;N;$!ba;s/\n\(WILL BE INVALID.*\)/ \1/' *.txt


sed -i 's/^new, algo [0-9]\+ //' *.txt
sed -i 's/WILL BE INVALID.*/invalid/' *.txt
#sed -i '1i workflow_name duration_of_prep #eviction duration_of_algorithm makespan' *.txt
#sed -i 's/^\(\S\+\) \1/\1/' *.txt
sed -i 's/ Invalid assignment.*/invalid/' *.txt
sed -i 's/\([0-9]\)\(invalid\)/\1 \2/g' *.txt
sed -i 's/ duration_of_prep//g' *.txt
sed -i 's/ #eviction//g' *.txt
sed -i 's/ duration_of_algorithm//g' *.txt
sed -i 's/ makespan//g' *.txt
sed -i 's/workflow_name//g' *.txt
sed -i '1i workflow_name duration_of_prep #eviction duration_of_algorithm makespan' *.txt
sed -i '/^\s*$/d; /^\s*\S\+\s*$/d' *.txt
sed -i 's/  \+/ /g' *.txt
