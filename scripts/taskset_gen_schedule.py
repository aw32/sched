#!/usr/bin/env python3
# Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
# SPDX-License-Identifier: BSD-2-Clause


import sys
import json
import random


import cdf

debug_vars = {}

class App:
	def __init__(self, ids, tasks):
		self.ids = ids
		self.tasks = tasks
		self.arrival = None
		for ix,t in enumerate(tasks):
			t["dependencies"] = [] if ix == 0 else [ids[ix]-1]
			#print(ids[ix],"->",t.dependencies)

def genApps(tasks, rand, maxtasksperapp):
	apps_cdf = cdf.CDF.loadCDF("gauss.cdf")
	apps = []
	tlist = tasks.copy()
	cur = 0
	while len(tlist) > 0:
		val = rand.random()
		size = round(apps_cdf.sample(val)*maxtasksperapp)
		if size == 0:
			continue
		if len(tlist) < size:
			size = len(tlist)
		a = App(list(range(cur,cur+size)),tlist[0:size])
		apps.append(a)
		tlist = tlist[size:]
		cur += size
		#print(a.ids)
	return apps


def genTimes(tasknum, maxtime, rand):
	arrival_cdf = cdf.CDF.loadCDF("gauss.cdf")
	times = []
	cur = 0.0
	for i in range(0, tasknum):
		val = rand.random()
		time = arrival_cdf.sample(val)*maxtime
		cur += time
		times.append(cur)
	return times


def normal(tasks, rand, maxtasksperapp, maxarrivaldiff):

	apps = genApps(tasks, rand, maxtasksperapp)
	apptimes = genTimes(len(apps), maxarrivaldiff, rand)
	dates = []
	for ix,a in enumerate(apps):
		dates.append((a.ids, apptimes[ix]))
	return dates


def printHelp():
	print(sys.argv[0], "command", "[options]")
	print("\tcommands:")
	print("\t\tnormal: normal distributed task intervals and number of tasks in app")
	print("\t\t\toptions: in-file out-file seed maxarrivaldiff maxtasksperapp")
	print("\toptions:")
	print("\t\tin-file: taskset file containing existing taskset")
	print("\t\tout-file: output taskset file including schedule")
	print("\t\tseed: seed used to initialize random number generator")
	print("\t\tmaxarrivaldiff: maximal time difference between tasks in seconds")
	print("\t\tmaxtasksperapp: maximal number of tasks in one app")


if __name__ == "__main__":
	if len(sys.argv) < 2:
		printHelp()
		sys.exit(-1)

	command = sys.argv[1]
	if command not in ["normal"]:
		print("Unknown command")
		printHelp()
		sys.exit(-1)

	tasksetfilepath = sys.argv[2]
	outfilepath = sys.argv[3]
	seed = int(sys.argv[4])

	rand = random.Random()
	rand.seed(seed)

	taskset = None
	try:
		with open(tasksetfilepath, "r") as f:
			fcontent = f.read()
			taskset = json.loads(fcontent)
	except Exception as e:
		print("Failed to open taskset file",tasksetfilepath)
		print(e)
		sys.exit(-1)

	parameters = None
	tasks = []
	other = []
	for obj in taskset:
		if obj["type"] == "PARAMETERS":
			parameters = obj
		elif obj["type"] == "TASKDEF":
			tasks.append(obj)
		elif obj["type"] == "TASKREG":
			print("taskset file", tasksetfilepath, "already contains TASKREG entries")
			sys.exit(-1)
		else:
			other.append(obj)

	taskreg = []
	if command == "normal":
		maxarrivaldiff = float(sys.argv[5])
		maxtasksperapp = float(sys.argv[6])
		taskreg = normal(tasks, rand, maxtasksperapp, maxarrivaldiff)
		parameters["taskreg_command"] = "normal"
		parameters["taskreg_randomseed"] = seed
		parameters["taskreg_maxarrivaldiff"] = maxarrivaldiff
		parameters["taskreg_maxtasksperapp"] = maxtasksperapp
		

	outfile = None
	outfile = open(outfilepath, "w")
	outfile.write("[\n")
	# write parameters
	for key in debug_vars:
		parameters[key] = debug_vars[key]
	outfile.write(json.dumps(parameters)+"\n")
	# write taskdef
	for task in tasks:
		outfile.write(","+json.dumps(task)+"\n")
	# write taskreg
	for ix,t in enumerate(taskreg):
		outfile.write(',{"type":"TASKREG"')
		outfile.write(',"tasks":%s' % str(t[0]))
		outfile.write(',"time":%ld' % (t[1]*1000000000.0))
		outfile.write('}\n')
	# write other
	for obj in other:
		outfile.write(","+json.dumps(obj)+"\n")
	outfile.write("]\n")
	outfile.close()

