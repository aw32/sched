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

SCHED="$SCHED_ENV/build/simsched"
EXEC="$SCHED_ENV/scripts/taskset_exec.py"

EXP="$PWD"

LOGS="$EXP/simlogs"
SCHED_OUT="$EXP/sched_sim.out"
SCHED_ERR="$EXP/sched_sim.err"
CONFIG="$EXP/$1.conf"
SIMFILE="$EXP/$2.sim"

mkdir -p "$LOGS"

# start scheduler
export SCHED_EVENTLOG="$LOGS/sched.eventlog"
export SCHED_LOG="$LOGS/sched.log"
export SCHED_SIMLOG="$LOGS/sched.simlog"
export SCHED_CONFIG="$CONFIG"
export SCHED_SOCKET="$(mktemp -u -t sched.socket.XXXXXX)"
export SCHED_SIMFILE="$SIMFILE"

# clean logs
rm -f $LOGS/*

if [ ! "$DEBUG" == "" ]; then
	export SCHED_LOG_PRIORITY="DEBUG"
fi

if [ "$VALGRIND" == "" ]; then
	"$SCHED" >"$SCHED_OUT" 2>"$SCHED_ERR"
else
	ulimit -c 0
	stdbuf -oL valgrind --leak-check=full --show-leak-kinds=all --child-silent-after-fork=yes "$SCHED" >"$SCHED_OUT" 2>"$SCHED_ERR"
fi

SCHEDCODE="$?"

echo sched "$SCHEDCODE"

if [ "$SCHEDCODE" != "0" ]; then
exit 1
fi

