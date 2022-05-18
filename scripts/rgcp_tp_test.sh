#! /bin/bash

rm ../out/rgcp_tp

for i in {1..3}
do
    echo "Starting RGCP Test" $i

    sleep .25
    for j in {1..10}
    do
        ../src/rgcp_throughput recv 127.0.0.1 TP_GROUP_$i 10 >> /dev/null &
    done

    ../src/rgcp_throughput send 127.0.0.1 TP_GROUP_$i 10 $i
done
