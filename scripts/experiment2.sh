#! /bin/bash

echo "Starting Experiment 2"

for i in `seq 5 5 30`
do
    ./rgcp_stab_test.sh $i 60 3 1000 "" 0
    sleep 1
done

echo "Experiment 2 Done"
