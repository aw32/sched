#!/usr/bin/env python3
# Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
# SPDX-License-Identifier: BSD-2-Clause


# Creates plots for scheduler event logs
# * experiment gantt chart displays tasks events
# * schedule gantt chart displays tasks as they are planned in a schedule
# * task lifeline chart displays resource allocation per task
# * taskset triangle chart displays resource affinity triangle for all event log tasks

import sys
import io
import math
import matplotlib.pyplot as plt
import matplotlib.lines
import matplotlib.patches
import xml.etree.ElementTree as ET

import plot_tod
import log_stats
import msresults as msres

#https://stackoverflow.com/questions/34387893/output-matplotlib-figure-to-svg-with-text-as-text-not-curves
#https://matplotlib.org/users/customizing.html
#svg.fonttype : 'path'         # How to handle SVG fonts:
#    'none': Assume fonts are installed on the machine where the SVG will be viewed.
#    'path': Embed characters as paths -- supported by most SVG renderers
#    'svgfont': Embed characters as SVG fonts -- supported only by Chrome,
#               Opera and Safari
#plt.rcParams['svg.fonttype'] = 'none'

RES_BAR_HEIGHT = 0.8
RES_BAR_MARGIN = 0.2

TASK_BAR_WIDTH = 0.8
TASK_BAR_MARGIN = 0.5

#FIGSIZE = [6.4, 4.8] # default rcParams["figure.figsize"]
FIGSIZE = [10, 4.8]

RES_LIST = ["IntelXeon","NvidiaTesla","MaxelerVectis"]
RES_COLOR = [(0.0,0.0,1.0,1.0),(1.0,0.0,0.0,1.0),(0.0,1.0,0.0,1.0)]

dep_color_cycle = [
	(0.9,0.9,0.9),
	(0.1,0.1,0.1),
	(0.8,0.8,0.8),
	(0.2,0.2,0.2),
	(0.7,0.7,0.7),
	(0.3,0.3,0.3),
	(0.6,0.6,0.6),
	(0.4,0.4,0.4)
	]

color_cycle = plt.rcParams['axes.prop_cycle'].by_key()['color']

# Plots the task rectangle in a gantt chart
class GanttBar:
	def __init__(self, barid, x, y, width, height, partix, parts, taskid, color, progress):
		self.id = barid
		self.x = x
		self.y = y
		self.width = width
		self.height = height
		self.partix = partix
		self.parts = parts
		self.taskid = taskid
		self.color = color
		self.progress = progress
	def draw_taskid(self, fig, ax):
		idbb = get_text_bb(fig, ax, self.taskid)
		if self.width > idbb.width:
			tx = self.x + (self.width/2.0) - (idbb.width/2.0)
			#ty = self.y
			ty = self.y + RES_BAR_HEIGHT*0.5 - (idbb.height/2.0) - 0.1
			ax.text( tx, ty , self.taskid, va="center")
	def draw_progress(self, fig, ax):
		progtext = "{0:2.1f}".format(self.progress)
		idbb = get_text_bb(fig, ax, progtext)
		if self.width > idbb.width:
			tx = self.x + (self.width/2.0) - (idbb.width/2.0)
			ty = self.y - RES_BAR_HEIGHT*0.5 + (idbb.height/2.0) + 0.1
			ax.text( tx, ty , progtext, va="center")

	def draw_bar(self, ax):
		ax.barh(self.y, 
			self.width, 
			height=self.height, 
			left=self.x, 
			color=self.color, 
			gid=self.id, 
			edgecolor="black")
	def draw_depcirc(self, ax):
		if self.partix == 0 or self.partix == self.parts-1:
			x = self.x + self.width/2.0
			y = self.y
			color = (0.0,0.0,1.0,1.0)
			ax.scatter(x,y, s=10, facecolor=color, edgecolor=(0.0,0.0,0.0,1.0), zorder=2)

# Plots the migration arrow linking two task rectangles
class MigArrow:
	def __init__(self, srcid, dstid):
		self.srcid = srcid
		self.dstid = dstid
	def draw_arrow(self, ax, src, dst):
		ydiff = -1 if src.y > dst.y else 1
		srcmidx = src.x + (src.width/2.0)
		dstmidx = dst.x + (dst.width/2.0)
		x1 = src.x + src.width
		y1 = src.y + RES_BAR_HEIGHT*0.5*ydiff
		x2 = dst.x
		y2 = dst.y + RES_BAR_HEIGHT*0.5*ydiff*-1.0
		arrow_color = (1.0,0.0,0.0,1.0)
		cfc = (1.0,1.0,1.0,1.0) # circle face color
		cec = (0.0,0.0,0.0,1.0) # circle edge color
		if src.y != dst.y:
			# different resource
			x1 -= 0.04
			x1 = srcmidx if x1 < srcmidx else x1
			y1 += 0.04*ydiff*-1.0
			x2 += 0.04
			x2 = dstmidx if x2 > dstmidx else x2
			y2 += 0.04*ydiff
			distance = x2-x1
			rad = 0
			conn = matplotlib.patches.ConnectionStyle.Arc3(rad=rad)
			arr = matplotlib.patches.ArrowStyle("Fancy", head_length=4, head_width=8, tail_width=2)
			fancy_arrow = matplotlib.patches.FancyArrowPatch((x1,y1), (x2, y2), 
				arrowstyle=arr, connectionstyle=conn, linewidth=0.5, 
				facecolor=arrow_color, edgecolor=(0.0,0.0,0.0,1.0), zorder=3)
			ax.scatter(x1,y1, s=10, facecolor=cfc, edgecolor=cec, zorder=2)
			ax.scatter(x2,y2, s=10, facecolor=cfc, edgecolor=cec, zorder=2)
			ax.add_patch(fancy_arrow)
		else:
			# same resource
			y1 = y2
			distance = x2-x1
			conn = None
			if distance < RES_BAR_MARGIN*0.5:
				# move endpoints to create distance
				x1 = x1 - RES_BAR_MARGIN*0.25
				x2 = x2 + RES_BAR_MARGIN*0.25
				distance = x2-x1
			rad = (RES_BAR_MARGIN) / distance
			conn = matplotlib.patches.ConnectionStyle.Arc3(rad=rad)
			arr = matplotlib.patches.ArrowStyle("Fancy", head_length=4, head_width=8, tail_width=2)
			x1 -= 0.04
			y1 += 0.04
			x2 += 0.04
			y2 += 0.04
			fancy_arrow = matplotlib.patches.FancyArrowPatch((x1,y1), (x2, y2), 
				arrowstyle=arr, connectionstyle=conn, linewidth=0.5, 
				facecolor=arrow_color, edgecolor=(0.0,0.0,0.0,1.0), zorder=3)
			ax.scatter(x1,y1, s=10, facecolor=cfc, edgecolor=cec, zorder=2)
			ax.scatter(x2,y2, s=10, facecolor=cfc, edgecolor=cec, zorder=2)
			ax.add_patch(fancy_arrow)
			


# Plots the dependency arrow between two task rectangles
class DepArrow:
	def __init__(self, srcid, dstid, color):
		self.srcid = srcid
		self.dstid = dstid
		self.color = color
	def draw_arrow(self, ax, src, dst):
		x1 = src.x + (src.width/2.0)
		y1 = src.y
		x2 = dst.x + (dst.width/2.0)
		y2 = dst.y
		arrow_color = self.color
		rad = RES_BAR_HEIGHT/4.0
		conn = matplotlib.patches.ConnectionStyle.Arc3(rad=rad)
		arr = matplotlib.patches.ArrowStyle("Fancy", head_length=4, head_width=8, tail_width=2)
		fancy_arrow = matplotlib.patches.FancyArrowPatch((x1,y1), (x2, y2), 
			arrowstyle=arr, connectionstyle=conn, linewidth=0.5, color=arrow_color, zorder=3)
		ax.add_patch(fancy_arrow)
		
# Plot gantt chart for task events from an event $log
# Saves plot as SVG to $filepath
# $barcolor defines color mapping of task rectangles:
# * "random": (default) use color from default color cycle
# * "max_res_affinity": use color of resource with highest affinity
# * "res_affinity": use mixed color representing task resource affinity
def plot_experiment(log, filepath, schedules=True, barcolor="random"):
	fig, ax = plt.subplots(figsize=FIGSIZE)

	ax.grid(which="both")
	ax.set_axisbelow(True)

	res = log.resources.copy()
	ylabel_res = log.resources.copy()

	fig.set_tight_layout(True)

	tooltip = {}

	migarrows = []
	deparrows = []
	bars = {}

	if barcolor in ["max_res_affinity","res_affinity"]:
		msresults = msres.MSResults.load_results()

	for taskix, task in enumerate(list(log.tasks.values())):
		dst = "task_"+str(task.tid)+"_"+str(0)
		for d in task.dep:
			dtask = log.tasks[d]
			src = "task_"+str(dtask.tid)+"_"+str(len(dtask.parts)-1)
			appix = log.get_app_id(dtask.tid)
			app_col = dep_color_cycle[appix % len(dep_color_cycle)]
			depa = DepArrow(src, dst, app_col)
			deparrows.append(depa)

	for taskix, task in enumerate(list(log.tasks.values())):
		if barcolor == "random":
			color = color_cycle[taskix%len(color_cycle)]
		elif barcolor == "max_res_affinity":
			res_aff = max_res_affinity(res, msresults, task.name, task.size)
			if res_aff == None:
				color = (0.0,0.0,0.0,1.0)
			else:
				color = RES_COLOR[RES_LIST.index(res_aff)]
		elif barcolor == "res_affinity":
			color = res_affinity_color(res, msresults, task.name, task.size)
		for ix,part in enumerate(task.parts):
			if part.res not in res:
				continue
			yoff = res.index(part.res)
			if part.stop != None:
				width = part.stop - part.start
			else:
				width = 1.0
			gid = "task_"+str(task.tid)+"_"+str(ix) # svg element id
			height = RES_BAR_HEIGHT
			x = part.start-log.minstart
			y = -yoff*(RES_BAR_HEIGHT + RES_BAR_MARGIN)

			if part.progress != None:
				prog = float(part.progress) / task.checkpoints
			else:
				prog = 0.0

			task_time = res_time(res, msresults, task.name, task.size)
			tooltip[gid] = "#"+str(task.tid)+" "\
				+task.name+"("+str(task.size)+")"+" "\
				+"({0:}-{1:}/{2:}) ".format(str(part.startprogress), str(part.progress), str(task.checkpoints))\
				+"{0:.6f}s".format(((part.stop-part.start) if part.stop != None and part.start != None else -1.0))
				#+str("{0[0]:.2f},{0[1]:.2f},{0[2]:.2f}".format(task_time))+" "\

			bar = GanttBar(gid, x,y,width,height,ix, len(task.parts),str(task.tid), color, prog)
			bars[gid] = bar

			# draw migration arrow
			if ix > 0:
				miga = MigArrow("task_"+str(task.tid)+"_"+str(ix-1), gid)
				migarrows.append(miga)

	# draw bars
	for gid in bars:
		bar = bars[gid]
		bar.draw_bar(ax)

	xlim = ax.get_xlim()

	for gid in bars:
		bar = bars[gid]
		bar.draw_depcirc(ax)

	# draw task ids
	for gid in bars:
		bar = bars[gid]
		bar.draw_taskid(fig, ax)
		bar.draw_progress(fig, ax)

	for miga in migarrows:
		src = bars[miga.srcid]
		dst = bars[miga.dstid]
		miga.draw_arrow(ax, src, dst)

	for depa in deparrows:
		if depa.srcid not in bars or depa.dstid not in bars:
			continue
		src = bars[depa.srcid]
		dst = bars[depa.dstid]
		depa.draw_arrow(ax, src, dst)

	if schedules == True:
		ylabel_res.append("Scheduler")
		for ix, schedule in enumerate(log.schedules):
			yoff = len(ylabel_res)-1
			stop = float(schedule.schedule["time"])
			width = schedule.schedule["schedule"]["compute_duration"]/1000000000.0
			if log.maxstop != None:
				width = max(width, (log.maxstop-log.minstart)*0.01)
			else:
				width = max(width, 0.01)
			start = stop - width
			gid = "schedule_"+str(ix)
			tooltip[gid] = "#"+str(ix)+" Computed in: "+str(schedule.schedule["schedule"]["compute_duration"]/1000000000.0)
			color = (1.0, 1.0, 1.0, 1.0)
			bar = ax.barh(-yoff*(RES_BAR_HEIGHT + RES_BAR_MARGIN), width, height=RES_BAR_HEIGHT, left=start-log.minstart, color=color, edgecolor=(0.0,0.0,0.0,1.0), gid=gid)

	# plot measurement
	if len(log.measure) > 0:
		ylabel_res.append("Power")
		yoff = len(ylabel_res)-1
		measure_min_y = -yoff*(RES_BAR_HEIGHT + RES_BAR_MARGIN) - (RES_BAR_HEIGHT*0.5)
		measure_max_y = -yoff*(RES_BAR_HEIGHT + RES_BAR_MARGIN) + (RES_BAR_HEIGHT*0.5)
		plot_measurement(log, None, fig=fig, ax=ax, 
			min_y = measure_min_y,
			max_y = measure_max_y)
				
	ax.set_ylim([-(len(ylabel_res)-1+0.5)*(RES_BAR_HEIGHT + RES_BAR_MARGIN), (RES_BAR_HEIGHT + RES_BAR_MARGIN)*0.5])

	ax.set_yticks([-(RES_BAR_HEIGHT + RES_BAR_MARGIN)*yoff for yoff in range(0,len(ylabel_res))])
	ax.set_yticklabels(ylabel_res)


	f = io.BytesIO()
	fig.savefig(f, format="svg") # save to buffer (file-like-object)

	f = svgAddTitles(f.getvalue(), tooltip)

	with open(filepath, "wb") as outfile:
		outfile.write(f.getvalue())


# Plot gantt chart for $schedule from $log
# Saves plot as SVG to $filepath
# $barcolor defines color mapping of task rectangles:
# * "random": (default) use color from default color cycle
# * "max_res_affinity": use color of resource with highest affinity
# * "res_affinity": use mixed color representing task resource affinity
def plot_schedule(log, schedule, filepath, barcolor="random"):
	fig, ax = plt.subplots(figsize=FIGSIZE)

	ax.grid(which="both")
	ax.set_axisbelow(True)

	res = log.resources
	ax.set_ylim([-(len(res)-1+0.5)*(RES_BAR_HEIGHT + RES_BAR_MARGIN), (RES_BAR_HEIGHT + RES_BAR_MARGIN)*0.5])

	ax.set_yticks([-(RES_BAR_HEIGHT + RES_BAR_MARGIN)*yoff for yoff in range(0,len(res))])
	ax.set_yticklabels(res)


	tooltip = {}

	task_color = {}


	if barcolor in ["max_res_affinity","res_affinity"]:
		msresults = msres.MSResults.load_results()



	migarrows = []
	deparrows = []
	bars = {}

	parts = {}

	# Create dictionary with task to part mapping
	# A part is one task execution on one resource represented by one rectangle
	for listix, tasklist in enumerate(schedule.schedule["schedule"]["tasks"]):
		for taskix, task in enumerate(tasklist):
			if task["id"] not in parts or task["part"] > parts[task["id"]]:
				parts[task["id"]] = task["part"]

	# Create dependency arrows between parts
	for listix, tasklist in enumerate(schedule.schedule["schedule"]["tasks"]):
		for taskix, task in enumerate(tasklist):
			realtask = log.tasks[task["id"]]
			dst = "task_"+str(realtask.tid)+"_"+str(0)
			for dep in realtask.dep:
				if dep not in parts:
					continue
				maxpart = parts[dep]
				src = "task_"+str(dep)+"_"+str(maxpart)
				appix = log.get_app_id(realtask.tid)
				app_col = dep_color_cycle[appix % len(dep_color_cycle)]
				depa = DepArrow(src, dst, app_col)
				deparrows.append(depa)

	# Create GanttBar objects for parts and dependency arrows
	for listix, tasklist in enumerate(schedule.schedule["schedule"]["tasks"]):
		for taskix, task in enumerate(tasklist):
			realtask = log.tasks[task["id"]]
			if barcolor=="random":
				color = color_cycle[taskix%len(color_cycle)]
			elif barcolor=="max_res_affinity":
				res_aff = max_res_affinity(res, msresults, 
					realtask.name, realtask.size)
				if res_aff == None:
					color = (0.0,0.0,0.0,1.0)
				else:
					color = RES_COLOR[RES_LIST.index(res_aff)]
			elif barcolor=="res_affinity":
				color = res_affinity_color(res, msresults, realtask.name, realtask.size)
			if task["id"] in task_color:
				color = task_color[task["id"]]
			yoff = listix
			start = int(task["time_ready"]) / 1000000000.0
			stop = int(task["time_finish"]) / 1000000000.0
			width = stop - start
			gid = "task_"+str(task["id"])+"_"+str(task["part"]) # svg element id
			name = log.tasks[task["id"]].name
			size = log.tasks[task["id"]].size
			tooltip[gid] = "#"+str(task["id"]) + " " + name + "("+str(size)+")"+" "\
				+"({0:}-{1:}/{2:}) ".format(task["start_progress"],task["stop_progress"],(realtask.checkpoints if realtask.checkpoints != None else 0.0))\
				+"{0:.6f}s".format(task["duration_total"]/1000000000.0)
				#+"("+str(size)+")"+" "+str(color[0:3])
			x = start
			y = -yoff*(RES_BAR_HEIGHT + RES_BAR_MARGIN)
			height = RES_BAR_HEIGHT

			prog = float(task["stop_progress"]) / realtask.checkpoints

			bar = GanttBar(gid, x, y, width, height, task["part"], parts[task["id"]]+1, str(task["id"]), color, prog)

			bars[gid] = bar

			# create dependency arrows
			if task["part"] > 0:
				ppart = task["part"]-1
				ptask = None
				pres = None
				# search previous part
				for resix, restasklist in enumerate(schedule.schedule["schedule"]["tasks"]):
					for prevtask in restasklist:
						if prevtask["id"] == task["id"] and prevtask["part"] == ppart:
							ptask = prevtask
							pres = resix
							break
					if ptask != None:
						break
				if ptask != None:
					dst = gid
					src = "task_"+str(ptask["id"])+"_"+str(ptask["part"])
					miga = MigArrow(src, dst)
					migarrows.append(miga)

	# first draw all bars to set correct xlim/ylim
	for gid in bars:
		bar = bars[gid]
		bar.draw_bar(ax)

	xlim = ax.get_xlim()

	# draw bar labels
	for gid in bars:
		bar = bars[gid]
		bar.draw_taskid(fig, ax)
		bar.draw_progress(fig, ax)

	# draw center circles
	for gid in bars:
		bar = bars[gid]
		bar.draw_depcirc(ax)

	# draw migration arrows
	for miga in migarrows:
		src = bars[miga.srcid]
		dst = bars[miga.dstid]
		miga.draw_arrow(ax, src, dst)

	# draw dependency arrows
	for depa in deparrows:
		src = bars[depa.srcid]
		dst = bars[depa.dstid]
		depa.draw_arrow(ax, src, dst)

	#ax.set_xlim(xlim)

	fig.set_tight_layout(True)
	f = io.BytesIO()
	fig.savefig(f, format="svg")

	f = svgAddTitles(f.getvalue(), tooltip)

	with open(filepath, "wb") as outfile:
		outfile.write(f.getvalue())


# Plot task lifeline chart for tasks in $log
def plot_task_lifelines(log, filepath):
	fig, ax = plt.subplots(figsize=FIGSIZE)

	ax.grid(which="both")
	ax.set_axisbelow(True)

	res = log.resources

	tooltip = {}

	colors = [(0.0,0.0,1.0,1.0),(1.0,0.0,0.0,1.0),(0.0,1.0,0.0,1.0)]

	msresults = msres.MSResults.load_results()

	# create life lines
	for taskix, tid in enumerate(sorted(list(log.tasks.keys()))):
		task = log.tasks[tid]
		xoff = taskix
		arrival = float(task.arrival)
		if len(task.parts) == 0:
			continue
		end = task.parts[-1].stop
		if end == None:
			end = arrival + 1.0
		enddiff = end - arrival

		# add life line
		x = xoff*(TASK_BAR_WIDTH + TASK_BAR_MARGIN)
		line = matplotlib.lines.Line2D([x,x],[arrival-log.minstart,end-log.minstart], color=(0.0,0.0,0.0,1.0), linewidth=0.75)
		ax.add_line(line)
		x1 = x-(TASK_BAR_WIDTH*0.5)
		x2 = x+(TASK_BAR_WIDTH*0.5)
		line_top = matplotlib.lines.Line2D([x1,x2],[arrival-log.minstart,arrival-log.minstart], color=(0.0,0.0,0.0,1.0), linewidth=0.5)
		line_bottom = matplotlib.lines.Line2D([x1,x2],[end-log.minstart,end-log.minstart], color=(0.0,0.0,0.0,1.0), linewidth=0.5)
		ax.add_line(line_top)
		ax.add_line(line_bottom)


		# add parts
		for ix,part in enumerate(task.parts):
			if part.res not in res:
				continue
			resoff = res.index(part.res)
			msresult = msresults.result(task.name, task.size, part.res)
			init = None
			fini = None
			if msresult != None:
				#init = msresult.avg_init() / 1000000000.0
				init = msresult.avg_init()
				#fini = msresult.avg_fini() / 1000000000.0
				fini = msresult.avg_fini()
			else:
				print("No results for ", task.name, task.size, part.res)
				init = 1.0
				fini = 1.0
			color = colors[resoff]
			if part.stop != None:
				height = part.stop - part.start
			else:
				height = 1.0
			gid = "task_"+str(task.tid)+"_"+str(ix) # svg element id
			tooltip[gid] = "#"+str(task.tid) + " " + task.name+"("+str(task.size)+")"
			bar = ax.bar(xoff*(TASK_BAR_WIDTH + TASK_BAR_MARGIN), height, width=TASK_BAR_WIDTH, bottom=part.start-log.minstart, color=color, gid=gid)
			# start line
			line_start = matplotlib.lines.Line2D([x1-TASK_BAR_MARGIN*0.2,x2+TASK_BAR_MARGIN*0.2],[part.start-log.minstart, part.start-log.minstart], color=(0.0,0.0,0.0,1.0), linewidth=0.5)
			# stop line
			line_stop = matplotlib.lines.Line2D([x1-TASK_BAR_MARGIN*0.2,x2+TASK_BAR_MARGIN*0.2],
				[part.start-log.minstart + height, part.start-log.minstart + height], color=(0.0,0.0,0.0,1.0), linewidth=0.5)
			ax.add_line(line_start)
			ax.add_line(line_stop)
			# init line
			line_init = matplotlib.lines.Line2D([x1,x2],[part.start-log.minstart + init, part.start-log.minstart + init], color=(0.0,0.7,0.0,1.0), linewidth=0.5)
			line_init1 = matplotlib.lines.Line2D([x1,x1],
				[part.start-log.minstart + init, 
				part.start-log.minstart],
				color=(0.0,0.7,0.0,1.0), linewidth=0.5)
			line_init2 = matplotlib.lines.Line2D([x2,x2],
				[part.start-log.minstart + init, 
				part.start-log.minstart], 
				color=(0.0,0.7,0.0,1.0), linewidth=0.5)
			# fini line
			line_fini = matplotlib.lines.Line2D([x1,x2],
				[part.start-log.minstart + height - fini, part.start-log.minstart + height - fini], color=(0.7,0.0,0.7,1.0), linewidth=0.5)
			line_fini1 = matplotlib.lines.Line2D([x1,x1],
				[part.start-log.minstart + height - fini, part.start-log.minstart + height], color=(0.7,0.0,0.7,1.0), linewidth=0.5)
			line_fini2 = matplotlib.lines.Line2D([x2,x2],
				[part.start-log.minstart + height - fini, part.start-log.minstart + height], color=(0.7,0.0,0.7,1.0), linewidth=0.5)
			ax.add_line(line_init)
			ax.add_line(line_init1)
			ax.add_line(line_init2)
			ax.add_line(line_fini)
			ax.add_line(line_fini1)
			ax.add_line(line_fini2)

	tasknum = len(list(log.tasks.keys()))

	ax.set_xlim([(-1)*(TASK_BAR_WIDTH + TASK_BAR_MARGIN), (TASK_BAR_WIDTH + TASK_BAR_MARGIN)*(tasknum)])

	ax.set_xticks([(TASK_BAR_WIDTH + TASK_BAR_MARGIN)*xoff for xoff in range(0,tasknum,5)])
	ax.set_xticklabels([str(taskid) for taskid in range(0,tasknum,5)])

	fig.set_tight_layout(True)
	#f = io.BytesIO()
	fig.savefig(filepath, format="svg") # save to buffer (file-like-object)


def plot_measurement(log, filepath, fig=None, ax=None, min_y=None, max_y=None):

	newfig = False
	if fig == None and ax == None:
		newfig = True

	if newfig == True:
		fig, ax = plt.subplots(figsize=FIGSIZE)
		ax.grid(which="both")
		ax.set_axisbelow(True)

	time = []
	cpu0 = []
	cpu1 = []
	gpu = []
	fpga = []
	system = []
	for m in log.measure:
		time += [float(m["time"])-log.minstart]
		cpu0 += [m["cpu_power"][0]]
		cpu1 += [m["cpu_power"][1]]
		gpu += [m["gpu_power"]]
		fpga += [m["fpga_power"]]
		system += [m["sys_power"]]

	# scale to min_y, max_y
	if newfig == False:
		diff_y = max_y - min_y
		max_power = max(cpu0 + cpu1 + gpu + fpga + system)
		cpu0 = [ (c / max_power)*diff_y + min_y
			for c in cpu0 ]
		cpu1 = [ (c / max_power)*diff_y + min_y
			for c in cpu1 ]
		gpu = [ (g / max_power)*diff_y + min_y
			for g in gpu ]
		fpga = [ (f / max_power)*diff_y + min_y
			for f in fpga ]
		system = [ (s / max_power)*diff_y + min_y
			for s in system ]
		

	ax.plot(time, cpu0, color="blue", label="CPU0")
	ax.plot(time, cpu1, color="blue", label="CPU1")
	ax.plot(time, gpu, color="red", label="GPU")
	ax.plot(time, fpga, color="green", label="FPGA")
	ax.plot(time, system, color="orange", label="SYSTEM")
	if newfig == False:
		ax.plot([time[0], time[-1]], [min_y, min_y], color="black", linestyle="dashed")


	if newfig == True:
		ax.legend()
		fig.set_tight_layout(True)
		ax.set_xlabel("Time (s)")
		ax.set_ylabel("Power (W)")
		fig.savefig(filepath, format="svg") # save to buffer (file-like-object)

# Takes SVG as bytes and adds Tooltips to task elements
# $tooltipMap maps element id to tooltip text
# Returns BytesIO object containing the changed SVG
def svgAddTitles(svg, tooltipMap):
	# https://matplotlib.org/2.1.2/gallery/user_interfaces/svg_tooltip_sgskip.html
	# modify svg
	tree, xmlid = ET.XMLID(svg)

	for element in tree.iter():
		#print(element.tag)
		gid = element.get("id", default=None)
		if gid == None:
			continue
		if gid not in tooltipMap:
			continue
		title = ET.SubElement(element, "ns0:title")
		title.text = tooltipMap[gid]
		element.attrib["class"] = "task"
	defs = tree.find("{http://www.w3.org/2000/svg}defs")
	css_style = ET.SubElement(defs, "ns0:style", attrib = {"type":"text/css"})
	css_style.text = """
		.task:hover {
			stroke-dasharray: 2,1,1,2 !important;
			stroke: #ff0000 !important;
			stroke-width: 1 !important;
		}
	"""

	f = io.BytesIO()
	ET.ElementTree(tree).write(f)
	return f

# Creates a text object and returns the calculated bounding box
def get_text_bb(fig, ax, text, fontsize=None, transform=True):
	r = fig.canvas.get_renderer()
	t = plt.text(0.5,0.5,text, fontsize=fontsize)
	bb = t.get_window_extent(renderer=r)
	t.remove()
	if transform==True:
		inv = ax.transData.inverted()
		return bb.transformed(inv)
	return bb

# Finds resource with highest affinity for task with $name and $size
# Returns the resource name
def max_res_affinity(res, msresults, name, size):
	res_aff = None
	res_min = None
	for r in res:
		result = msresults.result(name, size, r)
		if result == None:
			continue
		if res_min == None or result.avg_time() < res_min:
			res_min = result.avg_time()
			res_aff = r
	return res_aff

# Computes RGB color vector for resource affinity of task with $name and $size
def res_affinity_color(res, msresults, name, size):
	res_aff = []
	res_min = None
	for r in RES_LIST:
		if r not in res:
			res_aff.append(float("inf"))
			continue
		result = msresults.result(name, size, r)
		if result == None:
			res_aff.append(float("inf"))
			continue
		time = result.avg_time()
		res_aff.append(time)
		if res_min == None or time < res_min:
			res_min = time
	if res_min == None:
		return (0.0,0.0,0.0,1.0)
	r = res_min / res_aff[1] # gpu
	g = res_min / res_aff[2] # fpga
	b = res_min / res_aff[0] # cpu
	return (r,g,b,1.0)

# Returns resource time for task with $name and $size
def res_time(res, msresults, name, size):
	res_time = []
	for r in RES_LIST:
		if r not in res:
			res_time.append(float("inf"))
			continue
		result = msresults.result(name, size, r)
		if result == None:
			res_time.append(float("inf"))
			continue
		time = result.avg_time()
		res_time.append(time)
	return res_time

# Plot triangle for tasks in event $log and save as SVG to $filepath
def plot_taskset_triangle(log, filepath):

	taskset = []
	for tid in log.tasks:
		t = log.tasks[tid]
		task = (t.name, t.size)
		taskset.append(task)

	plot_tod.plot_taskset(taskset, filepath)

if __name__ == "__main__":

	if len(sys.argv) < 2:
		print(sys.argv[0], "log-file.log")
		print("Creates plots for event log files")
		sys.exit(1)

	logfile = sys.argv[1]

	log = log_stats.EventLog.loadEvents(logfile
)
	plot_experiment(log, logfile+"_all.svg", barcolor="res_affinity")
	for ix,schedule in enumerate(log.schedules):
		plot_schedule(log, schedule, logfile+"_schedule_"+str(ix)+".svg", barcolor="res_affinity")

	plot_task_lifelines(log, logfile+"_tasklifelines.svg")

	plot_taskset_triangle(log, logfile+"_taskset_triangle.svg")
