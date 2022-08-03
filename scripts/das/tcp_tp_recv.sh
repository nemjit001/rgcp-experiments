#! /bin/bash

./das/get_hostname.sh
../src/tcp_throughput recv >> /dev/null
