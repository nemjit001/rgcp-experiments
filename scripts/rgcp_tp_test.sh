#! /bin/bash

rm ../out/rgcp_tp
rm ../out/peers/*

RunLocal=1
ClientCount=5
Iterations=1

if [ ! -z $1 ]
then
    RunLocal=$(($1))
fi

if [ ! -z $2 ]
then
    ClientCount=$(($2))
fi

if [ ! -z $3 ]
then
    Iterations=$(($3))
fi

function DoLocalTest()
{
    if [ $# -ne 2 ]
    then
        exit 1
    fi

    for j in `seq 1 $1`
    do
        echo "Starting Client" $j
        ../src/rgcp_throughput recv 127.0.0.1 TP_GROUP_$2 $j > ../out/peers/peer_out_$j &
        
        sleep .1
    done
}

function DoDasTest()
{
    if [ $# -ne 2 ]
    then
        exit 1
    fi

    for j in `seq 1 $1`
    do
        echo "Starting Client" $j
        srun ../src/rgcp_throughput recv 127.0.0.1 TP_GROUP_$2 $j > ../out/peers/peer_out_$j &
        
        sleep .1
    done
}

for i in `seq 1 $Iterations`
do
    echo "Starting RGCP Throughput Test" $i

    if [ $RunLocal -eq 1 ]
    then
        DoLocalTest $ClientCount $i
    else
        DoDasTest $ClientCount $i
    fi

    sleep .1

    if [ $RunLocal -eq 1 ]
    then
        ../src/rgcp_throughput send 127.0.0.1 TP_GROUP_$i $ClientCount $i # >> ../out/rgcp_tp
    else
        srun ../src/rgcp_throughput send 127.0.0.1 TP_GROUP_$i $ClientCount $i
    fi
done
