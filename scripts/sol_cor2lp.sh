#!/bin/bash
# Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
# SPDX-License-Identifier: BSD-2-Clause


sed -i 's/^Optimal - objective value \(.*\)$/Value of objective function: \1\n\nActual values of the variables:/' "$1"

sed -i 's/[ ]*\([0-9]\+\) \([^ ]\+\)[ ]*\([^ ]\+\)[ ]*\([^ ]\+\)/\2 \3/' "$1"

