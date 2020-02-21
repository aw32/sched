#!/bin/bash

if [ "$TASKLIST" = "" ]; then
TASKLIST="correlation heat bfs markov gaussblur"
fi

if [ "$TYPE" = "" ]; then
TYPE="time energy"
fi

if [ "$RESLIST" = "" ]; then
RESLIST="IntelXeon MaxelerVectis NvidiaTesla"
fi

if [ "$SIZELIST" = "" ]; then
SIZELIST="128
256
512
1024
2048
4096
8192
16384"
fi

export LD_PRELOAD=/usr/ampehre/lib/libms_common_apapi.so
export LD_LIBRARY_PATH=/usr/local/cuda-7.0/targets/x86_64-linux/lib/:/opt/pgi/linux86-64/18.4/lib/
export CUDA_HOME=/usr/local/cuda-7.0/


for size in $SIZELIST; do
	for task in $TASKLIST; do
		for res in $RESLIST; do
			for typ in $TYPE; do
				# exceptions
				#if [ "$task" = "bfs" && "$res" = "MaxelerVectis" ]; then
				#	echo -n $res $size $task $typ " not supported"
				#	continue
				#fi

if [ ! -f "ms_$task($size)@"$res"_$typ.csv" ]; then
	echo "ms_$task($size)@"$res"_$typ.csv" is missing
fi
#ms_correlation(2048)@IntelXeon_time.csv
				
			done
		done
	done
done
