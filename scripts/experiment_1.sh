#! /bin/bash

echo "tcp test"
../src/tcp_throughput recv > /dev/null  &
sleep 1
../src/tcp_throughput send 127.0.0.1 > ../out/tcp_tp &
echo "done"