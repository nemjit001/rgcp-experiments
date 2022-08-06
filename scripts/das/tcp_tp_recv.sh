#! /bin/bash

./das/get_hostname.sh > ./tmp/tcp_host_ip
../src/tcp_throughput recv >> /dev/null
