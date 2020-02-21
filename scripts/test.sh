#!/bin/bash
# Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
# SPDX-License-Identifier: BSD-2-Clause


DIR="$(pwd)"
NAME="$(basename "$DIR")"

if [ $# -lt 1 ]; then
	echo "$0" "[sim|exp]"
	exit 1
fi

if [ "$1" != "sim" ] && [ "$1" != "exp" ]; then
	echo "$0" "[sim|exp]"
	exit 1
fi

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
	echo -e "Host: \e[93m${SCHED_HOST}\e[39m"
	echo -e "Results: \e[93m${SCHED_RESULTS}\e[39m"
fi


# single test execution
if [ "$TEST_RESULTDIR" == "" ]; then

	if [ "$SCHED_PRECONF" == "" ]; then
		PRECONF="default"
		echo -e "Preconf: \e[93m${PRECONF}\e[39m"
	else
		PRECONF="$SCHED_PRECONF"
	fi
	export PRECONF

	# create timestamp for this run
	TIMESTAMP="$(date "+%Y%m%d-%H%M%S")"

	# create results path
	TEST_RESULTDIR="$SCHED_RESULTS/$SCHED_HOST/test_results/$PRECONF/$TIMESTAMP"
	export TEST_RESULTDIR

	echo -e "Result directory: \e[93m${TEST_RESULTDIR}\e[39m"

	# Create results folder
	mkdir -p "${TEST_RESULTDIR}"
fi

TESTDIR="$TEST_RESULTDIR/$NAME"
mkdir -p "$TESTDIR"

# RUN TEST

if [ "$1" = "sim" ]; then
	TESTSCRIPT="$SCHED_ENV/scripts/execsim.sh"
elif [ "$1" = "exp" ]; then
	TESTSCRIPT="$SCHED_ENV/scripts/execexp.sh"
fi

CHECKSCRIPT="$DIR/check"
if [ ! -f "$DIR/check" ]; then
	CHECKSCRIPT="$SCHED_ENV/scripts/check"
fi

	(
		# PREPARE
		export TEST_RESULTDIR
		[ -f "$DIR/prepare.sh" ] && "$DIR/prepare.sh" "$1"
	)



	(
		set -a
		[ -f "$DIR/$NAME.conf" ] && source "$DIR/$NAME.conf"
		set +a
		"${SCHED_ENV}/scripts/genconfig.sh" > "$TESTDIR/$NAME.conf"

		# RUN
		cp -f "$DIR/$NAME.sim" "$TESTDIR/$NAME.sim"
		if [ -f "$DIR/wrap.conf" ]; then
			cp -f "$DIR/wrap.conf" "$TESTDIR/wrap.conf"
		fi
		cd "$TESTDIR"
		export TASKDEF="$SCHED_RESULTS/$SCHED_HOST/tasks.def"
		"$TESTSCRIPT" $NAME $NAME > $TESTDIR/$1.log
	)
	EXECCODE=$?



	# CHECK
	PASS=false
	CHECK_RESULT=""
	if [ "$EXECCODE" = "0" ]; then
		cd "$TESTDIR"
		CHECK_RESULT="$("$CHECKSCRIPT" "$1")" && PASS=true
	fi
	# PRINT RESULT
	if [ "$PASS" = "true" ]; then
		echo -e "[\e[92;1m PASS \e[0m]  $NAME $1 $EXECCODE  Check: $CHECK_RESULT"
	else
		echo -e "[\e[91;1m FAIL \e[0m]  $NAME $1 $EXECCODE  Check: $CHECK_RESULT"
	fi
	# EXIT
	if [ "$PASS" = "true" ]; then
		exit 0
	else
		exit 1
	fi


