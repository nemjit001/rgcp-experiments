#! /bin/bash

echo "Starting Experiment 1"

./tcp_tp_test.sh 1
sleep 1
./rgcp_tp_test.sh 2 30 "" 1

echo "Experiment 1 Done"
