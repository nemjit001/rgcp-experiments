#! /bin/bash

ClientCount=5
Iterations=1
OutputPrefix=""

RunLocal=1

function PrintHelp()
{
    echo "./rgcp_tp_test.sh <Number of Clients> <Repeats> <Outputfile Prefix> <Run Locally>"
    exit 1
}

for param in "$@"
do
    if [[ $param == "help" ]]
    then 
        PrintHelp
    fi
done

if [ ! -z $1 ]
then
    ClientCount=$(($1))
fi

if [ ! -z $2 ]
then
    Iterations=$(($2))
fi

if [ ! -z $3 ]
then
    OutputPrefix=$3
fi

if [ ! -z $4 ]
then
    RunLocal=$(($4))
fi

function DoLocalTest()
{
    local client_count=$1
    local output_prefix=$3
    local test_num=$2

    ../src/rgcp_throughput setup ${client_count} $(cat ./tmp/rgcp_mw_ip) "8000"

    seq_count=`expr $client_count - 1`
    for j in `seq 1 ${seq_count}`
    do
        echo "Starting Recv Peer" $j
        ../src/rgcp_throughput recv ${client_count} $(cat ./tmp/rgcp_mw_ip) "8000" >> /dev/null &
        sleep .1
    done

    echo "Starting Send Peer"
    ../src/rgcp_throughput send ${client_count} $(cat ./tmp/rgcp_mw_ip) "8000" > ../out/${output_prefix}rgcp_tp_$test_num\_$client_count
    sleep 1
}

function DoDasTest()
{
    local client_count=$1
    local output_prefix=$3
    local test_num=$2

    srun ../src/rgcp_throughput setup ${client_count} $(cat ./tmp/rgcp_mw_ip) "8000"

    seq_count=`expr $client_count - 1`
    for j in `seq 1 ${seq_count}`
    do
        echo "Starting Recv Peer" $j
        srun ../src/rgcp_throughput recv ${client_count} $(cat ./tmp/rgcp_mw_ip) "8000" >> /dev/null &
        sleep .1
    done

    echo "Starting Send Peer"
    srun -o ../out/${output_prefix}rgcp_tp_$test_num\_$client_count ../src/rgcp_throughput send ${client_count} $(cat ./tmp/rgcp_mw_ip) "8000"
    sleep 1
}

for i in `seq 1 $Iterations`
do
    echo "Starting RGCP Throughput Test" $i

    [ -d ../out/ ] || mkdir -p ../out/

    if [ $RunLocal -eq 1 ]
    then
        DoLocalTest $ClientCount $i $OutputPrefix
    else
        DoDasTest $ClientCount $i $OutputPrefix  
    fi
done
