#!/bin/python3
# Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
# SPDX-License-Identifier: BSD-2-Clause


import os
import os.path
import sys
import simstats

tasknumlist=list(range(40,90,10))
arrtimedifflist=[1, 2, 4, 8]
tasksperapplist=[5, 10, 15, 20]
repeatlist=list(range(1,4))

if __name__ == "__main__":

	if len(sys.argv)<2:
		print(sys.argv[0],"algolistfile")
		sys.exit(1)


	algolistfile = sys.argv[1]
	algolist = simstats.loadAlgos(os.path.join(os.getcwd(),algolistfile))

	# load all logs
	logs = {}
	for tasknum in tasknumlist:
		tasknum_d = {}
		logs[tasknum] = tasknum_d
		for arrtimediff in arrtimedifflist:
			arrtimed_d = {}
			tasknum_d[arrtimediff] = arrtimed_d
			for tasksperapp in tasksperapplist:
				tasksperapp_d = {}
				arrtimed_d[tasksperapp] = tasksperapp_d
				for repeat in repeatlist:
					repeat_d = {}
					tasksperapp_d[repeat] = repeat_d
					for algo in algolist:
						simfilepath = os.path.join(os.getcwd(),"%d_%d_%d_%d" % (tasknum,arrtimediff,tasksperapp,repeat),algo,"simlogs","sched.simlog")
						logfilepath = os.path.join(os.getcwd(),"%d_%d_%d_%d" % (tasknum,arrtimediff,tasksperapp,repeat),algo,"logs","sched.eventlog")
						simlog = None
						log = None
						if os.path.exists(simfilepath):
							simlog = simstats.EventLog.loadEvents(simfilepath)
						if os.path.exists(logfilepath):
							log = simstats.EventLog.loadEvents(logfilepath)
						repeat_d[algo] = (log,simlog)

	# compute algo average makespan
	algoavg = {}
	for algo in algolist:
		avg = 0.0
		num = 0
		for tasknum in tasknumlist:
			for arrtimediff in arrtimedifflist:
				for tasksperapp in tasksperapplist:
					for repeat in repeatlist:
						(log,simlog) = logs[tasknum][arrtimediff][tasksperapp][repeat][algo]
						if simlog != None:
							ms = simlog.stat("makespan")
							if ms != None:
								avg += ms
								num += 1
		algoavg[algo] = avg/num if num != 0 else 0.0
		print(algo, num, avg/num if num != 0 else 0.0)
