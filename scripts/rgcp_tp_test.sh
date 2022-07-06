#! /bin/bash

rm ../out/rgcp_tp_*
rm ../out/peers/*

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

    ../src/rgcp_throughput setup
    echo "Starting Send Peer"
    ../src/rgcp_throughput send ${ClientCount} &#> ../out/rgcp_tp_$i &
    sleep .1

    SeqCount=`expr $ClientCount - 1`
    for j in `seq 1 ${SeqCount}`
    do
        echo "Starting Recv Peer" $j
        ../src/rgcp_throughput recv ${ClientCount} > ../out/peers/peer_$j &
        sleep .1
    done
done
