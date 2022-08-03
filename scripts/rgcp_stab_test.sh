#! /bin/bash

NumClients=5
Runtime=10
Cycles=1
ConnectProb=1000
OutputPrefix=""

RunLocal=1

function PrintHelp()
{
    echo "./rgcp_stab_test.sh <Number of Clients> <Time in Seconds> <Repeats> <Connection Probability> <Outputfile Prefix> <Run Locally>"
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
    NumClients=$(($1))
fi

if [ ! -z $2 ]
then
    Runtime=$(($2))
fi

if [ ! -z $3 ]
then
    Cycles=$(($3))
fi

if [ ! -z $4 ]
then
    ConnectProb=$(($4))
fi

if [ ! -z $5 ]
then
    OutputPrefix=$5
fi

if [ ! -z $6 ]
then
    RunLocal=$(($6))
fi

function DoLocalTest()
{
    if [ $# -ne 4 ]
    then
        exit 1
    fi

    local client_count=$1

    for j in `seq 1 $client_count`
    do
        echo "Starting Client" $j "out of" $client_count

        if [ "$j" -lt "$NumClients" ]
        then
            ../src/rgcp_stability_client $2 $3 $j STAB_TEST_$4 "127.0.0.1" "8000" >> /dev/null &
        else
            ../src/rgcp_stability_client $2 $3 $j STAB_TEST_$4 "127.0.0.1" "8000" >> /dev/null
        fi

        sleep .1
    done

    free -twm -c $2 -s 1 > ../out/${OutputPrefix}rgcp_stab_ram_$4\_$client_count\_$2 &
    mpstat -A -o JSON 1 $2 > ../out/${OutputPrefix}rgcp_stab_cpu_$4\_$client_count\_$2 &
}

function DoDasTest()
{
    if [ $# -ne 4 ]
    then
        exit 1
    fi

    srun ./das/rgcp_stab.sh $2 $3 $1 $4 >> /dev/null
}

for i in `seq 1 $Cycles`
do
    echo "Starting RGCP Stability Test" $i

    [ -d ../out/ ] || mkdir -p ../out/
    
    if [ $RunLocal -eq 1 ]
    then
        DoLocalTest $NumClients $Runtime $ConnectProb $i
    else
        DoDasTest $NumClients $Runtime $ConnectProb $i
    fi
done
