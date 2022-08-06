#! /bin/bash

echo "Starting Experiment 4"

for i in `seq 5 5 30`
do
    srun ./das/rgcp_tp_stab.sh $i > /dev/null
    sleep 1
done

echo "Experiment 4 Done"
