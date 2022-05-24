#! /bin/bash

rm -r ../out/rgcp_stab_peers_*
NumClients=30

for i in {1..3}
do
    echo "Starting RGCP Stability Test" $i

    [ -d ../out/rgcp_stab_peers_$i ] || mkdir -p ../out/rgcp_stab_peers_$i
    
    for j in `seq 1 $NumClients`
    do
        echo "Starting Client" $j "out of" $NumClients

        if [ "$j" -lt "$NumClients" ]
        then
            ../src/rgcp_stability_client 60 1000 $j STAB_TEST_$i >> ../out/rgcp_stab_peers_$i/peer_$j &
        else
            ../src/rgcp_stability_client 60 1000 $j STAB_TEST_$i >> ../out/rgcp_stab_peers_$i/peer_$j
        fi

        sleep .1
    done
done
