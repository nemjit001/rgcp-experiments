#! /bin/bash

rm ../out/tcp_tp

for i in {1..30}
do
    echo "Starting TCP Test" $i
    ../src/tcp_throughput recv >> /dev/null  &
    sleep .25
    ../src/tcp_throughput send 127.0.0.1 $i >> ../out/tcp_tp
done
