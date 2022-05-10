#! /bin/bash

rm ../out/tcp_tp
rm ../out/rgcp_tp

for i in {1..30}
do
    echo "Starting TCP Test" $i
    ../src/tcp_throughput recv >> /dev/null  &
    sleep .25
    ../src/tcp_throughput send 127.0.0.1 $i >> ../out/tcp_tp
done

for i in {1..3}
do
    echo "Starting RGCP Test" $i
    for j in {1..10}
    do
        ../src/rgcp_throughput recv 127.0.0.1 TP_GROUP_$i >> /dev/null &
    done

    sleep .25
    ../src/rgcp_throughput send 127.0.0.1 TP_GROUP_$i $i >> ../out/rgcp_tp
done
