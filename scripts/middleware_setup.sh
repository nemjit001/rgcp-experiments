#! /bin/bash

./das/get_hostname.sh > ./tmp/rgcp_mw_ip
../../rgcp-middleware/bin/rgcp_middleware >> /dev/null
