#!/bin/bash
# Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
# SPDX-License-Identifier: BSD-2-Clause


if [ "$SCHED_ENV" = "" ]; then
	echo "SCHED_ENV invalid"
	exit 1
fi

GENSET="$SCHED_ENV/scripts/gentaskset.py"
ALGOSIM="$SCHED_ENV/scripts/execalgosim.sh"
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

				if [ -d $EXPFOLDER ]; then

					echo "$EXPFOLDER"

					# execute sim for all algorithms
					cd "$EXPFOLDER"
					"$ALGOSIM" "$EXPFOLDER" "$ALGOLIST"
					RET="$?"
					if [ "$RET" != "0" ]; then
						exit 1
					fi
					cd ..

				else
					echo "$EXPFOLDER" does not exist
				fi

			done

		done

	done

done
