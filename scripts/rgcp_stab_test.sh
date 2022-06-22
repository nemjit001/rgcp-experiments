#! /bin/bash

rm -r ../out/rgcp_stab_peers_*
NumClients=5
RunLocal=1
Runtime=10
ConnectProb=1000
Cycles=1

if [ ! -z $1 ]
then
    NumClients=$(($1))
fi

if [ ! -z $2 ]
then
    RunLocal=$(($2))
fi

if [ ! -z $3 ]
then
    Runtime=$(($3))
fi

if [ ! -z $4 ]
then
    ConnectProb=$(($4))
fi

if [ ! -z $5 ]
then
    Cycles=$(($5))
fi

function DoLocalTest()
{
    if [ $# -ne 3 ]
    then
        exit 1
    fi

    local client_count=$1
    
    for j in `seq 1 $client_count`
    do
        echo "Starting Client" $j "out of" $client_count

        if [ "$j" -lt "$NumClients" ]
        then
            ../src/rgcp_stability_client $2 $3 $j STAB_TEST_$i >> ../out/rgcp_stab_peers_$i/peer_$j &
        else
            ../src/rgcp_stability_client $2 $3 $j STAB_TEST_$i >> ../out/rgcp_stab_peers_$i/peer_$j
        fi

        sleep .1
    done
}

function DoDasTest()
{
    if [ $# -ne 3 ]
    then
        exit 1
    fi

    local client_count=$1

    for j in `seq 1 $client_count`
    do
        echo "Starting Das5 Client" $j "out of" $client_count
        srun ../src/rgcp_stability_client $2 $3 $j STAB_TEST_$i >> ../out/rgcp_stab_peers_$i/peer_$j
        sleep .1
    done
}

for i in `seq 1 $Cycles`
do
    echo "Starting RGCP Stability Test" $i "(" $NumClients $RunLocal ")"

    [ -d ../out/rgcp_stab_peers_$i ] || mkdir -p ../out/rgcp_stab_peers_$i
    
    if [ $RunLocal -eq 1 ]
    then
        DoLocalTest $NumClients $Runtime $ConnectProb
    else
        DoDasTest $NumClients $Runtime $ConnectProb
    fi
done
