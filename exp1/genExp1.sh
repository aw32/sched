#!/bin/bash
# Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
# SPDX-License-Identifier: BSD-2-Clause


# read 4 bytes from /dev/urandom and print them as unsigned integer
function getrand() {
	dd if=/dev/urandom count=4 bs=1 2>/dev/null | od -A n -t u
}

if [ "$SCHED_ENV" = "" ]; then
	echo "SCHED_ENV invalid"
	exit 1
fi

GENSET="$SCHED_ENV/scripts/gentaskset.py"
GENALGO="$SCHED_ENV/scripts/genalgoexp.sh"
TASKDEFFILE="$SCHED_ENV/tasks_mig.def"
MSRESULTS="$SCHED_ENV/ms_results"
ALGOLIST="algolistdyn.txt"

TASKNUMLIST="40 50 60 70 80"
ARRTIMEDIFFLIST="1 2 4 8"
TASKSPERAPPLIST="5 10 15 20"

for REPEAT in {1..3}; do

	for TASKNUM in $TASKNUMLIST; do

		for ARRTIMEDIFF in $ARRTIMEDIFFLIST; do

			for TASKSPERAPP in $TASKSPERAPPLIST; do

				EXPFOLDER="$TASKNUM"_"$ARRTIMEDIFF"_"$TASKSPERAPP"_"$REPEAT"

				if [ ! -d $EXPFOLDER ]; then

					echo "$EXPFOLDER"
					TASKSET="$EXPFOLDER/$EXPFOLDER.sim"
					SEED="$(getrand)"

					# create base folder for parameter combination
					mkdir -p "$EXPFOLDER"
					"$GENSET" bimodal "$TASKDEFFILE" "$TASKSET" "$SEED" "$MSRESULTS" "$TASKNUM" "$ARRTIMEDIFF" "$TASKSPERAPP"
					cp wrap.conf "$EXPFOLDER/"
					cp "$ALGOLIST" "$EXPFOLDER/"

					# create algo folders
					cd "$EXPFOLDER"
					"$GENALGO" "$EXPFOLDER" "$ALGOLIST"
					cd ..

				else
					echo "$EXPFOLDER" already exists
				fi

			done

		done

	done

done
