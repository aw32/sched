#!/bin/bash
# Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
# SPDX-License-Identifier: BSD-2-Clause



if [ $# -lt 1 ]; then
	echo "$0 file"
	echo "Greps all d= path attributes out of the file"
	exit 1
fi

grep -P "^[\s]*d=" "$1"  | sed 's/^ *d="//;s/"$//;s/\([^MmLlh]\) /\1\n/g'

