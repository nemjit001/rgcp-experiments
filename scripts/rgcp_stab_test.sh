#! /bin/bash

rm -r ../out/rgcp_stab_peers_*
NumClients=30
RunLocal=1

if [ ! -z $1 ]
then
    NumClients=$(($1))
fi

if [ ! -z $2 ]
then
    RunLocal=$(($2))
fi

function DoLocalTest()
{
    if [ $# -ne 1 ]
    then
        exit 1
    fi

    local client_count=$1
    
    for j in `seq 1 $client_count`
    do
        echo "Starting Client" $j "out of" $client_count

        if [ "$j" -lt "$NumClients" ]
        then
            ../src/rgcp_stability_client 60 1000 $j STAB_TEST_$i >> ../out/rgcp_stab_peers_$i/peer_$j &
        else
            ../src/rgcp_stability_client 60 1000 $j STAB_TEST_$i >> ../out/rgcp_stab_peers_$i/peer_$j
        fi

        sleep .1
    done
}

function DoDasTest()
{
    if [ $# -ne 1 ]
    then
        exit 1
    fi

    local client_count=$1

    for j in `seq 1 $client_count`
    do
        echo "Starting Das5 Client" $j "out of" $client_count
        srun ../src/rgcp_stability_client 60 1000 $j STAB_TEST_$i >> ../out/rgcp_stab_peers_$i/peer_$j
        sleep .1
    done
}

for i in {1..3}
do
    echo "Starting RGCP Stability Test" $i "(" $NumClients $RunLocal ")"

    [ -d ../out/rgcp_stab_peers_$i ] || mkdir -p ../out/rgcp_stab_peers_$i
    
    if [ $RunLocal -eq 1 ]
    then
        DoLocalTest $NumClients
    else
        DoDasTest $NumClients
    fi
done
