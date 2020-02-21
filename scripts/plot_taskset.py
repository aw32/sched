#!/usr/bin/env python3
# Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
# SPDX-License-Identifier: BSD-2-Clause


# Reads a taskset file and creates an resource affinity triangle.
# Tasks are displayed as points.
# A cross marks the affinity of the sum of task times.
# The resulting plot is displayed or saved to a file svg.


import sys
import json

import plot_tod



if __name__ == "__main__":

	if len(sys.argv) < 2:
		print(sys.argv[0], "taskset-file", "[outfile.svg]")
		print("Create triangle from a taskset file.")
		print("Omit second parameter to only show.")
		sys.exit(1)

	tasksetfile = sys.argv[1]
	outfile = None if len(sys.argv) < 3 else sys.argv[2]
	show = True if outfile == None else False

	# read taskset file
	tasksetjson = None
	try:
		with open(tasksetfile, "r") as f:
			fcontent = f.read()
			tasksetjson = json.loads(fcontent)
	except Exception as e:
		print("Failed to read taskset file",tasksetfile)
		print(e)
		sys.exit(-1)

	# create task list
	tasks = []
	for e in tasksetjson:
		if e["type"] == "TASKDEF":
			task = (e["name"], e["size"])
			tasks.append(task)

	# create triangle and show or save result
	plot_tod.plot_taskset(tasks, filepath=outfile, show=show)
