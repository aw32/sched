#!/usr/bin/env python3
# Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
# SPDX-License-Identifier: BSD-2-Clause


# Reads ms task measurement results from CSV files.
# 
# If the path to the results is not given it is read from the following environment variables:
# * SCHED_MSRESULTS
# * SCHED_RESULTS and SCHED_HOST: $SCHED_RESULTS/$SCHED_HOST/ms_results
#
# Use MSResults.load_results() get a MSResults object containing the results.

import os
import os.path
import re
import csv

# Regex for format of CSV files
# e.g. "ms_markov(1024)@IntelXeon_energy.csv"
# e.g. "ms_gaussblur(512)@NvidiaTesla_time.csv"
re_msresult_filename = re.compile("^ms_([^(]+)\(([0-9]+)\)@([^_]+)_(energy|time).csv")

# Contains measurement results read from energy and time CSV files
class MSResult:
	def __init__(self, task, size, res):
		self.task = task
		self.size = size
		self.res = res
		self.time = [] # all csv entries
		self.energy = [] # all csv entries
		self.avgtime = [] # averaged over all measurements
		self.avgenergy = [] # averaged over all measurements
	def avg_time(self):
		if len(self.avgtime) == 0:
			self.computeAvg()
		return self.avgtime[3]
	def avg_init(self):
		if len(self.avgtime) == 0:
			self.computeAvg()
		return self.avgtime[4]
	def avg_fini(self):
		if len(self.avgtime) == 0:
			self.computeAvg()
		return self.avgtime[6]
	def avg_energy(self):
		if len(self.avgenergy) == 0:
			self.computeAvg()
		return self.avgenergy[2]
	def read(task, size, resource, mspath):
		res = MSResult(task, size, resource)
		time = []
		energy = []
		try:
			csvpath = os.path.join(mspath,"ms_"+task+"("+str(size)+")@"+resource+"_time.csv")
			#print(csvpath)
			with open(csvpath,"r") as f:
				csvr = csv.reader(f, delimiter=";")
				for ix,row in enumerate(csvr):
					if ix < 2:
						continue
					frow = []
					for string in row:
						if string == "":
							continue
						frow.append(float(string))
					time.append(frow)
		except Exception as e:
			print("MSResults read failed: ",e,"time",task,resource,size)
		try:
			csvpath = os.path.join(mspath,"ms_"+task+"("+str(size)+")@"+resource+"_energy.csv")
			#print(csvpath)
			with open(csvpath,"r") as f:
				csvr = csv.reader(f, delimiter=";")
				for ix,row in enumerate(csvr):
					if ix < 2:
						continue
					frow = []
					for string in row:
						if string == "":
							continue
						frow.append(float(string))
					energy.append(frow)
		except Exception as e:
			print("MSResults read failed:",e,"energy",task,resource,size)
		if len(time) == 0 and len(energy) == 0:
			return None
		res.time = time
		res.energy = energy
		res.computeAvg()
		return res
	def computeAvg(self):
		self.avgtime = []
		self.avgenergy = []
		for c in range(0,7):
			a = 0.0
			n = 0
			for r in self.time:
				n += 1
				a += r[c]
			avg = 0
			if n != 0:
				avg = a/n
			self.avgtime.append(avg)
		for c in range(0,7):
			a = 0.0
			n = 0
			for r in self.energy:
				n += 1
				a += r[c]
			avg = 0.0
			if n != 0:
				avg = a/n
			self.avgenergy.append(avg)

# Contains list of available results and loaded task measurments
# MSResult objects are created lazily on first access
class MSResults:
	def __init__(self, mspath=None):
		self.mspath = mspath
		self.result_list = []
		self.results = {}
	# Return list of available sizes for $task
	def task_sizes(self, task):
		sizes = []
		for t in self.result_list:
			if t[0] == task and t[1] not in sizes:
				sizes.append(t[1])
		return sorted(sizes)
	def task_res_sizes(self, task, res):
		sizes = []
		for t in self.result_list:
			if t[0] == task and t[2] == res and t[1] not in sizes:
				sizes.append(t[1])
		return sorted(sizes)
	# Read ms results directory with task measurement results
	# If $mspath is not given, the environment variables are used
	def load_results(mspath=None):
		if mspath == None:
			# try to find mspath
			if "SCHED_MSRESULTS" in os.environ:
				mspath = os.environ["SCHED_MSRESULTS"]
			elif "SCHED_RESULTS" in os.environ or "SCHED_HOST" in os.environ:
				mspath = os.path.join(os.environ["SCHED_RESULTS"], os.environ["SCHED_HOST"], "ms_results")
			if mspath == None:
				print("Error: SCHED_MSRESULTS or SCHED_RESULTS and SCHED_HOST environment variables not defined, can't locate ms results")
				return None
			if os.path.isdir(mspath) == False:
				print("Error: ms results path seems not to exist: ", mspath)
				print("Check SCHED_MSRESULTS, SCHED_RESULTS and SCHED_HOST environment variables")
				return None
		msres = MSResults(mspath=mspath)
		allresults = []
		results = []
		# Check list of files in $mspath matching the $re_msresult_filename regex
		for dirname, dirnames, filenames in os.walk(mspath):
			for name in filenames:
				match = re_msresult_filename.match(name)
				if match == None:
					continue
				task, size, res, restype = match.groups()
				entry = (task, int(size), res)
				if entry not in results:
					results.append(entry)
				allresults.append(match.groups())
		# Check if both, energy and time, CSV files were found
		# Only use results with both files existing
		for entry in results:
			task, size, res = entry
			eentry = (task, str(size), res, "energy")
			tentry = (task, str(size), res, "time")
			if eentry in allresults and tentry in allresults:
				msres.result_list.append(entry)
		return msres
	# Return result for $task with $size on resource $res
	def result(self, task, size, res):
		if (task, size, res) not in self.result_list:
			print("task", task, size, res, "not in ms results" )
			return None
		# check if result was already loaded, else load the result and add it to the dict
		if (task, size, res) not in self.results:
			result = MSResult.read(task, size, res, self.mspath)
			self.results[(task, size, res)] = result
		return self.results[(task, size, res)]
