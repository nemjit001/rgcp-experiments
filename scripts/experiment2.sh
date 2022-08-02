#! /bin/bash

RunLocal=1

if [ ! -z $1 ]
then
    RunLocal=$(($1))
fi

for i in `seq 5 5 30`
do
    ./rgcp_stab_test.sh $i $RunLocal 60 1000 3
done
