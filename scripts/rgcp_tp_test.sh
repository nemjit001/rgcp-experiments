#! /bin/bash

rm ../out/rgcp_tp
rm ../out/peers/*

for i in {1..1}
do
    echo "Starting RGCP Throughput Test" $i
    
    for j in {1..5}
    do
        echo "Starting Client" $j
        
        if j > 1
        then
            ../src/rgcp_throughput recv 127.0.0.1 TP_GROUP_$i 5 > ../out/peers/peer_out_$j &
        else
            ../src/rgcp_throughput recv 127.0.0.1 TP_GROUP_$i 5&
        fi
        
        sleep .1
    done

    sleep .1

    ../src/rgcp_throughput send 127.0.0.1 TP_GROUP_$i 5 $i # >> ../out/rgcp_tp
done
