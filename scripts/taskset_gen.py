#!/usr/bin/env python3
# Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
# SPDX-License-Identifier: BSD-2-Clause



import sys
import json
import time
import subprocess
import os
import random
import csv
import copy
import math
import statistics
import tempfile
import traceback

import cdf
import msresults as msres
import plot_tod_img

debug_vars = {}

def ratio_distance(a,b):
	# a-b
	if a[0] == None and a[1] == None or b[0] == None and b[1] == None:
		return a[2] - b[2]
	if a[1] == None and a[2] == None or b[1] == None and b[2] == None:
		return a[0] - b[0]
	if a[0] == None and a[2] == None or b[0] == None and b[2] == None:
		return a[1] - b[1]
	if a[0] == None or b[0] == None:
		return math.sqrt((a[1]-b[1])**2 + (a[2]-b[2])**2)
	if a[1] == None or b[1] == None:
		return math.sqrt((a[0]-b[0])**2 + (a[2]-b[2])**2)
	if a[2] == None or b[2] == None:
		return math.sqrt((a[1]-b[1])**2 + (a[0]-b[0])**2)
	return math.sqrt((a[0]-b[0])**2 + (a[1]-b[1])**2 + (a[2]-b[2])**2)

def update_min_max(old_max,old_min,val):
	if val == None:
		return old_max, old_min
	new_max = val if old_max == None or val > old_max else old_max
	new_min = val if old_min == None or val < old_min else old_min
	return new_max, new_min


def gettime():
	try:
		return time.time_ns()
	except AttributeError:
		return time.time()*1000000000

class Task:
	def __init__(self, name, taskdef, msresults):
		self.name = name
		self.resources = taskdef["res"]
		self.definition = taskdef
		self.msresults = {} # double dict, resource -> dict, size -> MSResults
		for res in self.resources:
			sizes = msresults.task_res_sizes(self.name, res)
			if len(sizes) == 0:
				print("task",name,"No sizes for",res)
				continue
			#if name == "correlation":
			#	print(sizes)
			sizeresults = {}
			for size in sizes:
				#print(name, res, size)
				sizeindex = self.definition['sizes'][res].index(size)
				checkpoints = self.definition['checkpoints'][res][sizeindex]
				msres = msresults.result(self.name, size, res)
				if msres == None:
					print("MSResults failed for ",name,res,size)
					continue
				msres.checkpoints = checkpoints
				sizeresults[size] = msres
			self.msresults[res] = sizeresults
	def resForSize(self, size):
		res = []
		for r in self.resources:
			if size in self.msresults[r]:
				res.append(r)
		return res

class TaskOption:
	def __init__(self, task, res, size, tasktime, taskenergy, checkpoints):
		self.task = task
		self.res = res # list of resources
		self.size = size
		self.tasktime = tasktime
		self.taskenergy = taskenergy
		self.checkpoints = checkpoints
		self.dependencies = []
		self.aff_time = None
		self.aff_energy = None
		


def findTaskLength(tasks, length, maxtime):
	cur = None
	curdiff = 0.0
	#print("Length:",length)
	for t in tasks:
		#print(t)
		diff = abs(t.tasktime*maxtime-length)
		#print(length,diff)
		if cur == None or diff<curdiff:
			cur = t
			curdiff = diff
	return cur

def genStaticTaskset(tasklength_cdf, tasks, tasknum, rand, maxtime):
	gentasks = []
	for ix in range(0,tasknum):
		val = rand.random()
		ideallength = tasklength_cdf.sample(val)*maxtime
		task = findTaskLength(tasks, ideallength, maxtime)
		gentasks.append(copy.deepcopy(task))
#	# add dependencies
#	for ix in range(0,tasknum):
#		if ix == 0:
#			continue
#		# min ix for dependency to create chains
#		# dependency or not?
#		val = rand.random()
#		print(val)
#		if val < 0.7:
#			# choose dependency
#			val = rand.random()
#			dix = round(val * ix)
#			if dix == ix:
#				dix -= 1
#			gentasks[ix].dependencies = [dix]
#		else:
#			gentasks[ix].dependencies = []
	return gentasks




def bimodal(tasks, tasknum, rand):
	# generate all variations
	alltasks = []
	mintime = float("inf")
	maxtime = 0.0
	minenergy = float("inf")
	maxenergy = 0.0
	for task in tasks:
		task_sizes = []
		# get available sizes for this task
		for res in task.msresults:
			for size in task.msresults[res]:
				if size not in task_sizes:
					task_sizes.append(size)
		for size in task_sizes:
			res_time = []
			res_energy = []
			task_res = []
			# fill lists with time/energy of available resources
			for res in task.msresults:
				if size in task.msresults[res]:
					msres = task.msresults[res][size]
					tasktime = msres.avgtime[3]
					taskenergy = msres.avgenergy[2]
					res_time.append(tasktime)
					res_energy.append(taskenergy)
					task_res.append(res)
				else:
					task_res.append(None)
			# average time/energy
			tasktime = statistics.mean(res_time)
			taskenergy = statistics.mean(res_energy)
			entry = TaskOption(task, task_res, size, tasktime, taskenergy, msres.checkpoints)
			alltasks.append(entry)
			if tasktime < mintime:
				mintime = tasktime
			if tasktime > maxtime:
				maxtime = tasktime
			if taskenergy < minenergy:
				minenergy = taskenergy
			if taskenergy > maxenergy:
				maxenergy = taskenergy
	alltasks.sort(key=lambda task: task.tasktime)
	# load cdf
	tasklength_cdf = cdf.CDF.loadCDF("bimodal.cdf")
	gentasks = genStaticTaskset(tasklength_cdf, alltasks, tasknum, rand, maxtime)
	return gentasks

def resource_affinity_normal_taskset(tasks, tasknum, rand, target_aff):
	gauss_cdf = cdf.CDF.loadCDF("gauss.cdf")
	x_max, x_min = None,None
	y_max, y_min = None,None
	z_max, z_min = None,None
	#aff_max = None
	#aff_min = None
	for task in tasks:
		aff_time = task.aff_time
		#dist = ratio_distance(target_aff, aff_time)
		x_max, x_min = update_min_max(x_max,x_min,aff_time[0])
		y_max, y_min = update_min_max(y_max,y_min,aff_time[1])
		z_max, z_min = update_min_max(z_max,z_min,aff_time[2])
	x_abs_max = None
	y_abs_max = None
	z_abs_max = None
	x_diff = None
	y_diff = None
	z_diff = None
	if x_max != None:
		x_abs_max = max(abs(target_aff[0]-x_max),abs(target_aff[0]-x_min))
		x_diff = x_abs_max*2.0
	if y_max != None:
		y_abs_max = max(abs(target_aff[1]-y_max),abs(target_aff[1]-y_min))
		y_diff = y_abs_max*2.0
	if z_max != None:
		z_abs_max = max(abs(target_aff[2]-z_max),abs(target_aff[2]-z_min))
		z_diff = z_abs_max*2.0
	gentasks = []
	for ix in range(0,tasknum):
		x_val = rand.random()
		y_val = rand.random()
		z_val = rand.random()
		print("new")
		if x_max != None:
			print(x_val,target_aff[0]-x_max, target_aff[0]+x_max)
			x_val = (target_aff[0]-x_max) + gauss_cdf.sample(x_val)*x_diff
			x_val = max(0.0,min(1.0, x_val))
		else:
			x_val = None
		if y_max != None:
			print(y_val,target_aff[1]-y_max, target_aff[1]+y_max)
			y_val = (target_aff[1]-y_max) + gauss_cdf.sample(y_val)*y_diff
			y_val = max(0.0,min(1.0, y_val))
		else:
			y_val = None
		if z_max != None:
			print(z_val,target_aff[2]-z_max, target_aff[2]+z_max)
			z_val = (target_aff[2]-z_max) + gauss_cdf.sample(z_val)*z_diff
			z_val = max(0.0,min(1.0, z_val))
		else:
			z_val = None
		sample = (x_val, y_val, z_val)
		print("sample:",sample)
		d_task = None
		dist = math.inf
		for task in tasks:
			d = ratio_distance(task.aff_time, sample)
			if d_task == None or d < dist:
				print("  > ",task.task.name,task.size,"new dist:",d,"sample aff:",task.aff_time)
				d_task = task
				dist = d
		print(d_task.task.name, d_task.size, "distance: ", dist)
		gentasks.append(d_task)
	return gentasks

def res_normal(tasks, tasknum, rand, target_aff):
	alltasks = []
	for task in tasks:
		task_sizes = []
		for res in task.msresults:
			for size in task.msresults[res]:
				if size not in task_sizes:
					task_sizes.append(size)
		for size in task_sizes:
			all_time = []
			all_energy = []
			task_res = []
			# fill lists with time/energy of available resources
			
			for res in ["IntelXeon","NvidiaTesla","MaxelerVectis"]:
				if res not in task.msresults:
					all_time.append(0.0)
					all_energy.append(0.0)
					task_res.append(None)
					continue
				if size in task.msresults[res]:
					msres = task.msresults[res][size]
					tasktime = msres.avgtime[3]
					taskenergy = msres.avgenergy[2]
					all_time.append(tasktime)
					all_energy.append(taskenergy)
					task_res.append(res)
				else:
					all_time.append(0.0)
					all_energy.append(0.0)
					task_res.append(None)
			aff_time = plot_tod_img.abs2ratio(all_time)
			if aff_time[0] == 0.0:
				aff_time = (None,aff_time[1],aff_time[2])
			if aff_time[1] == 0.0:
				aff_time = (aff_time[0],None,aff_time[2])
			if aff_time[2] == 0.0:
				aff_time = (aff_time[0],aff_time[1],None)
			aff_energy = plot_tod_img.abs2ratio(all_energy)
			if aff_energy[0] == 0.0:
				aff_energy = (None,aff_time[1],aff_time[2])
			if aff_energy[1] == 0.0:
				aff_energy = (aff_time[0],None,aff_time[2])
			if aff_energy[2] == 0.0:
				aff_energy = (aff_time[0],aff_time[1],None)

			entry = TaskOption(task, task_res, size, None, None, msres.checkpoints)
			entry.aff_time = aff_time
			entry.aff_energy = aff_energy
			alltasks.append(entry)
	gentasks = resource_affinity_normal_taskset(alltasks, tasknum, rand, target_aff)
	return gentasks

def res_ilp(tasks, tasknum, rand, target_aff, task_min, task_max):
	alltasks = []
	for task in tasks:
		task_sizes = []
		for res in task.msresults:
			for size in task.msresults[res]:
				if size not in task_sizes:
					task_sizes.append(size)
		for size in task_sizes:
			all_time = []
			all_energy = []
			task_res = []
			# fill lists with time/energy of available resources
			
			for res in ["IntelXeon","NvidiaTesla","MaxelerVectis"]:
				if res not in task.msresults:
					all_time.append(0.0)
					all_energy.append(0.0)
					task_res.append(None)
					continue
				if size in task.msresults[res]:
					msres = task.msresults[res][size]
					tasktime = msres.avgtime[3]
					taskenergy = msres.avgenergy[2]
					all_time.append(tasktime)
					all_energy.append(taskenergy)
					task_res.append(res)
				else:
					all_time.append(0.0)
					all_energy.append(0.0)
					task_res.append(None)
			aff_time = plot_tod_img.abs2ratio(all_time)
			if aff_time[0] == 0.0:
				aff_time = (None,aff_time[1],aff_time[2])
			if aff_time[1] == 0.0:
				aff_time = (aff_time[0],None,aff_time[2])
			if aff_time[2] == 0.0:
				aff_time = (aff_time[0],aff_time[1],None)
			aff_energy = plot_tod_img.abs2ratio(all_energy)
			if aff_energy[0] == 0.0:
				aff_energy = (None,aff_time[1],aff_time[2])
			if aff_energy[1] == 0.0:
				aff_energy = (aff_time[0],None,aff_time[2])
			if aff_energy[2] == 0.0:
				aff_energy = (aff_time[0],aff_time[1],None)

			entry = TaskOption(task, task_res, size, None, None, msres.checkpoints)
			entry.aff_time = aff_time
			entry.aff_energy = aff_energy
			alltasks.append(entry)
	#gentasks = resource_affinity_normal_taskset(alltasks, tasknum, rand, target_aff)
	ilp = gen_ilp(alltasks, target_aff, tasknum, task_min, task_max, rand)
	solution = run_ilp(alltasks, target_aff, ilp)
	if solution == None:
		return [], []
	random.shuffle(solution, rand.random)
	gentasks = []
	for sol in solution:
		for i in range(0,sol[1]):
			gentasks.append(alltasks[sol[0]])
	return gentasks

def resource_affinity_beta_taskset(tasks, tasknum, rand, target_aff):
	print(target_aff)
	target_aff_ratio = plot_tod_img.abs2ratio(target_aff)
	beta_cdf_x = cdf.CDFBeta(target_aff_ratio[0], 8)
	beta_cdf_y = cdf.CDFBeta(target_aff_ratio[1], 8)
	beta_cdf_z = cdf.CDFBeta(target_aff_ratio[2], 8)
	global debug_vars
	debug_vars["beta_cdf_x"] = "alpha {} beta {} mean {} real mean {}".format(beta_cdf_x.alpha, beta_cdf_x.beta, beta_cdf_x.mean, beta_cdf_x.beta_dist.mean())
	debug_vars["beta_cdf_y"] = "alpha {} beta {} mean {} real mean {}".format(beta_cdf_y.alpha, beta_cdf_y.beta, beta_cdf_y.mean, beta_cdf_y.beta_dist.mean())
	debug_vars["beta_cdf_z"] = "alpha {} beta {} mean {} real mean {}".format(beta_cdf_z.alpha, beta_cdf_z.beta, beta_cdf_z.mean, beta_cdf_z.beta_dist.mean())
	print("Target affinity ratio:", target_aff_ratio)
	print("Beta x alpha:", beta_cdf_x.alpha, "beta:", beta_cdf_x.beta, "mean:", beta_cdf_x.mean, "real mean:", beta_cdf_x.beta_dist.mean())
	print("Beta y alpha:", beta_cdf_y.alpha, "beta:", beta_cdf_y.beta, "mean:", beta_cdf_y.mean, "real mean:", beta_cdf_y.beta_dist.mean())
	print("Beta z alpha:", beta_cdf_z.alpha, "beta:", beta_cdf_z.beta, "mean:", beta_cdf_z.mean, "real mean:", beta_cdf_z.beta_dist.mean())
	x_max, x_min = None,None
	y_max, y_min = None,None
	z_max, z_min = None,None
	#aff_max = None
	#aff_min = None
	for task in tasks:
		aff_time = task.aff_time
		#dist = ratio_distance(target_aff, aff_time)
		x_max, x_min = update_min_max(x_max,x_min,aff_time[0])
		y_max, y_min = update_min_max(y_max,y_min,aff_time[1])
		z_max, z_min = update_min_max(z_max,z_min,aff_time[2])
	x_abs_max = None
	y_abs_max = None
	z_abs_max = None
	x_diff = None
	y_diff = None
	z_diff = None
	if x_max != None:
		x_abs_max = max(abs(target_aff[0]-x_max),abs(target_aff[0]-x_min))
		x_diff = x_abs_max*2.0
	if y_max != None:
		y_abs_max = max(abs(target_aff[1]-y_max),abs(target_aff[1]-y_min))
		y_diff = y_abs_max*2.0
	if z_max != None:
		z_abs_max = max(abs(target_aff[2]-z_max),abs(target_aff[2]-z_min))
		z_diff = z_abs_max*2.0
	gentasks = []
	for ix in range(0,tasknum):
		x_val = rand.random()
		y_val = rand.random()
		z_val = rand.random()
		print("new")
		if x_max != None:
			print("x", x_val,target_aff[0]-x_max, target_aff[0]+x_max)
			#x_val = (target_aff[0]-x_max) + beta_cdf_x.sample(x_val)*x_diff
			#x_val = max(0.0,min(1.0, x_val))
			x_val = beta_cdf_x.sample(x_val)
		else:
			x_val = None
		if y_max != None:
			print("y", y_val,target_aff[1]-y_max, target_aff[1]+y_max)
			#y_val = (target_aff[1]-y_max) + beta_cdf_y.sample(y_val)*y_diff
			#y_val = max(0.0,min(1.0, y_val))
			y_val = beta_cdf_y.sample(y_val)
		else:
			y_val = None
		if z_max != None:
			print("z", z_val,target_aff[2]-z_max, target_aff[2]+z_max)
			#z_val = (target_aff[2]-z_max) + beta_cdf_z.sample(z_val)*z_diff
			#z_val = max(0.0,min(1.0, z_val))
			z_val = beta_cdf_z.sample(z_val)
		else:
			z_val = None
		sample = (x_val, y_val, z_val)
		print("final random value", sample)
		d_task = None
		dist = math.inf
		for task in tasks:
			d = ratio_distance(task.aff_time, sample)
			if d_task == None or d < dist:
				print("  > ",task.task.name,task.size,"new dist:",d, task.aff_time)
				d_task = task
				dist = d
		print(d_task.task.name, d_task.size, "distance: ", dist)
		print(d_task.aff_time)
		gentasks.append(d_task)
	return gentasks


def res_beta(tasks, tasknum, rand, target_aff):
	alltasks = []
	for task in tasks:
		task_sizes = []
		for res in task.msresults:
			for size in task.msresults[res]:
				if size not in task_sizes:
					task_sizes.append(size)
		for size in task_sizes:
			all_time = []
			all_energy = []
			task_res = []
			# fill lists with time/energy of available resources
			
			for res in ["IntelXeon","NvidiaTesla","MaxelerVectis"]:
				if res not in task.msresults:
					all_time.append(0.0)
					all_energy.append(0.0)
					task_res.append(None)
					continue
				if size in task.msresults[res]:
					msres = task.msresults[res][size]
					tasktime = msres.avgtime[3]
					taskenergy = msres.avgenergy[2]
					all_time.append(tasktime)
					all_energy.append(taskenergy)
					task_res.append(res)
				else:
					all_time.append(0.0)
					all_energy.append(0.0)
					task_res.append(None)
			aff_time = plot_tod_img.abs2ratio(all_time)
			if aff_time[0] == 0.0:
				aff_time = (None,aff_time[1],aff_time[2])
			if aff_time[1] == 0.0:
				aff_time = (aff_time[0],None,aff_time[2])
			if aff_time[2] == 0.0:
				aff_time = (aff_time[0],aff_time[1],None)
			aff_energy = plot_tod_img.abs2ratio(all_energy)
			if aff_energy[0] == 0.0:
				aff_energy = (None,aff_time[1],aff_time[2])
			if aff_energy[1] == 0.0:
				aff_energy = (aff_time[0],None,aff_time[2])
			if aff_energy[2] == 0.0:
				aff_energy = (aff_time[0],aff_time[1],None)

			entry = TaskOption(task, task_res, size, None, None, msres.checkpoints)
			entry.aff_time = aff_time
			entry.aff_energy = aff_energy
			alltasks.append(entry)
	gentasks = resource_affinity_beta_taskset(alltasks, tasknum, rand, target_aff)
	return gentasks

def gen_ilp(tasks, target, task_total, task_min, task_max, rand):
	ilp = "min: diff_total;\n"
	for ix,t in enumerate(target):
		ilp += "T_p"+str(ix)+" = "+str(t)+";\n";
		ilp += "diff_p"+str(ix)+" = T_p"+str(ix)+" - t_p"+str(ix)+";\n"
		ilp += "diff_p"+str(ix)+" <= diff_p"+str(ix)+"_2;"
		ilp += "- diff_p"+str(ix)+" <= diff_p"+str(ix)+"_2;"
	for ix,res in enumerate(["IntelXeon","NvidiaTesla","MaxelerVectis"]):
		ilpr = "t_p"+str(ix)+" = "
		for tix,task in enumerate(tasks):
			if res in task.task.msresults:
				if task.size in task.task.msresults[res]:
					tasktime = task.task.msresults[res][task.size].avgtime[3]
					ilpr += str(tasktime)+" x"+str(tix)
					ilpr += " + " if tix < len(tasks)-1 else ";\n"
		ilp += ilpr
	ilpd = "diff_total = "
	for ix,t in enumerate(target):
		ilpd += "diff_p"+str(ix)+"_2"
		ilpd += " + " if ix < len(target)-1 else ";\n"
	ilp += ilpd
	ilp += "0 <= x_total <= "+str(task_total)+";\n"
	ilp += "x_total = "
	for tix,task in enumerate(tasks):
		ilp += "x"+str(tix)
		ilp += " + " if tix < len(tasks)-1 else ";\n"
	x_list = []
	for tix,task in enumerate(tasks):
		x_list.append( str(task_min)+" <= x"+str(tix)+" <= "+str(task_max)+";\n" )
	random.shuffle(x_list, rand.random)
	ilp += "".join(x_list)
	for tix,task in enumerate(tasks):
		ilp += "int x"+str(tix)+";\n"
	return ilp

def run_ilp(tasks, target, ilp):
	tempf = tempfile.mkstemp(prefix="ilp_", dir="/tmp/", suffix=".lp")
	solution = None
	try:
		os.close(tempf[0])
		with open(tempf[1],"w") as f:
			f.write(ilp)
	#with tempfile.TemporaryFile(mode="w", prefix="ilp_", dir="/tmp/", suffix=".lp") as tempf:
	#	tempf.write(ilp)
		out = open(tempf[1]+".sol", "ab")
		args = ["lp_solve","-timeout","10",tempf[1]]
		process = subprocess.Popen(args, stdout=out, stderr=sys.stdout.buffer)
		process.wait()
		out.close()
		with open(tempf[1]+".sol") as f:
			solution = read_ilp_solution(tasks, target, f)
		#os.remove(tempf[1])
		#os.remove(tempf[1]+".sol")
	except Exception as e:
		print(e)
		traceback.print_exc()
	return solution

def read_ilp_solution(tasks, target, f):
	solution = []
	for line in f:
		if line[0] == "x" and line[1] != "_":
			line_parts = line.rstrip().split(" ")
			print(line_parts)
			index = int(line_parts[0][1:])
			num = int(line_parts[-1])
			solution.append((index, num))
	return solution

def resource_affinity_beta_taskset_weighted(tasks, tasknum, rand, target_aff, task_weight, sum_weight):
	print(target_aff)
	target_aff_ratio = plot_tod_img.abs2ratio(target_aff)
	beta_cdf_x = cdf.CDFBeta(target_aff_ratio[0], 8)
	beta_cdf_y = cdf.CDFBeta(target_aff_ratio[1], 8)
	beta_cdf_z = cdf.CDFBeta(target_aff_ratio[2], 8)
	global debug_vars
	debug_vars["beta_cdf_x"] = "alpha {} beta {} mean {} real mean {}".format(beta_cdf_x.alpha, beta_cdf_x.beta, beta_cdf_x.mean, beta_cdf_x.beta_dist.mean())
	debug_vars["beta_cdf_y"] = "alpha {} beta {} mean {} real mean {}".format(beta_cdf_y.alpha, beta_cdf_y.beta, beta_cdf_y.mean, beta_cdf_y.beta_dist.mean())
	debug_vars["beta_cdf_z"] = "alpha {} beta {} mean {} real mean {}".format(beta_cdf_z.alpha, beta_cdf_z.beta, beta_cdf_z.mean, beta_cdf_z.beta_dist.mean())
	print("Target affinity ratio:", target_aff_ratio)
	print("Beta x alpha:", beta_cdf_x.alpha, "beta:", beta_cdf_x.beta, "mean:", beta_cdf_x.mean, "real mean:", beta_cdf_x.beta_dist.mean())
	print("Beta y alpha:", beta_cdf_y.alpha, "beta:", beta_cdf_y.beta, "mean:", beta_cdf_y.mean, "real mean:", beta_cdf_y.beta_dist.mean())
	print("Beta z alpha:", beta_cdf_z.alpha, "beta:", beta_cdf_z.beta, "mean:", beta_cdf_z.mean, "real mean:", beta_cdf_z.beta_dist.mean())
	x_max, x_min = None,None
	y_max, y_min = None,None
	z_max, z_min = None,None
	#aff_max = None
	#aff_min = None
	for task in tasks:
		aff_time = task.aff_time
		#dist = ratio_distance(target_aff, aff_time)
		x_max, x_min = update_min_max(x_max,x_min,aff_time[0])
		y_max, y_min = update_min_max(y_max,y_min,aff_time[1])
		z_max, z_min = update_min_max(z_max,z_min,aff_time[2])
	x_abs_max = None
	y_abs_max = None
	z_abs_max = None
	x_diff = None
	y_diff = None
	z_diff = None
	if x_max != None:
		x_abs_max = max(abs(target_aff[0]-x_max),abs(target_aff[0]-x_min))
		x_diff = x_abs_max*2.0
	if y_max != None:
		y_abs_max = max(abs(target_aff[1]-y_max),abs(target_aff[1]-y_min))
		y_diff = y_abs_max*2.0
	if z_max != None:
		z_abs_max = max(abs(target_aff[2]-z_max),abs(target_aff[2]-z_min))
		z_diff = z_abs_max*2.0
	gentasks = []
	sum_task_set = (0.0, 0.0, 0.0)
	for ix in range(0,tasknum):
		x_val = rand.random()
		y_val = rand.random()
		z_val = rand.random()
		print("new")
		if x_max != None:
			print("x", x_val,target_aff[0]-x_max, target_aff[0]+x_max)
			#x_val = (target_aff[0]-x_max) + beta_cdf_x.sample(x_val)*x_diff
			#x_val = max(0.0,min(1.0, x_val))
			x_val = beta_cdf_x.sample(x_val)
		else:
			x_val = None
		if y_max != None:
			print("y", y_val,target_aff[1]-y_max, target_aff[1]+y_max)
			#y_val = (target_aff[1]-y_max) + beta_cdf_y.sample(y_val)*y_diff
			#y_val = max(0.0,min(1.0, y_val))
			y_val = beta_cdf_y.sample(y_val)
		else:
			y_val = None
		if z_max != None:
			print("z", z_val,target_aff[2]-z_max, target_aff[2]+z_max)
			#z_val = (target_aff[2]-z_max) + beta_cdf_z.sample(z_val)*z_diff
			#z_val = max(0.0,min(1.0, z_val))
			z_val = beta_cdf_z.sample(z_val)
		else:
			z_val = None
		sample = (x_val, y_val, z_val)
		print("final random value", sample)
		best_task = None
		task_dist = math.inf
		sum_dist = math.inf
		overall_dist = math.inf
		for task in tasks:
			new_sum_task_set = (
				sum_task_set[0] + task.tasktime[0],
				sum_task_set[1] + task.tasktime[1],
				sum_task_set[2] + task.tasktime[2]
				)
			new_sum_ratio = plot_tod_img.abs2ratio(new_sum_task_set)
			sum_d = ratio_distance(target_aff_ratio, new_sum_ratio)
			task_d = ratio_distance(task.aff_time, sample)
			all_d = sum_weight * sum_d + task_weight * task_d
			if best_task == None or all_d < overall_dist:
				print("  > ",task.task.name,task.size,"new task dist:",task_d, "new sum dist:", sum_d, "new all dist:", all_d, task.aff_time)
				best_task = task
				task_dist = task_d
				sum_dist = sum_d
				overall_dist = all_d
		print(best_task.task.name, best_task.size, "all distance: ", overall_dist)
		print(best_task.aff_time)
		gentasks.append(best_task)
		sum_task_set = (
			sum_task_set[0] + best_task.tasktime[0],
			sum_task_set[1] + best_task.tasktime[1],
			sum_task_set[2] + best_task.tasktime[2]
			)
	return gentasks


def res_beta_weighted(tasks, tasknum, rand, target_aff, task_weight, sum_weight):
	alltasks = []
	for task in tasks:
		task_sizes = []
		for res in task.msresults:
			for size in task.msresults[res]:
				if size not in task_sizes:
					task_sizes.append(size)
		for size in task_sizes:
			all_time = []
			all_energy = []
			task_res = []
			# fill lists with time/energy of available resources
			
			for res in ["IntelXeon","NvidiaTesla","MaxelerVectis"]:
				if res not in task.msresults:
					all_time.append(0.0)
					all_energy.append(0.0)
					task_res.append(None)
					continue
				if size in task.msresults[res]:
					msres = task.msresults[res][size]
					tasktime = msres.avgtime[3]
					taskenergy = msres.avgenergy[2]
					all_time.append(tasktime)
					all_energy.append(taskenergy)
					task_res.append(res)
				else:
					all_time.append(0.0)
					all_energy.append(0.0)
					task_res.append(None)
			aff_time = plot_tod_img.abs2ratio(all_time)
			if aff_time[0] == 0.0:
				aff_time = (None,aff_time[1],aff_time[2])
			if aff_time[1] == 0.0:
				aff_time = (aff_time[0],None,aff_time[2])
			if aff_time[2] == 0.0:
				aff_time = (aff_time[0],aff_time[1],None)
			aff_energy = plot_tod_img.abs2ratio(all_energy)
			if aff_energy[0] == 0.0:
				aff_energy = (None,aff_time[1],aff_time[2])
			if aff_energy[1] == 0.0:
				aff_energy = (aff_time[0],None,aff_time[2])
			if aff_energy[2] == 0.0:
				aff_energy = (aff_time[0],aff_time[1],None)

			entry = TaskOption(task, task_res, size, None, None, msres.checkpoints)
			entry.aff_time = aff_time
			entry.tasktime = all_time
			entry.aff_energy = aff_energy
			entry.taskenergy = all_energy
			alltasks.append(entry)
	gentasks = resource_affinity_beta_taskset_weighted(alltasks, tasknum, rand, target_aff, task_weight, sum_weight)
	return gentasks



def printHelp():
	print(sys.argv[0],"command","[options]")

	print("\tcommand: one of")
	print("\t\tbimodal: create bimodal taskset (task average runtime over all resources is bimodal distributed)")
	print("\t\t\toptions:","taskset-file","seed", "tasknum", )
	print("\t\tres-normal: create resource affinity taskset")
	print("\t\t\toptions:","taskset-file","seed", "tasknum", "res-affinity")
	print("\t\tres-beta: create resource affinity taskset")
	print("\t\t\toptions:","taskset-file","seed", "tasknum", "res-affinity")
	print("\t\tres-beta-weighted: create resource affinity taskset")
	print("\t\t\toptions:","taskset-file","seed", "tasknum", "res-affinity", "task-affinity-weight", "sum-affinity-weight")
	print("\t\tres-ilp: create resource affinity taskset using ilp")
	print("\t\t\toptions:","taskset-file","seed", "tasknum", "res-affinity","min_task","max_task")
	print("\toptions:")
	print("\t\ttaskset-file: path to output file")
	print("\t\tseed: seed used to initialize random number generator")
	print("\t\ttasknum: number of tasks to create")
	print("\t\tres-affinity: 3 resource runtimes")
	print("\t\ttask-affinity-weight: weight of task affinity, distance (target affinity, sample affinity)")
	print("\t\tsum-affinity-weight: weight of sum affinity, distance (target affinity, affinity of sum + sample)")


if __name__ == "__main__":
	if len(sys.argv) < 5:
		printHelp()
		sys.exit(-1)

	command = sys.argv[1]
	if command not in ["bimodal", "res-normal", "res-ilp", "res-beta", "res-beta-weighted"]:
		print("Unknown command")
		printHelp()
		sys.exit(-1)


	if "SCHED_HOST" not in os.environ:
		print("SCHED_HOST variable not found")
		sys.exit(-1)
	if "SCHED_RESULTS" not in os.environ:
		print("SCHED_RESULTS variable not found")
		sys.exit(-1)

	msresults = msres.MSResults.load_results()

	deffilepath = os.path.join(os.environ["SCHED_RESULTS"],
		os.environ["SCHED_HOST"], "tasks.def")
	mspath = os.path.join(os.environ["SCHED_RESULTS"],
		os.environ["SCHED_HOST"], "ms_results")

	#deffilepath = sys.argv[2]
	setfilepath = sys.argv[2]
	seed = int(sys.argv[3])
	tasknum = int(sys.argv[4])

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

	rand = random.Random()
	rand.seed(seed)

	# read tasks
	tasks = []
	for name in taskdefinitions:
		t = Task(name, taskdefinitions[name], msresults)
		tasks.append(t)

	gentasks = None
	gendates = None
	parameters = None
	if command == "bimodal":
		gentasks = bimodal(tasks, tasknum, rand)
		parameters = {}
		parameters["gentaskset"] = "bimodal"
		parameters["tasknum"] = tasknum
		parameters["randomseed"] = seed
	elif command == "res-normal":
		aff1 = float(sys.argv[5])
		aff2 = float(sys.argv[6])
		aff3 = float(sys.argv[7])
		aff = (aff1, aff2, aff3)
		gentasks = res_normal(tasks, tasknum, rand, aff)
		parameters = {}
		parameters["gentaskset"] = "res-normal"
		parameters["tasknum"] = tasknum
		parameters["randomseed"] = seed
		parameters["affinity"] = aff
	elif command == "res-beta":
		aff1 = float(sys.argv[5])
		aff2 = float(sys.argv[6])
		aff3 = float(sys.argv[7])
		aff = (aff1, aff2, aff3)
		gentasks = res_beta(tasks, tasknum, rand, aff)
		parameters = {}
		parameters["gentaskset"] = "res-beta"
		parameters["tasknum"] = tasknum
		parameters["randomseed"] = seed
		parameters["affinity"] = aff
	elif command == "res-beta-weighted":
		aff1 = float(sys.argv[5])
		aff2 = float(sys.argv[6])
		aff3 = float(sys.argv[7])
		aff = (aff1, aff2, aff3)
		task_weight = float(sys.argv[8])
		sum_weight = float(sys.argv[9])
		gentasks = res_beta_weighted(tasks, tasknum, rand, aff, task_weight, sum_weight)
		parameters = {}
		parameters["gentaskset"] = "res-beta-weighted"
		parameters["tasknum"] = tasknum
		parameters["randomseed"] = seed
		parameters["affinity"] = aff
		parameters["task-weight"] = task_weight
		parameters["sum-weight"] = sum_weight
	elif command == "res-ilp":
		aff1 = float(sys.argv[5])
		aff2 = float(sys.argv[6])
		aff3 = float(sys.argv[7])
		aff = (aff1, aff2, aff3)
		task_min = float(sys.argv[8])
		task_max = float(sys.argv[9])
		gentasks = res_ilp(tasks, tasknum, rand, aff, task_min, task_max)
		parameters = {}
		parameters["gentaskset"] = "res-ilp"
		parameters["tasknum"] = tasknum
		parameters["randomseed"] = seed
		parameters["affinity"] = aff
		parameters["task_min"] = task_min
		parameters["task_max"] = task_max


	# read taskset file
	setfile = None
	setfile = open(setfilepath,"w")
	setfile.write("[\n")
	parameters["type"] = "PARAMETERS"
	for key in debug_vars:
		parameters[key] = debug_vars[key]
	setfile.write(json.dumps(parameters)+"\n")
	for ix,t in enumerate(gentasks):
		setfile.write(',{"type":"TASKDEF"')
		setfile.write(',"id":%d' % ix)
		setfile.write(',"name":"%s"' % t.task.name)
		setfile.write(',"size":%d' % t.size)
		setfile.write(',"checkpoints":%d' % t.checkpoints)
		setfile.write(',"dependencies":%s' %  str(t.dependencies))
		setfile.write(',"resources":[%s]' % ("\""+"\",\"".join(t.task.resForSize(t.size))+"\""))
		setfile.write('}\n')
	setfile.write("]\n")
	setfile.close()


