#! /bin/bash

ClientCount=5
Iterations=1

if [ ! -z $1 ]
then
    ClientCount=$(($1))
fi

if [ ! -z $2 ]
then
    Iterations=$(($2))
fi

for i in `seq 1 $Iterations`
do
    echo "Starting RGCP Throughput Test" $i

    [ -d ../out/tp_peers_$i\_$ClientCount ] || mkdir -p ../out/tp_peers_$i\_$ClientCount

    ../src/rgcp_throughput setup ${ClientCount} "127.0.0.1" "8000"

    SeqCount=`expr $ClientCount - 1`
    for j in `seq 1 ${SeqCount}`
    do
        echo "Starting Recv Peer" $j
        ../src/rgcp_throughput recv ${ClientCount} "127.0.0.1" "8000" > ../out/tp_peers_$i\_$ClientCount/peer_$j &
        sleep .1
    done

    echo "Starting Send Peer"
    ../src/rgcp_throughput send ${ClientCount} "127.0.0.1" "8000" > ../out/rgcp_tp_$i\_$ClientCount
    sleep 1
done
