#! /bin/bash

make --directory ./src/ clean all
export LD_LIBRARY_PATH=$HOME/libs/
echo set "lib path to" \"$LD_LIBRARY_PATH\"
