#!/bin/bash
# Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
# SPDX-License-Identifier: BSD-2-Clause


# get script path
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
# get parent path
DIR="$(realpath "$DIR/..")"

export SCHED_ENV="$DIR"

if [ "$PYTHONPATH" == "" ]; then
	export PYTHONPATH="$DIR/scripts/"
else
	export PYTHONPATH="${PYTHONPATH}:$DIR/scripts/"
fi

if [ "$SCHED_RESULTS" == "" ]; then
	export SCHED_RESULTS="$HOME/sched/hosts"
	if [ ! -d "$SCHED_RESULTS" ]; then
		mkdir -p "$SCHED_RESULTS"
	fi
fi

if [ "$SCHED_HOST" == "" ]; then
	export SCHED_HOST="$(hostname)"
fi

echo -e "SCHED_ENV: \e[93m${SCHED_ENV}\e[39m"
echo -e "SCHED_HOST: \e[93m${SCHED_HOST}\e[39m"
echo -e "SCHED_RESULTS: \e[93m${SCHED_RESULTS}\e[39m"
