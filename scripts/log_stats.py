#!/usr/bin/env python3
# Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
# SPDX-License-Identifier: BSD-2-Clause


import sys
import json
import io
import math
import datetime

resource_short = {"IntelXeon":"C", "NvidiaTesla":"G", "MaxelerVectis":"F"}

class App:
	def __init__(self):
		self.tids = []
		self.tasks = []
	def arrival_time(self):
		return self.tasks[0].arrival
	def load_apps(tasks):
		pools = []
		for tid in tasks.keys():
			pools += [[tid]]
		for tid in tasks.keys():
			ownpool = None
			# search own pool
			for p in pools:
				if tid in p:
					ownpool = p
					pools.remove(p)
					break
			# search pool with dependencies
			deppools = []
			for d in tasks[tid].dep:
				for p in pools:
					if d in p:
						deppools.append(p)
						pools.remove(p)
						break
			# merge
			for d in deppools:
				ownpool += d
			# put back
			pools.append(ownpool)
		apps = []
		for p in pools:
			a = App()
			a.tids = sorted(p)
			for tid in a.tids:
				a.tasks.append(tasks[tid])
			apps.append(a)
		return apps
			
			

class Task:
	def __init__(self):
		self.parts = []
		self.dep = []
		self.tid = None
		self.name = None
		self.arrival = None
		self.checkpoints = None
		self.size = None
		self.newtask = None
		self.endtask = None
		self.state = None
		self.finish = None

class TaskPart:
	def __init__(self):
		self.start = None
		self.stop = None
		self.res = None
		self.res_short = None
		self.startevent = None
		self.stopevent = None
		self.startprogress = 0
		self.progress = None

class Schedule:
	def __init__(self):
		self.schedule = None

class Algorithm:
	def __init__(self, name, algorithm):
		self.name = name
		self.algorithm = algorithm
		self.parameters = []

class Algo:
	def __init__(self):
		self.start = None
		self.stop = None

class EventLog:
	def __init__(self):
		self.jsonobjects = []
		self.reset()
		self.filepath = None
		self.schedule_start = None
		self.schedule_stop = None
	def reset(self):
		self.resources = []
		self.tasks = {}
		self.algos = []
		self.minstart = None
		self.maxstop = None
		self.resstop = {}
		self.schedules = []
		self.apps = []
		self.measure = []
		self.algorithm = None
		self.algorithm_parameters = []
	def get_app_id(self, taskid):
		for ix,a in enumerate(self.apps):
			if taskid in a.tids:
				return ix
		return -1
	def start_time(self):
		if self.schedule_start != None and "realtime" in self.schedule_start:
			realtime = self.schedule_start["realtime"]
			return datetime.datetime.fromtimestamp(int(realtime[0:realtime.find(".")]))
		return None
	def stop_time(self):
		start = self.start_time()
		leng = self.length()
		if start != None and leng != None:
			return start + datetime.timedelta(seconds=leng)
		return None
	def length(self):
		if self.schedule_start != None and self.schedule_stop != None:
			start = self.schedule_start["time"]
			stop = self.schedule_stop["time"]
			return float(stop) - float(start)
		return None
	def real_stop_time(self):
		start = self.start_time()
		leng = self.length()
		if start != None and leng != None:
			return start + datetime.timedelta(seconds=leng)
		return None
	def real_length(self):
		if self.schedule_start != None and self.schedule_stop != None:
			if "walltime" in self.schedule_start and "walltime" in self.schedule_stop:
				start = self.schedule_start["walltime"]
				stop = self.schedule_stop["walltime"]
			else:
				start = self.schedule_start["time"]
				stop = self.schedule_stop["time"]
			return float(stop) - float(start)
		return None
	
	def loadEvents(filepath):
		# load lines
		jsonlines = None
		with open(filepath, "r") as jsonf:
			jsonlines = jsonf.readlines()

		log = EventLog()
		log.filepath = filepath
		# load json objects from lines
		for line in jsonlines:
			#print(line)
			try:
				obj = json.load(io.StringIO(line))
				log.jsonobjects.append(obj)
			except Exception as e:
				if line != None and line != "":
					print(e)

		for o in log.jsonobjects:
			if "event" not in o:
				continue
			if o["event"] == "SCHEDULER_START":
				log.reset()
				log.schedule_start = o
			elif o["event"] == "SCHEDULER_STOP":
				log.schedule_stop = o
			elif o["event"] == "ALGORITHM":
				log.algorithm = Algorithm(o["algorithm"], o)
			elif o["event"] == "ALGORITHM_PARAM":
				log.algorithm_parameters.append(o)
			elif o["event"] == "NEWTASK":
				t = Task()
				t.tid = o["id"]
				t.dep = o["dep"]
				t.name = o["name"]
				t.size = o["size"]
				t.arrival = o["time"]
				t.checkpoints = o["checkpoints"]
				t.newtask = o
				log.tasks[t.tid] = t
			elif o["event"] == "SCHEDULE":
				s = Schedule()
				s.schedule = o
				log.schedules.append(s)
			elif o["event"] == "RESOURCES":
				log.resources = o["resources"]
			elif o["event"] == "ENDTASK":
			#	tasks.append(o)
				t = o["times"]
				#if minstart == None or t["started"] < minstart:
				#	minstart = float(t["started"])
				#if maxstop == None or t["finished"] > maxstop:
				#	maxstop = float(t["finished"])
				tid = o["id"]
				if tid in log.tasks:
					log.tasks[tid].endtask = o
					if "state" in o:
						log.tasks[tid].state = o["state"]
			elif o["event"] == "TASK_START":
				tid = o["id"]
				t = None
				if tid not in log.tasks:
					t = Task()
					log.tasks[tid] = t
				else:
					t = log.tasks[tid]
				# check if last part was finished (update message)
				if len(t.parts) > 0 and t.parts[-1].stop == None:
					continue
				p = TaskPart()
				p.start = float(o["time"]) #* 1000000000.0
				#print(tid, p.start, "START")
				p.startevent = o
				if log.minstart == None or p.start < log.minstart:
					log.minstart = float(p.start)
				p.res = o["res"]
				if p.res in resource_short:
					p.res_short = resource_short[p.res]
				else:
					p.res_short = p.res
				# set start progress if available
				if len(t.parts) > 0:
					p.startprogress = t.parts[-1].progress
				t.parts.append(p)
			elif o["event"] == "TASK_STARTED":
				tid = o["id"]
				time = float(o["time"]) #* 1000000000.0
				#print(tid, time, "STARTED")
			elif o["event"] == "TASK_SUSPENDED" or o["event"] == "TASK_FINISHED":
				tid = o["id"]
				t = log.tasks[tid]
				p = t.parts[-1]
				p.stop = float(o["time"]) #* 1000000000.0
				p.stopevent = o
				#print(tid, p.stop, o["event"])
				if o["event"] == "TASK_SUSPENDED":
					if "progress" in o:
						p.progress = o["progress"]
					else:
						p.progress = 0
				elif o["event"] == "TASK_FINISHED":
					p.progress = t.checkpoints
					t.finish = o["time"]
				if log.maxstop == None or p.stop > log.maxstop:
					log.maxstop = float(p.stop)
			elif o["event"] == "COMPUTER_ALGOSTART":
				a = Algo()
				a.start = float(o["time"])
				log.algos.append(a)
				if log.minstart == None or a.start < log.minstart:
					log.minstart = float(a.start)
			elif o["event"] == "COMPUTER_ALGOSTOP":
				log.algos[-1].stop = float(o["time"])
			elif o["event"] == "MEASURE":
				log.measure.append(o)

		# add parameters to algorithm object
		if log.algorithm != None:
			log.algorithm.parameters += log.algorithm_parameters

		# compute task stop times
		for t in log.tasks:
			for p in log.tasks[t].parts:
				r = p.startevent["res"]
				if r not in log.resstop:
					log.resstop[r] = 0.0
				if p.stop != None and p.stop > log.resstop[r]:
					log.resstop[r] = p.stop

		# default resources
		if len(log.resources) == 0:
			log.resources = ["IntelXeon","NvidiaTesla","MaxelerVectis","Scheduler"]

		log.apps = App.load_apps(log.tasks)
		# sort apps
		def min_app(a):
			return min(a.tids)
		log.apps.sort(key=min_app)

		return log

	def stat(self, metric):
		if metric == "makespan":
			return self.maxstop - self.minstart if self.maxstop != None and self.minstart != None else None
		elif metric == "mintime":
			return self.minstart
		elif metric == "maxtime":
			return self.maxstop
		elif metric == "events":
			return len(self.jsonobjects)
		return None

class WrapApp:
	def __init__(self):
		self.tasks = []
		self.name = None
		self.size = None
		self.status = None
		self.signaled = None
		self.signaled_signal = None
		self.id = None
		self.added = None
		self.started = None
		self.finished = None
		self.aborted = None
		self.state = None

class WrapLog:
	def __init__(self):
		self.filepath = None
		self.jsonobjects = []
		self.apps = []
	def loadWrapLog(filepath):
		log = None
		try:
			# load lines
			jsonlines = None
			with open(filepath, "r") as jsonf:
				jsonlines = jsonf.readlines()

			log = WrapLog()
			log.filepath = filepath

			for line in jsonlines:
				#print(line)
				try:
					obj = json.load(io.StringIO(line))
					log.jsonobjects.append(obj)
				except Exception as e:
					if line != None and line != "":
						print(e)
			for o in log.jsonobjects:
				if o["event"] == "WRAPAPP":
					app = WrapApp()
					app.tasks = o["tasks"]
					app.name = o["name"]
					app.size = o["size"]
					app.status = o["status"]
					app.signaled = o["signaled"]
					app.signaled_signal = o["signaled_signal"]
					if "endtask" in o and "id" in o["endtask"]:
						app.id = o["endtask"]["id"]
						app.added = o["endtask"]["times"]["added"]
						app.started = o["endtask"]["times"]["started"]
						app.finished = o["endtask"]["times"]["finished"]
						app.aborted = o["endtask"]["times"]["aborted"]
						app.state = o["endtask"]["state"]
					log.apps.append(app)
		except Exception as e:
			print(e)

		return log

if __name__ == "__main__":

	if len(sys.argv)<3:
		print(sys.argv[0], "eventlogfile", "stat")
		print("stats:")
		print("\t\tmakespan")
		print("\t\tmintime")
		print("\t\tmaxtime")
		print("\t\tevents")
		sys.exit(1)

	jsonfilename = sys.argv[1]
	statname = sys.argv[2]

	#print(len(jsonobjects), "events")
	log = EventLog.loadEvents(jsonfilename)

	#print(resources)
	#print("min",minstart)
	#print("max",maxstop)
	#for r in resstop:
	#	print(r, resstop[r])
	print(log.stat(statname))

