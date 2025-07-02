#!/bin/bash
pwd

BINARY="./build/fonda_scheduler"
COMMON_ARGS="-m 1000000 -s 100 -r 1 -o 0.001 -a heft-bl -f input/machines.csv -d 2 -S"

# Run experiments with different workloads
WORKLOADS=(
    "atacseq_200 2223941232"
    "atacseq_2000 2223941232"
    "atacseq_8000 2223941232"
    "atacseq_15000 2223941232"
    "bacass_200 3793245764"
    "bacass_2000 3793245764"
    "bacass_8000 3793245764"
    "bacass_15000 3793245764"
)

for workload in "${WORKLOADS[@]}"; do
    IFS=' ' read -r -a params <<< "$workload"
    COMMAND="$BINARY ${COMMON_ARGS} -w ${params[0]} -i ${params[1]}"
    echo "Running command: $COMMAND"
    $COMMAND
done