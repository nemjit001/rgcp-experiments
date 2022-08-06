#! /bin/bash

client_count=$(($1))

for j in {1..10}
do
    ../src/rgcp_stability_client 120 1000 $j RGCP_TP_TEST $(cat ./tmp/rgcp_mw_ip) "8000" > /dev/null &
    sleep .1
done

./rgcp_tp_test.sh $client_count 30 "exp4_" 1 > /dev/null

sleep 120
