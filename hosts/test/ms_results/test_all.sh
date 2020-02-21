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
				maxforceidle -a

				echo -n $res $size $task $typ " "
				export SCHED_MEASURE=$typ
				export SCHED_OFFLINE=$res
				export SCHED_TASKSIZE=$size
				task_mig_"$task"1 2>"$task"_"$res"_"$size".err >"$task"_"$res"_"$size".log
				echo $?
				
			done
		done
	done
done
