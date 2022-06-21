#! /bin/bash

rm ../out/tcp_tp

RunLocal=1

if [ ! -z $1 ]
then
    RunLocal=$(($1))
fi

for i in {1..30}
do
    echo "Starting TCP Test" $i

    if [ $RunLocal -eq 1 ]
    then
        ../src/tcp_throughput recv >> /dev/null  &
        sleep .1
        ../src/tcp_throughput send 127.0.0.1 $i >> ../out/tcp_tp
    else
        srun ../src/tcp_throughput recv >> /dev/null  &
        srun ../src/tcp_throughput send 127.0.0.1 $i >> ../out/tcp_tp &
    fi
done
