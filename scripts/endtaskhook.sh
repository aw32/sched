#!/bin/sh
# Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
# SPDX-License-Identifier: BSD-2-Clause


# Return codes:
# 0 - Loaded idle bitstream
# 1 - Generic Error
# 2 - Break time not long enough

for var in "$@"
do
    echo -n " " "$var"
done
echo

# not enough arguments
if [ $# -lt 1 ]; then
	echo "$0 [Resource] [Current task name] [Current task size] [[Next task name] [Next task size] [Time until next task in nanoseconds]]"
	echo "not enough arguments"
	exit 1
fi

if [ $1 != "MaxelerVectis" ]; then
	echo "wrong resource" "$1"
	exit 1
fi

# does the program exist?
which maxforceidle &> /dev/null
FOUND=$?

if [ $FOUND != 0 ]; then
	echo "maxforceidle not found"
	exit 1
fi

# no next task
if [ $# -lt 6 ]; then
	echo "$0 [Resource] [Current task name] [Current task size] [[Next task name] [Next task size] [Time until next task in nanoseconds]]"
	echo "not enough arguments but setting idle bitstream anyway"
	maxforceidle -a
	exit 0
fi

BREAK=$[$6]

# time between current and next task in nanoseconds
# maxforceidle takes for one device with non idle bitstream around 300ms
# maxforceidle takes for one device with idle bitstream around 10ms
if [ $BREAK -gt 1000000000 ]; then
	maxforceidle -a
	exit 0
else
	exit 2
fi

exit 1

