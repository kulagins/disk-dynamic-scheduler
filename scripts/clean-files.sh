#!/bin/bash

# Check if a directory path is provided as an argument
if [ -z "$1" ]; then
    echo "Please provide a path to the parent directory."
    exit 1
fi

parent_dir="$1"

# Check if the provided directory exists
if [ ! -d "$parent_dir" ]; then
    echo "The directory '$parent_dir' does not exist."
    exit 1
fi



# Loop through each subfolder in the provided directory
for dir in "$parent_dir"/*; do
    # Only proceed if it's a directory
    if [ -d "$dir" ]; then
        # Create a merged file for this subfolder
        merged_file="${dir%/}_merged.txt"
        find . -type f -name "*.status" -delete

        txt_files=$(find "$dir" -type f -name "*.out")
        echo $txt_files
                if [ -n "$txt_files" ]; then
                    # Loop through all text files found and append them to the merged file
                    for file in $txt_files; do
                        cat "$file" >> "$merged_file"
                    done
                else
                    echo "No .out files found in $dir"
                fi
    fi
done

 mkdir -p merged
cp *merged* merged/
