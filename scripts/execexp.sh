#!/bin/bash
# Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
# SPDX-License-Identifier: BSD-2-Clause


if [ "$SCHED_ENV" == "" ]; then
	echo "SCHED_ENV variable not found"
	exit 1
fi


if [ $# -lt 2 ]; then
	echo "$0 algo-name sim-name"
	exit 1
fi

SCHED="$SCHED_ENV/build/sched"
WRAP="$SCHED_ENV/build/wrap"
WRAPVALWRAP="$SCHED_ENV/scripts/valwrap.sh"
EXEC="$SCHED_ENV/scripts/taskset_exec.py"

EXP="$PWD"

LOGS="$EXP/logs"
SCHED_OUT="$EXP/sched_exp.out"
SCHED_ERR="$EXP/sched_exp.err"
CONFIG="$EXP/$1.conf"
SIMFILE="$EXP/$2.sim"

mkdir -p "$LOGS"

# start scheduler
export SCHED_LOG_PRIORITY="DEBUG"
export SCHED_EVENTLOG="$LOGS/sched.eventlog"
export SCHED_LOG="$LOGS/sched.log"
export SCHED_CONFIG="$CONFIG"
export SCHED_SOCKET="$(mktemp -u -t sched.socket.XXXXXX)"

if [ ! "$DEBUG" == "" ]; then
	export SCHED_LOG_PRIORITY="DEBUG"
fi

if [ "$VALGRIND" == "" ]; then
	PAPI_NVML_NOPERS="1" LD_PRELOAD=/usr/ampehre/lib/libms_common_apapi.so "$SCHED" >"$SCHED_OUT" 2>"$SCHED_ERR" &
else
	ulimit -c 0
	stdbuf -oL valgrind --leak-check=full --show-leak-kinds=all --child-silent-after-fork=yes --track-origins=yes \
	env PAPI_NVML_NOPERS="1" LD_PRELOAD=/usr/ampehre/lib/libms_common_apapi.so "$SCHED" >"$SCHED_OUT" 2>"$SCHED_ERR" &
fi

SCHEDPID="$!"

# clean logs
rm -f $LOGS/*


sleep 1

# start taskset execution
if [ "$VALGRIND" == "" ]; then
	WRAP_FILE_PATH="$EXP" WRAP_CONFIG="$CONFIG" LOG_PATH="$LOGS" WRAP_PATH="$WRAP"      "$EXEC" exec "$TASKDEF" "$SIMFILE"
else
	VAL_WRAP="$WRAP" WRAP_FILE_PATH="$EXP" WRAP_CONFIG="$CONFIG" LOG_PATH="$LOGS" WRAP_PATH="$WRAPVALWRAP"      "$EXEC" exec "$TASKDEF" "$SIMFILE"
fi

EXECCODE="$?" 


# stop scheduler
$(sleep 1;kill "$SCHEDPID") &
wait "$SCHEDPID"

SCHEDCODE="$?"

echo exec "$EXECCODE"
echo sched "$SCHEDCODE"

if [ "$EXECCODE" != "0" ] || [ "$SCHEDCODE" != "0" ]; then
exit 1
fi

