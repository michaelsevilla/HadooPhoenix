#!/bin/bash

sar -f logs/malloc_240GB.sar 2 > /dev/null 2>&1 &
sarpid=$!
echo $sarpid
./word_count /data2/Data/randomtextwriter-input_240GB 10

kill -9 $sarpid

