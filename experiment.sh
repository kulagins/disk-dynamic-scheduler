#!/bin/bash
pwd
#!/bin/bash

echo "Arguments passed to the script: $*"

port=$((RANDOM % 9000 + 1000))

echo "Random port number: $port"

./build/fonda_scheduler $port &
server_pid=$!

# Check if the scheduler ran successfully
if [ $? -ne 0 ]; then
    echo "fonda_scheduler failed"
    exit 1
fi

# Sleep for 2 seconds before running the Python script
sleep 2


cd ../runtime-system || exit

# Run the python script with all the arguments passed to this bash script
echo "Executing: python -m gear $*"
pwd
python -m gear "$@" -p $port

#Check if the python script ran successfully
if [ $? -ne 0 ]; then
    echo "Python script failed"
    exit 1
fi

kill $server_pid


echo "Both executables ran successfully."