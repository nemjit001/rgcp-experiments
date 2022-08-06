#! /bin/bash

echo "Starting Experiment 3"

for i in `seq 5 5 30`
do
    srun ./rgcp_tp_test.sh $i 30 "" 1
    sleep 1
done

echo "Experiment 3 Done"
