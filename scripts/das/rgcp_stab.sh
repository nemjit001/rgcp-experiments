#! /bin/bash

if [ $# -ne 4 ]
then
    exit 1
fi

run_time=$(($1))
probability=$(($2))
client_count=$(($3))
test_num=$(($4))

echo $run_time $probability $client_count $test_num

for j in `seq 1 $client_count`
do
    echo "Starting Client" $j "out of" $client_count

    ../src/rgcp_stability_client $run_time $probability $j STAB_TEST_$test_num $(cat ./tmp/rgcp_mw_ip) "8000" &# >> /dev/null &
    sleep .1
done

free -twm -c $run_time -s 1 > ../out/${OutputPrefix}rgcp_stab_ram_$test_num\_$client_count\_$run_time &
mpstat -A -o JSON 1 $run_time > ../out/${OutputPrefix}rgcp_stab_cpu_$test_num\_$client_count\_$run_time
