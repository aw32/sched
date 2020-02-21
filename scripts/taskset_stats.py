#!/bin/env python3
# Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
# SPDX-License-Identifier: BSD-2-Clause


import sys
import json
import math
import statistics

import msresults
import plot_tod_img

def aff_mean(aff_list):
	x = []
	y = []
	z = []
	for aff in aff_list:
		if aff[0] != 0.0:
			x.append(aff[0])
		if aff[1] != 0.0:
			y.append(aff[1])
		if aff[2] != 0.0:
			z.append(aff[2])
	#print(x,y,z)
	x_mean = statistics.mean(x) if len(x) > 0 else 0.0
	y_mean = statistics.mean(y) if len(y) > 0 else 0.0
	z_mean = statistics.mean(z) if len(z) > 0 else 0.0
	return (x_mean, y_mean, z_mean)

def reverse_ratio(ratio):
	ret = []
	for x in ratio:
		ret += [x if x == 0.0 else 1.0/x]
	return tuple(ret)

if __name__ == "__main__":

	if len(sys.argv) < 2:
		print(sys.argv[0], "taskset-file")
		sys.exit(1)

	# load taskset
	tasksetpath = sys.argv[1]
	taskset = None
	try:
		with open(tasksetpath, "r") as f:
			fcontent = f.read()
			taskset = json.loads(fcontent)
	except Exception as e:
		print("Failed to open taskset file", tasksetpath)
		print(e)
		sys.exit(1)

	# load results
	msresults = msresults.MSResults.load_results()

	tasks = []
	tasks_time = []
	tasks_energy = []
	tasks_time_affinity = []
	tasks_energy_affinity = []
	tasks_num = 0
	tasks_all_time = []
	tasks_all_energy = []
	for entry in taskset:
		if entry["type"] != "TASKDEF":
			continue
		tasks_num += 1
		msresult = {}
		time = []
		energy = []
		all_time = []
		all_energy = []
		for res in entry["resources"]:
			msresult[res] = msresults.result(entry["name"], entry["size"], res)
		for res in ["IntelXeon","NvidiaTesla","MaxelerVectis"]:
			if res not in msresult:
				all_time.append(0.0)
				all_energy.append(0.0)
			else:
				result = msresult[res]
				time.append(result.avgtime[3])
				all_time.append(result.avgtime[3])
				energy.append(result.avgenergy[2])
				all_energy.append(result.avgenergy[2])
		tasks_time.append(statistics.mean(time))
		tasks_energy.append(statistics.mean(energy))
		aff_time = plot_tod_img.abs2ratio(all_time)
		aff_energy = plot_tod_img.abs2ratio(all_energy)
		tasks_time_affinity.append(aff_time)
		tasks_energy_affinity.append(aff_energy)
		tasks_all_time.append(all_time)
		tasks_all_energy.append(all_energy)
		print("{0:>10} \t {1:>10} \t {2:}".format(entry["name"], entry["size"], aff_time))

	print("Number of tasks: ", tasks_num)
	print("Time mean: ", statistics.mean(tasks_time))
	print("Energy mean: ", statistics.mean(tasks_energy))
	aff_time_mean = aff_mean(tasks_time_affinity)
	tasks_all_time_comp = list(zip(*tasks_all_time))
	time_sum = sum(tasks_all_time_comp[0]), sum(tasks_all_time_comp[1]), sum(tasks_all_time_comp[2])
	print("Time sum: ", time_sum)
	print("Time sum ratio: ", plot_tod_img.abs2ratio(time_sum))
	print("Time sum ratio reverse: ", reverse_ratio(plot_tod_img.abs2ratio(time_sum)))
	print("Time affinity mean: ", aff_time_mean)
	print("Time affinity mean reverse: ", reverse_ratio(aff_time_mean))
	aff_energy_mean = aff_mean(tasks_energy_affinity)
	tasks_all_energy_comp = list(zip(*tasks_all_energy))
	print("Energy sum: ", sum(tasks_all_energy_comp[0]), sum(tasks_all_energy_comp[1]), sum(tasks_all_energy_comp[2]))
	print("Energy affinity mean: ", aff_energy_mean)


