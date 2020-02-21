#!/usr/bin/env python3
# Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
# SPDX-License-Identifier: BSD-2-Clause


import os
import os.path
import sys
import json

import log_stats
import report
import plot

class SchedTest:
	def __init__(self, testtype):
		self.testtype = testtype
	def loadTest(testtype, testname = None, resultdir = None, host = None):
		if testtype not in ["sim","exp"]:
			return None
		test = SchedTest(testtype)
		if "SCHED_ENV" not in os.environ:
			print("SCHED_ENV not set")
			return None
		test.SCHED_ENV = os.environ["SCHED_ENV"]
		if "SCHED_RESULTS" not in os.environ:
			print("SCHED_RESULTS not set")
			return None
		test.SCHED_RESULTS = os.environ["SCHED_RESULTS"]
		if host == None and "SCHED_HOST" not in os.environ:
			print("SCHED_HOST not set")
			return None
		test.SCHED_HOST = host if host != None else os.environ["SCHED_HOST"]
		if resultdir == None and "TEST_RESULTDIR" not in os.environ:
			print("TEST_RESULTDIR not set")
			return None
		test.PRECONF = "default"
		if "SCHED_PRECONF" in os.environ and os.environ["SCHED_PRECONF"] != "":
			test.PRECONF = os.environ["SCHED_PRECONF"]
		test.TESTPATH = os.getcwd()
		test.NAME = os.path.basename(test.TESTPATH) if testname == None else testname
		test.TEST_RESULTDIR = os.path.join(os.environ["TEST_RESULTDIR"],test.NAME) if resultdir == None else resultdir
		return test
	def log_folder(self):
		if self.testtype == "sim":
			return "simlogs/sched.simlog"
		else:
			return "logs/sched.eventlog"
	def loadReferenceLogs(self):
		REFDIR = os.path.join(self.SCHED_RESULTS,self.SCHED_HOST,"test_references",self.PRECONF,self.NAME)
		reflogs = []
		if os.path.isdir(REFDIR):
			for subf in os.listdir(REFDIR):
				refpath = os.path.join(REFDIR, subf)
				if os.path.isdir(refpath) == False:
					continue
				logpath = os.path.join(refpath,self.log_folder())
				#print(logpath)
				if os.path.isfile(logpath) == False:
					continue
				log = log_stats.EventLog.loadEvents(logpath)
				if log != None:
					reflogs.append(log)
		self.reflogs = reflogs
	def loadTaskset(self):
		self.tasksetpath = os.path.join(self.TEST_RESULTDIR, self.NAME+".sim")
		taskset = None
		taskset_tasks = []
		taskset_reg = []
		try:
			with open(self.tasksetpath) as f:
				fcontent = f.read()
				taskset = json.loads(fcontent)
			for o in taskset:
				if o["type"] == "TASKDEF":
					taskset_tasks += [o]
				if o["type"] == "TASKREG":
					taskset_reg += [o]
		except Exception as e:
			print(e)
		self.taskset = taskset
		self.taskset_tasks = taskset_tasks
		self.taskset_reg = taskset_reg
	def loadTestLog(self):
		logpath = os.path.join(self.TEST_RESULTDIR, self.log_folder())
		#print(logpath)
		log = None
		if os.path.exists(logpath):
			log = log_stats.EventLog.loadEvents(logpath)
		else:
			print(logpath, "does not exist")
		self.testlog = log

		self.wraplogs = []
		if self.testtype == "exp" and self.testlog != None:
			found = True
			wrapi = 0
			appnum = len(self.testlog.apps)
			while found == True or wrapi < appnum:
				wrappath = os.path.join(self.TEST_RESULTDIR, "logs", "wrap"+str(wrapi)+".eventlog")
				if os.path.isfile(wrappath) == True:
					wraplog = log_stats.WrapLog.loadWrapLog(wrappath)
					self.wraplogs.append(wraplog)
				else:
					self.wraplogs.append(None)
					found = False
				wrapi += 1
		self.loadTaskset()
		return self.testlog != None
	def result(self, res, message=""):
		if message != "":
			sys.stdout.write(message)
		if res == "PASS":
			sys.exit(0)
		elif res == "FAIL":
			sys.exit(1)
	def generate_report(self):
		report_prefix = os.path.join(self.TEST_RESULTDIR, self.testtype)
		report.create_report(self, self.NAME, self.testlog, self.wraplogs, self.testtype, report_prefix)
		plot.plot_experiment(self.testlog, report_prefix+"_all.svg", barcolor="res_affinity")
		for ix,schedule in enumerate(self.testlog.schedules):
			plot.plot_schedule(self.testlog, schedule, report_prefix+"_schedule_"+str(ix)+".svg", barcolor="res_affinity")

		plot.plot_task_lifelines(self.testlog, report_prefix+"_tasklifelines.svg")

		plot.plot_taskset_triangle(self.testlog, report_prefix+"_taskset_triangle.svg")
		if len(self.testlog.measure) > 0:
			plot.plot_measurement(self.testlog, report_prefix+"_measurement.svg")

