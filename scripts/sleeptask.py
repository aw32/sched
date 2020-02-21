#!/usr/bin/env python3
# Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
# SPDX-License-Identifier: BSD-2-Clause


import sys
import os
import json
import socket
import time
import errno

resources = ["IntelXeon","NvidiaTesla","MaxelerVectis"]

class UnixClient:
	def __init__(self, socketPath, tasks):
		self.socketPath = socketPath
		self.tasks = tasks
		self.socket = None
		self.buffer = bytes()
		self.pid = 0
	def connect(self):
		print("connect to "+self.socketPath)
		if self.socket == None:
			self.socket = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
			self.socket.connect(self.socketPath)
			self.send("PROTOCOL=1\00")
	def disconnect(self):
		if self.socket != None:
			self.socket.close()
			self.socket = None
	def send(self, msg):
		if self.socket != None:
			self.socket.send(msg.encode("ASCII"))
	def sendObj(self, obj):
		msg = json.dumps(obj)
		print(self.pid,"> "+msg)
		self.send(msg+"\00")
	def readObj(self):
		if self.socket != None:
			data = None
			try:
				data = self.socket.recv(1024)
				if len(data) != 0:
					self.buffer += data
			except BlockingIOError as e:
				if e.errno == errno.EAGAIN:
					pass
				else:
					print(e)
			except Exception as e:
				print(e)
			if len(self.buffer) > 0:
				end = -1
				for i in range(0, len(self.buffer)):
					if self.buffer[i] == 0x00:
						end = i
						break
				if end == -1:
					print("no end found")
					return None
				msgdata = self.buffer[0:end]
				self.buffer = self.buffer[end+1:]
				try:
					msgstr = msgdata.decode("ASCII")
				except Exception as e:
					print("decode msgdata failed: ",e)
					return None
				msg = json.loads(msgstr)
				print(self.pid,"< "+msgstr)
				return msg
	def execute(self):
		# send tasklist
		json_tasks = []
		for i,t in enumerate(self.tasks):
			if i == 0:
				json_tasks += [{"name":t.name,"size":t.size,"checkpoints":t.size,"resources":resources}]
			else:
				json_tasks += [{"name":t.name,"size":t.size,"checkpoints":t.size,"dependencies":[i-1],"resources":resources}]
		tasklist = {"msg":"TASKLIST", "tasklist":json_tasks}
		self.sendObj(tasklist)
		# get task ids
		obj = self.readObj()
		if obj != None:
			for i,t in enumerate(obj["taskids"]):
				self.tasks[i].id = t
		currenttaskix = None
		while True:
			if currenttaskix == None:
				# nothing better to do, wait for next task
				obj = self.readObj()
			else:
				self.socket.setblocking(False)
				obj = self.readObj()
				self.socket.setblocking(True)
			if obj != None:
				if obj["msg"] == "TASK_START":
					taskid = obj["id"]
					for i,t in enumerate(self.tasks):
						if t.id == taskid:
							currenttaskix = i
							break
					if currenttaskix == None:
						print("task id not found")
						continue
					# do task init
					print("task",self.tasks[currenttaskix].id,"init",self.tasks[currenttaskix].init)
					time.sleep(self.tasks[currenttaskix].init)
					# send task started
					self.sendObj({"msg":"TASK_STARTED","id":self.tasks[currenttaskix].id})
				elif obj["msg"] == "TASK_SUSPEND":
					if currenttaskix == None:
						print("task id not found")
						continue
					# do task fini
					print("task",self.tasks[currenttaskix].id,"fini",self.tasks[currenttaskix].fini)
					time.sleep(self.tasks[currenttaskix].fini)
					# send task suspended
					oldix = currenttaskix
					currenttaskix = None
					self.sendObj({"msg":"TASK_SUSPENDED","id":self.tasks[oldix].id,"progress":self.tasks[oldix].progress})
				elif obj["msg"] == "TASK_ABORT":
					abortix = -1
					taskid = obj["id"]
					for i,t in enumerate(self.tasks):
						if t.id == taskid:
							abortix = i
							break
					if abortix == -1:
						print("task id not found")
						continue
					self.tasks[abortix].progress = self.tasks[abortix].size
				elif obj["msg"] == "TASK_PROGRESS":
					taskid = obj["id"]
					for i,t in enumerate(self.tasks):
						if t.id == taskid:
							self.sendObj({"msg":"PROGRESS","id":t.id,"progress":t.progress})
				elif obj["msg"] == "QUIT":
					print("hmm...")
					break
				else:
					print("what?")
			elif obj == None:
				if currenttaskix != None:
					# compute
					if self.tasks[currenttaskix].progress < self.tasks[currenttaskix].size:
						time.sleep(self.tasks[currenttaskix].compute)
						self.tasks[currenttaskix].progress += 1
					elif self.tasks[currenttaskix].progress == self.tasks[currenttaskix].size:
						oldix = currenttaskix
						currenttaskix = None
						# do task fini
						print("task",self.tasks[oldix].id,"fini",self.tasks[oldix].fini)
						time.sleep(self.tasks[oldix].fini)
						self.sendObj({"msg":"TASK_FINISHED","id":self.tasks[oldix].id})
						# check if everything is done
						done = True
						for i,t in enumerate(self.tasks):
							if t.progress < t.size:
								done = False
								break
						if done == True:
							self.sendObj({"msg":"QUIT"})
							self.disconnect()
							break

class CTask:
	def __init__(self, params):
		self.name, self.init, self.size, self.compute, self.fini = params
		self.progress = 0
		self.resource = None
		self.id = -1

if len(sys.argv)!=2:
	print("sleeptask.py NAME,INIT,SIZE,COMPUTE,FINI;[...;]")
	print("NAME - string, else integers")
	sys.exit(1)

argstr = sys.argv[1]
args = argstr.split(";")
tasks = []
for x in args:
	if len(x) == 0:
		continue
	vals = x.split(",")
	if len(vals)!=5:
		print("Not enough values")
		sys.exit(1)
	valsn = [vals[0]]
	for y in vals[1:]:
		try:
			n = float(y)
		except Exception as e:
			print(e)
			sys.exit(1)
		valsn += [n]
	tasks += [CTask(valsn)]

client = UnixClient("/tmp/sched.socket", tasks)
client.pid = os.getpid()
client.connect()
client.execute()
