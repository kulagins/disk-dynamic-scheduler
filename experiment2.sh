#!/bin/bash
pwd

for workflow in \
    "chipseq_200 3793245764" "chipseq_2000 3793245764" "chipseq_8000 3793245764" "chipseq_15000 3793245764" \
    "atacseq_200 2223941232" "atacseq_2000 2223941232" "atacseq_8000 2223941232" "atacseq_15000 2223941232" \
    "eager_200 25705994498" "eager_2000 25705994498" "eager_8000 25705994498" "eager_15000 25705994498" \
    "methylseq_200 17027257018" "methylseq_2000 17027257018" "methylseq_8000 17027257018" "methylseq_15000 17027257018"
do
    set -- $workflow  # Splits the string into separate arguments
    ./fonda_scheduler 1000000 100 1 0.001 "$1" "$2" 1 no ../ machines.csv
    echo "Press any key to continue..."
    read -n 1 -s  # Waits for a single key press
done

