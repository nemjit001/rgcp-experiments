#! /bin/bash

rm ../out/rgcp_tp

for i in {1..1}
do
    echo "Starting RGCP Test" $i
    
    for j in {1..10}
    do
        ../src/rgcp_throughput recv 127.0.0.1 TP_GROUP_$i 10 &# >> /dev/null &
        sleep .25
    done

    ../src/rgcp_throughput send 127.0.0.1 TP_GROUP_$i 10 $i
done
