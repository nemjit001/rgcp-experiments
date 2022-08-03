#! /bin/bash

echo "Starting Experiment 1"

./tcp_tp_test.sh
sleep 1
./rgcp_tp_test.sh 2 30

echo "Experiment 1 Done"
