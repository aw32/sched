#!/bin/bash
# Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
# SPDX-License-Identifier: BSD-2-Clause


stdbuf -oL valgrind --leak-check=full --show-leak-kinds=all --child-silent-after-fork=yes --track-origins=yes "$VAL_WRAP" "$@"
