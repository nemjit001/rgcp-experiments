#! /bin/bash

rm ../out/tcp_tp

RunLocal=1

function PrintHelp()
{
    echo "./tcp_tp_test.sh <Run Locally>"
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
    RunLocal=$(($1))
fi

for i in {1..500}
do
    echo "Starting TCP Test" $i

    [ -d ../out/ ] || mkdir -p ../out/
    [ -d ./tmp/ ] || mkdir -p ./tmp/

    if [ $RunLocal -eq 1 ]
    then
        ../src/tcp_throughput recv >> /dev/null  &
        sleep .1
        ../src/tcp_throughput send 127.0.0.1 $i >> ../out/tcp_tp
    else
        srun ./das/tcp_tp_recv.sh >> /dev/null &
        sleep .1
        srun ../src/tcp_throughput send $(cat ./tmp/tcp_host_ip) >> ../out/tcp_tp
    fi
done
