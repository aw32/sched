#!/bin/bash
# Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
# SPDX-License-Identifier: BSD-2-Clause


DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

if [ $# -lt 1 ]; then
	echo "$0" "[sim|exp]"
	exit 1
fi

for testtype in $1; do
	if [ "$testtype" != "sim" ] && [ "$testtype" != "exp" ]; then
		echo "$0" "[sim|exp]"
		exit 1
	fi
done

if [ "$SCHED_ENV" == "" ]; then
	echo SCHED_ENV not set
	exit 1
fi

if [ "$SCHED_RESULTS" == "" ]; then
	echo SCHED_RESULTS not set
	exit 1
fi

if [ "$SCHED_HOST" == "" ]; then
	SCHED_HOST="$(hostname)"
	export SCHED_HOST
fi

if [ "$SCHED_PRECONF" == "" ]; then
	PRECONF="default"
else
	PRECONF="$SCHED_PRECONF"
fi
export PRECONF


if [ "$TEST_RESULTDIR" == "" ]; then
	# create timestamp for this run
	TIMESTAMP="$(date "+%Y%m%d-%H%M%S")"

	# create results path
	TEST_RESULTDIR="$SCHED_RESULTS/$SCHED_HOST/test_results/$PRECONF/$TIMESTAMP"
fi

DIRS="$(find ! -path . -prune -type d -print)"

COUNT_ALL=0
COUNT_PASS=0
TEST_RESULTLOG="$TEST_RESULTDIR/test.log"

# PREPARE

	echo -e "Host: \e[93m${SCHED_HOST}\e[39m"
	echo -e "Results: \e[93m${SCHED_RESULTS}\e[39m"
	echo -e "Preconf: \e[93m${PRECONF}\e[39m"
	echo -e "Result directory: \e[93m${TEST_RESULTDIR}\e[39m"

	# Create results folder
	mkdir -p "${TEST_RESULTDIR}"

	echo "---"


# RUN TESTS

	for TESTDIR in $DIRS; do
		for testtype in $1; do

			# run test
			(
				cd "$TESTDIR"
				if [ -f "test" ]; then
					TEST_EXEC="./test"
				else
					TEST_EXEC="$SCHED_ENV/scripts/test.sh"
				fi
				TEST_RESULTDIR="$TEST_RESULTDIR" "$TEST_EXEC" "$testtype"
			)
			CODE="$?"

			# test succeeded
			if [ "$CODE" = "0" ]; then
				COUNT_PASS=$((COUNT_PASS+1))
			fi

			# test type not supported
			if [ "$CODE" != "2" ]; then
				COUNT_ALL=$((COUNT_ALL+1))
			fi

			# log result
			TESTNAME="$(basename "$TESTDIR")"
			echo "$CODE $TESTDIR $testtype" >> $TEST_RESULTLOG

		done
	done


# SUMMARY
	echo "---"
	echo $COUNT_PASS / $COUNT_ALL
	if [ "$COUNT_PASS" != "$COUNT_ALL" ]; then
		exit 2
	else
		exit 0
	fi
