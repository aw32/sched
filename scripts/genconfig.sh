#!/bin/bash
# Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
# SPDX-License-Identifier: BSD-2-Clause


function getrand() {
	dd if=/dev/urandom count=4 bs=1 2>/dev/null | od -A n -t u
}

# path to this script, used for default directories
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

if [ "$SCHED_ENV" == "" ]; then
	echo "SCHED_ENV not defined"
	exit 1
fi

if [ "$SCHED_HOST" == "" ]; then
	echo "SCHED_HOST not defined"
	exit 1
fi

# load preconfiguration
if [ "$SCHED_PRECONF" != "" ]; then
	source "$SCHED_ENV/preconf/$SCHED_PRECONF"
fi


if [ "$RES" == "" ]; then
RES="IntelXeon NvidiaTesla MaxelerVectis"
fi

if [ "$SCHEDULER" == "" ]; then
SCHEDULER="Linear"
fi

if [ "$RESOURCE_TASKENDHOOK" == "" ]; then
RESOURCE_TASKENDHOOK="$DIR/endtaskhook.sh"
fi

if [ "$GENETIC_SEED" == "" ]; then
GENETIC_SEED="$(getrand)"
fi

if [ "$GENETICMIG_SOLVER" == "" ]; then
GENETICMIG_SOLVER="$SCHED_ENV/scripts/lp_solve.sh"
fi

if [ "$LP_DESTINATION" == "" ]; then
LP_DESTINATION="$SCHED_ENV"
fi

if [ "$SA_RATIO_LOWER" == "" ]; then
SA_RATIO_LOWER="0.5"
fi

if [ "$SA_RATIO_HIGHER" == "" ]; then
SA_RATIO_HIGHER="0.9"
fi

if [ "$KPB_PERCENTAGE" == "" ]; then
KPB_PERCENTAGE="0.5"
fi

if [ "$SIMANN_INIT_PROB" == "" ]; then
SIMANN_INIT_PROB="0.4"
fi

if [ "$SIMANN_LOOPS_FACTOR" == "" ]; then
SIMANN_LOOPS_FACTOR="16"
fi

if [ "$SIMANN_REDUCE" == "" ]; then
SIMANN_REDUCE="0.95"
fi

if [ "$SIMANN_MIN_PROB" == "" ]; then
SIMANN_MIN_PROB="0.02"
fi



if [ "$COMPUTER_INTERRUPT" == "" ]; then
COMPUTER_INTERRUPT="get_progress"
fi

if [ "$COMPUTER_REQUIRED_APPLICATIONS" == "" ]; then
COMPUTER_REQUIRED_APPLICATIONS="0"
fi

if [ "$EXECUTOR_IDLE_RESCHEDULE" == "" ]; then
EXECUTOR_IDLE_RESCHEDULE="false"
fi

if [ "$TASK_RUNUNTIL" == "" ]; then
TASK_RUNUNTIL="progress_suspend"
fi



if [ "$TASKLOADER" == "" ]; then
TASKLOADER="taskloaderms"
fi

if [ "$TASKLOADERMSPATH" == "" ]; then
TASKLOADERMSPATH="$(realpath "$SCHED_RESULTS/$SCHED_HOST/ms_results")"
fi



if [ "$RESOURCELOADER" == "" ]; then
RESOURCELOADER="resourceloaderms"
fi

if [ "$RESOURCELOADERMS_IDLE" == "" ]; then
RESOURCELOADERMS_IDLE="$(realpath "$SCHED_RESULTS/$SCHED_HOST/ms_idle/idle_power_test.json")"
fi



if [ "$MEASURE" == "" ]; then
MEASURE="false"
fi

if [ "$MEASUREMENT" == "" ]; then
MEASUREMENT="ampehre"
fi

if [ "$AMPEHRE_CPU_S" == "" ]; then
AMPEHRE_CPU_S="0"
fi

if [ "$AMPEHRE_CPU_NS" == "" ]; then
AMPEHRE_CPU_NS="500000000"
fi

if [ "$AMPEHRE_GPU_S" == "" ]; then
AMPEHRE_GPU_S="0"
fi

if [ "$AMPEHRE_GPU_NS" == "" ]; then
AMPEHRE_GPU_NS="500000000"
fi

if [ "$AMPEHRE_FPGA_S" == "" ]; then
AMPEHRE_FPGA_S="0"
fi

if [ "$AMPEHRE_FPGA_NS" == "" ]; then
AMPEHRE_FPGA_NS="500000000"
fi

if [ "$AMPEHRE_SYSTEM_S" == "" ]; then
AMPEHRE_SYSTEM_S="0"
fi

if [ "$AMPEHRE_SYSTEM_NS" == "" ]; then
AMPEHRE_SYSTEM_NS="500000000"
fi

if [ "$AMPEHRE_MIC_S" == "" ]; then
AMPEHRE_MIC_S="0"
fi

if [ "$AMPEHRE_MIC_NS" == "" ]; then
AMPEHRE_MIC_NS="500000000"
fi



# output

echo "resources:"
for r in $RES; do
	echo " - name: \"$r\""
done

echo "scheduler: \"$SCHEDULER\""


echo "resource_taskendhook: \"$RESOURCE_TASKENDHOOK\""


echo "genetic_seed: $GENETIC_SEED"
echo "geneticmig_solver: $GENETICMIG_SOLVER"
echo "lp_destination: $LP_DESTINATION"

echo "sa_ratio_lower: $SA_RATIO_LOWER"
echo "sa_ratio_higher: $SA_RATIO_HIGHER"

echo "kpb_percentage: $KPB_PERCENTAGE"

echo "simann_init_prob: $SIMANN_INIT_PROB"
echo "simann_loops_factor: $SIMANN_LOOPS_FACTOR"
echo "simann_reduce: $SIMANN_REDUCE"
echo "simann_min_prob: $SIMANN_MIN_PROB"



echo "computer_interrupt: \"$COMPUTER_INTERRUPT\""
echo "computer_required_applications: $COMPUTER_REQUIRED_APPLICATIONS"

echo "executor_idle_reschedule: $EXECUTOR_IDLE_RESCHEDULE"

echo "task_rununtil: \"$TASK_RUNUNTIL\""

echo "taskloader: \"$TASKLOADER\""

echo "taskloadermspath: \"$TASKLOADERMSPATH\""

echo "resourceloader: \"$RESOURCELOADER\""

echo "resourceloaderms_idle: \"$RESOURCELOADERMS_IDLE\""

echo "measure: $MEASURE"

echo "measurement: \"$MEASUREMENT\""

echo "ampehre_cpu_s: $AMPEHRE_CPU_S"
echo "ampehre_cpu_ns: $AMPEHRE_CPU_NS"
echo "ampehre_gpu_s: $AMPEHRE_GPU_S"
echo "ampehre_gpu_ns: $AMPEHRE_GPU_NS"
echo "ampehre_fpga_s: $AMPEHRE_FPGA_S"
echo "ampehre_fpga_ns: $AMPEHRE_FPGA_NS"
echo "ampehre_system_s: $AMPEHRE_SYSTEM_S"
echo "ampehre_system_ns: $AMPEHRE_SYSTEM_NS"
echo "ampehre_mic_s: $AMPEHRE_MIC_S"
echo "ampehre_mic_ns: $AMPEHRE_MIC_NS"


