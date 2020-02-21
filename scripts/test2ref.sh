#!/bin/bash
# Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
# SPDX-License-Identifier: BSD-2-Clause


# Turns test results to reference results

if [ $# -lt 1 ]; then
	echo "$0 <Test result path>"
	exit 1
fi

TEST_RESULTS="$(realpath "$1")"

if [ ! -d "$TEST_RESULTS" ]; then
	echo "$TEST_RESULTS does not exist"
	exit 1
fi

TEST_DATE="$(basename "$TEST_RESULTS")"

REF_DIR="$(realpath "$TEST_RESULTS/../../../test_references")"

if [ ! -d "$REF_DIR" ]; then
	mkdir -p "$REF_DIR"
fi

PRECONF_NAME="$(basename "$(realpath "$TEST_RESULTS/../")")"
PRECONF_PATH="$REF_DIR/$PRECONF_NAME"

if [ ! -d "$PRECONF_PATH" ]; then
	mkdir -p "$PRECONF_PATH"
fi

(
	cd "$TEST_RESULTS"
	for subdir in $(find . -maxdepth 1 -mindepth 1 -type d -printf '%f\n'); do

		SOURCE="$TEST_RESULTS/$subdir"
		if [ ! -d "$SOURCE" ]; then
			echo "skip $SOURCE"
			continue
		fi

		# copy test result to reference
		mkdir -p "$PRECONF_PATH/$subdir"
		TARGET="$PRECONF_PATH/$subdir/$TEST_DATE"
		if [ -d "$TARGET" ]; then
			echo "$TARGET already exists"
		else
			echo "$SOURCE" "$TARGET"
			cp -r "$SOURCE" "$TARGET"
		fi
	done
	#echo "$REF_DIR"
	#echo "$PRECONF_PATH"
)
