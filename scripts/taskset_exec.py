#!/usr/bin/env python3
# Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
# SPDX-License-Identifier: BSD-2-Clause


import sys
import json
import time
import subprocess
import os
import os.path
import tempfile

def gettime():
	try:
		return time.time_ns()
	except AttributeError:
		return time.time()*1000000000

class TaskWrap:
	def __init__(self,number,tasks, definitions):
		self.id = number
		self.tasks = tasks
		self.tdefpath = deffilepath
		self.wrapfile = None
		self.wrapsocket = None
		self.definitions = definitions
		self.create()
	def name(self):
		return "wrap"+str(self.id)
	def create(self):
		# create wrapfile
		#self.wrapfile = tempfile.mkstemp(prefix="wrap")
		self.wrapfile = os.path.join(wrapfilepath,"wrap"+str(self.id)+".groupconf")
		wraptasks = []
		for ix,t in enumerate(self.tasks):
			task = {}
			task['name'] = t['name']
			task['tasks'] = [ix]
			dep = []
			for d in t['dependencies']:
				for ix2,t2 in enumerate(self.tasks):
					if t2['id'] == d:
						dep.append(ix2)
			task['dependencies'] = [dep]
			task['exec'] = True
			task['size'] = t['size']
			wraptasks.append(task)
		# write wrapfile
		with open(self.wrapfile, 'w') as f:
			f.write(json.dumps(wraptasks))
		# create wrap socket file
		# use tempfile to create random temporary file
		# remove created regular file and use file name for wrap socket
		self.wrapsocket = tempfile.mkstemp(prefix="wrapsocket")
		os.close(self.wrapsocket[0])
		os.remove(self.wrapsocket[1])
	def destroy(self):
		if self.wrapsocket != None:
			# remove wrap socket in case the wrap process failed
			if os.path.exists(self.wrapsocket[1]) == True:
				try:
					os.remove(self.wrapsocket[1])
				except Exception as e:
					print(e)
			self.wrapsocket = None
	def exec(self):
		args = [wrappath]
		env = os.environ.copy()
		env['SCHED_CONFIG'] = wrapconfigpath
		env['WRAP_FILE'] = self.wrapfile
		env['WRAP_SOCKET'] = self.wrapsocket[1]
		env['WRAP_LOGPREFIX'] = os.path.join(logpath,"wrap"+str(self.id))
		env['SCHED_SOCKET'] = schedsocket
		env['SCHED_TASKDEF'] = self.tdefpath
		env['SCHED_LOG'] = os.path.join(logpath,"wrap"+str(self.id)+".log")
		env['SCHED_EVENTLOG'] = os.path.join(logpath,"wrap"+str(self.id)+".eventlog")
		wd = env['PWD']
		# example call
		# SCHED_CONFIG=wrapconfig.yml WRAP_FILE=wrap.json SCHED_SOCKET=/tmp/sched.socket WRAP_SOCKET=/tmp/wrap.socket SCHED_TASKDEF=../tasks_mig.def ../build/wrap
		self.process_out = open(os.path.join(logpath,"wrap"+str(self.id)+".log"),"ab")
		self.process_err = open(os.path.join(logpath,"wrap"+str(self.id)+".err"),"ab")
		#print(args, env)
		self.process = subprocess.Popen(args, cwd=wd, env=env, stdout=self.process_out, stderr=self.process_err)
		#self.process = subprocess.Popen(args, cwd=wd, env=env, stdout=sys.stdout.buffer, stderr=sys.stdout.buffer)
	def close(self):
		self.process.wait()
		self.process_out.close()
		self.process_err.close()
		self.destroy()
		return self.process.returncode
	def print(self, time):
		print("Taskgroup:")
		for t in self.tasks:
			print("","%d" % time, printTask(t, self.definitions))

class TaskAction:
	def __init__(self,json,tasks,definitions):
		self.reltime = json["time"]
		self.tasks = []
		self.definitions = definitions
		for tid in json["tasks"]:
			action = tasks[tid]
			self.tasks.append(action)
	def check(self):
		for action in self.tasks:
			if action["name"] not in self.definitions:
				print("Task",action["id"],"not defined in task definitions")
				return False
			tdef = self.definitions[action["name"]]
			for r in action["resources"]:
				if r not in tdef["res"]:
					print("Task",action["id"],"Resource conflict with task definitions")
					return False
				# empty sizes array means every size is accepted
				#if len(tdef["sizes"][r]) != 0 and action["size"] not in tdef["sizes"][r]:
				#	print("Task",action["id"],"size not contained in task definitions")
				#	return False
		return True
	def createProcess(self, number):
		if len(self.tasks) > 1:
			p = TaskWrap(number, self.tasks,self.definitions)
		else:
			p = TaskProc(self.tasks[0], self.definitions)
		return p

class TaskProc:
	def __init__(self, task, definitions):
		self.task = task
		self.definitions = definitions
		self.taskdef = definitions[task['name']]
	def name(self):
		return str(self.task["name"])+str(self.task["size"])
	def close(self):
		self.process.wait()
		self.process_out.close()
		self.process_err.close()
		return self.process.returncode
	def exec(self):
		args = self.taskdef["arg"]
		wd = None
		if "wd" in self.taskdef:
			wd = self.taskdef["wd"]
		env = os.environ.copy()
		if "env" in self.taskdef:
			for e in self.taskdef["env"]:
				ename = e.split("=")[0]
				evalue = e[e.find("=")+1:]
				env[ename] = evalue
		env["SCHED_TASKSIZE"] = str(self.task["size"])
		#p = Proc()
		self.process_out = open(os.path.join(logpath,str(self.task["id"])+".log"),"ab")
		self.process_err = open(os.path.join(logpath,str(self.task["id"])+".err"),"ab")
		self.process = subprocess.Popen(args, cwd=wd, env=env, stdout=self.process_out, stderr=self.process_err,)
	def print(self, time):
		print("%d" % time, printTask(self.task, self.definitions))



def printTask(task, definitions):
	d = definitions[task["name"]]
	env = "SCHED_TASKSIZE="+str(task["size"])+" "
	if "env" in d:
		for ix,e in enumerate(d["env"]):
			env += e+" "
	arg = " ".join(d["arg"])
	return env+arg

def execActions(definitions, taskactions, sim):
	starttime = gettime()
	todo = [] + taskactions
	processlist = []
	currenttime = gettime()
	print("%d" % currenttime, "START")
	stop = False
	actionnum = 0
	try:
		while len(todo) > 0:
			action = todo[0]
			todo = todo[1:]
			sleeptime = action.reltime - (currenttime-starttime)
			if sleeptime > 0:
				try:
					time.sleep(sleeptime/1000000000.0)
				except KeyboardInterrupt:
					stop = True
					print("STOP")
					break
			currenttime = gettime()
			if sim == False:
				p = action.createProcess(actionnum)
				processlist.append(p)
				p.exec()
				p.print(currenttime)
				#print("%d" % currenttime, printTask())
				#processlist.append(execTask(t, taskdef))
			actionnum += 1
	except KeyboardInterrupt:
		stop = True
		currenttime = gettime()
		print("%d" % currenttime, "STOP")
	print("%d" % currenttime,"DONE")
	# wait for processes
	returncode = 0
	for p in processlist:
		rc = p.close()
		print(p.name(), rc)
		if rc != 0:
			returncode = 1
	return returncode

def printActions(definitions, taskactions):
	for a in taskactions:
		print("> ",a.reltime)
		for t in a.tasks:
			print("  ",printTask(t, definitions))

def printHelp():
	print(sys.argv[0],"command","taskdef-file","taskset-file")
	print("\t\tcommand: one of")
	print("\t\t\texec: execute the taskset")
	print("\t\t\tprint: print the actions")
	print("\t\t\tsim: like print but with timings as with exec")
	print("\t\ttaskdef-file: json file containing task definitions")
	print("\t\ttaskset-file: json file containing taskset")
	print("ENV:")
	print("\t\tSCHED_SOCKET: path to scheduler socket")
	print("\t\tWRAP_PATH: path for wrap executable")
	print("\t\tLOG_PATH: path for logfiles")
	print("\t\tWRAP_FILE_PATH: path for wrap group files")


if __name__ == "__main__":
	if len(sys.argv) < 4:
		printHelp()
		sys.exit(-1)

	command = sys.argv[1]
	if command not in ["exec","print","sim"]:
		print("Unknown command")
		printHelp()
		sys.exit(-1)

	deffilepath = sys.argv[2]
	setfilepath = sys.argv[3]
	schedsocket = os.environ["SCHED_SOCKET"] if "SCHED_SOCKET" in os.environ else ""
	wrapconfigpath = os.environ["WRAP_CONFIG"] if "WRAP_CONFIG" in os.environ else ""
	wrappath = os.environ["WRAP_PATH"] if "WRAP_PATH" in os.environ else ""
	logpath = os.environ["LOG_PATH"] if "LOG_PATH" in os.environ else ""
	wrapfilepath = os.environ["WRAP_FILE_PATH"] if "WRAP_FILE_PATH" in os.environ else ""

	# read definitions file
	taskdefinitions = None
	try:
		with open(deffilepath, "r") as f:
			fcontent = f.read()
			taskdefinitions = json.loads(fcontent)
	except Exception as e:
		print("Failed to open definitions file",deffilepath)
		print(e)
		sys.exit(-1)

	# read taskset file
	setfile = None
	try:
		with open(setfilepath, "r") as f:
			fcontent = f.read()
			setfile = json.loads(fcontent)
	except Exception as e:
		print("Failed to open taskset file",setfilepath)
		print(e)
		sys.exit(-1)

	# create simtask dictionary for fast access by id
	# and task action list
	settasks = {}
	setactions = []
	for e in setfile:
		if e["type"] == "TASKDEF":
			settasks[e["id"]] = e
		elif e["type"] == "TASKREG":
			action = TaskAction(e,settasks,taskdefinitions)
			if action.check() == False:
				print("Check failed")
				sys.exit(-1)
			setactions.append(action)

	# sort by action time
	setactions.sort(key=lambda o: o.reltime)

	returncode = 0
	if command == "print":
		printActions(taskdefinitions, setactions)
	elif command == "sim":
		execActions(taskdefinitions, setactions, sim=True)
	elif command == "exec":
		returncode = execActions(taskdefinitions, setactions, sim=False)
	sys.exit(returncode)


