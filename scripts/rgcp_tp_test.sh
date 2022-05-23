#! /bin/bash

rm ../out/rgcp_tp

for i in {1..1}
do
    echo "Starting RGCP Test" $i
    
    for j in {1..5}
    do
        ../src/rgcp_throughput recv 127.0.0.1 TP_GROUP_$i 5 > ../out/peers/peer_out_$j &
        sleep .1
    done

    ../src/rgcp_throughput send 127.0.0.1 TP_GROUP_$i 5 $i >> ../out/rgcp_tp
done
