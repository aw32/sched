#!/bin/bash
# Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
# SPDX-License-Identifier: BSD-2-Clause


if [ "$SCHED_ENV" == "" ]; then
	echo "SCHED_ENV not defined"
	exit 1
fi

PYTHONPATH="$SCHED_ENV/scripts" ./plotExp1.py $@
