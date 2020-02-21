#!/bin/bash
# Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
# SPDX-License-Identifier: BSD-2-Clause


DIR="$(dirname "$1")"
BASE="$(basename "$1" .lp)"

lp_solve -parse_only -wmps "$DIR/$BASE.mps" "$1"
