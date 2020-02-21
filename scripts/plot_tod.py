#!/usr/bin/env python3
# Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
# SPDX-License-Identifier: BSD-2-Clause


# Plot resource affinity triangle
# Prepare triangle plot and add points
# Executed as standalone script it plots the triangle for a task.

import random

import sys
import time
import math
import functools
import time as pytime
import matplotlib
import statistics
 
import matplotlib.pyplot as plt
import matplotlib.image
import matplotlib.lines
import matplotlib.text

import msresults as msres
import plot_tod_img

RES_LIST = ["IntelXeon", "NvidiaTesla", "MaxelerVectis"]
FIGSIZE = [10, 10]

default_angle_func = plot_tod_img.ratio2angle_factor
default_ratio_func = plot_tod_img.angle2ratio_factor
default_scale_func = plot_tod_img.ratio_identity
default_rescale_func = plot_tod_img.ratio_identity


def task_result_times(results):
	result_times = []
	for res in results:
		times = []
		for ix, result in enumerate(res[0]):
			if result == None:
				times.append(0.0)
			else:
				time = result.avg_time()
				times.append(time)
		result_times.append(times)
	return result_times

def task_result_energys(results):
	result_energys = []
	for res in results:
		energys = []
		for ix, result in enumerate(res[0]):
			if result == None:
				energys.append(0.0)
			else:
				energy = result.avg_energy()
				energys.append(energy)
		result_energys.append(energys)
	return result_energys

def abs2tricoord(absval, scale_func, angle_func):
	ratio = plot_tod_img.abs2ratio(absval)
	ratio = plot_tod_img.ratio_scale(scale_func, ratio)
	ang = angle_func(ratio)
	x,y = plot_tod_img.angle2tricoord(ang)
	return (x,y)


def taskset_ratios_sum(ratios):
	res = (0.0,0.0,0.0)
	for r in ratios:
		res = (res[0]+r[0], res[1]+r[1], res[2]+r[2])
	return res

def taskset_ratios_avg(ratios):
	res = (0.0,0.0,0.0)
	for r in ratios:
		res = (res[0]+r[0], res[1]+r[1], res[2]+r[2])
	return (
		res[0]/len(ratios),
		res[1]/len(ratios),
		res[2]/len(ratios)
		)

def taskset_ratios_median(ratios):
	res = [[],[],[]]
	for r in ratios:
		res[0].append(r[0])
		res[1].append(r[1])
		res[2].append(r[2])
	return (
		statistics.median(res[0]),
		statistics.median(res[1]),
		statistics.median(res[2])
		)

# Plot ticks and grid lines for affinity triangle
def create_ticks(ax, start, stop, tick_positions, tick_labels=None, tick_length=1.0, text_dist=0.1, text_off=None, va='baseline', ha='left', grid_start=None, grid_list=None, zorder=1):
	# ax direction
	diff = (stop[0]-start[0], stop[1]-start[1])
	# tick direction
	tdiff = (diff[1],-diff[0])
	tdiff = ( tdiff[0] / math.sqrt(tdiff[0]**2 + tdiff[1]**2) , tdiff[1] / math.sqrt(tdiff[0]**2 + tdiff[1]**2))
	# create ticks
	#lines = []
	#texts = []
	for ix, p in enumerate(tick_positions):
		x1 = start[0] + diff[0]*p
		y1 = start[1] + diff[1]*p
		x2 = x1 + tdiff[0]*tick_length
		y2 = y1 + tdiff[1]*tick_length
		l = matplotlib.lines.Line2D(
			(x1,x2),
			(y1,y2),
			color="black",
			zorder=zorder)
		if tick_labels == None:
			label = "{0:3.1f}".format(p)
		else:
			label = tick_labels[ix]
		if text_off == None:
			tx = 0
			ty = 0
		else:
			tx, ty = text_off
		if label != None:
			t = matplotlib.text.Text(
				x2 + tdiff[0]*text_dist + tx, 
				y2 + tdiff[1]*text_dist + ty, label,
				va=va,
				ha=ha,
				zorder=zorder)
			ax.add_artist(t)
		ax.add_line(l)
		if grid_start != None:
			if grid_list == None or ix in grid_list:
				gx1,gy1 = grid_start
				l = matplotlib.lines.Line2D(
					(gx1,x1),
					(gy1,y1),
					linestyle=":",
					linewidth=1,
					#color=(1.0,1.0,1.0,0.5))
					color=(0.0,0.0,0.0,0.3),
					zorder=zorder)
				ax.add_line(l)
		
# Add affinity triangle background, axis, ticks and labels to plot
def ia_triplot(fig, ax, 
	plot_scale=False, plot_grid=False, img_alpha=0.5,
	angle_func = default_angle_func,
	ratio_func = default_ratio_func,
	scale_func = default_scale_func,
	rescale_func = default_rescale_func,
	zorder=1):

	# experimental options
	#angle_func = plot_tod_img.ratio2angle_factor
	#ratio_func = plot_tod_img.angle2ratio_factor
	
	#angle_func = plot_tod_img.ratio2angle_sum
	#ratio_func = plot_tod_img.angle2ratio_sum

	#scale_func = plot_tod_img.ratio_identity
	#rescale_func = plot_tod_img.ratio_identity

	#scale_func = plot_tod_img.bind_ratio_log(2)
	#rescale_func = plot_tod_img.bind_ratio_exp(2)

	#scale_func = plot_tod_img.bind_ratio_exp(0.7)
	#rescale_func = plot_tod_img.bind_ratio_log(0.7)

	#scale_func = plot_tod_img.bind_ratio_power(2)
	#rescale_func = plot_tod_img.bind_ratio_power(1/2.0)

	#scale_func = plot_tod_img.bind_ratio_power(1/2.0)
	#rescale_func = plot_tod_img.bind_ratio_power(2.0)


	# Calculate affinity triangle color background
	img=plot_tod_img.create_tod(500, alpha=img_alpha, ratio_func=ratio_func, scale_func=rescale_func)

	# Use this code to load recalculated triangle with high resolution
	# load triangle from file and change alpha
	#img=matplotlib.image.imread("example.png")
	#if img_alpha < 1.0:
	#	for x,col in enumerate(img):
	#		for y,p in enumerate(col):
	#			r,g,b,a = p
	#			col[y] = (r,g,b,a*0.5)

	# Draw color triangle
	ax.imshow(img, extent=(0.0, 1.0, 0.0, 1.0))

	# Triangle corner points
	a = (0.0,0.0)
	b = (1.0,0.0)
	c = (0.5, math.sqrt(3.0)/2.0)

	# create triangle axis
	l1 = matplotlib.lines.Line2D((a[0],b[0]),(a[1],b[1]), color="black", zorder=zorder)
	l2 = matplotlib.lines.Line2D((b[0],c[0]),(b[1],c[1]), color="black", zorder=zorder)
	l3 = matplotlib.lines.Line2D((c[0],a[0]),(c[1],a[1]), color="black", zorder=zorder)
	ax.add_line(l1)
	ax.add_line(l2)
	ax.add_line(l3)

	# Plot ticks and grid lines
	if plot_scale == True:
		ticks = [
			#(20,1,0.0),
			(10,1,0.0),
			(5,1,0.0),
			#(4,1,0.0),
			#(3,1,0.0),
			(2,1,0.0),
			(1,1,0.0),
			(1,2,0.0),
			#(1,3,0.0),
			#(1,4,0.0),
			(1,5,0.0),
			(1,10,0.0),
			#(1,20,0.0)
			]
		fr = []
		ft = []
		for t in ticks:
			rat = plot_tod_img.abs2ratio(t)
			rat = plot_tod_img.ratio_scale(scale_func, rat)
			ang = angle_func(rat)
			p = plot_tod_img.angle2tricoord(ang)
			fr.append(p[0])
			ft.append(str(t[0]+t[1]-1))

		create_ticks(ax, a, b, 
			fr, tick_length=0.02, text_dist=0.01,
			text_off=(0,-0.04),
			ha="center",
			tick_labels=ft,
			grid_start=c if plot_grid==True else None,
			grid_list=[math.floor(len(fr)/2)],
			zorder=zorder
			)
		
		create_ticks(ax, b, c, 
			fr, tick_length=0.02, text_dist=0.01,
			text_off=(0,0),
			ha="left", va="center",
			tick_labels=ft,
			grid_start=a if plot_grid==True else None,
			grid_list=[math.floor(len(fr)/2)],
			zorder=zorder
			)
		create_ticks(ax, c, a, 
			fr, tick_length=0.02, text_dist=0.01,
			text_off=(0,0),
			ha="right", va="center",
			tick_labels=ft,
			grid_start=b if plot_grid==True else None,
			grid_list=[math.floor(len(fr)/2)],
			zorder=zorder
			)

	# Add corner resource labels
	ax.text(-0.1,-0.05,"CPU", ha="left", va="bottom", zorder=zorder)
	ax.text(1.1,-0.05,"GPU", ha="right", va="bottom", zorder=zorder)
	ax.text(0.5,c[1]+0.02,"FPGA", ha="center", va="bottom", zorder=zorder)

	#ax.set_xlim([-0.1, 1.1])
	#ax.set_ylim([-0.2, 1.1])
	ax.axis('off')


def plot_mstask(task):

	# create figure
	fig, ax = plt.subplots(figsize=FIGSIZE)

	# plot triangle
	ia_triplot(fig, ax,
		plot_scale=True, 
		plot_grid=True, 
		img_alpha=1.0)

	# get available task sizes and task results
	msresults = msres.MSResults.load_results()
	sizes = msresults.task_sizes(task)
	results = []
	for s in sizes:
		rresult = []
		for r in RES_LIST:
			result = msresults.result(task, s, r)
			rresult.append(result)
		results.append((rresult, task, s))

	angle_func = default_angle_func
	ratio_func = default_ratio_func
	scale_func = default_scale_func
	rescale_func = default_rescale_func
	# add points
	# time
	time_x = []
	time_y = []
	time_text = []
	result_times = task_result_times(results)
	for time in result_times:
		x,y = abs2tricoord(time, scale_func, angle_func)
		time_x.append(x)
		time_y.append(y)
	for res in results:
		time_text.append(str(res[2]))
	ax.plot(time_x,time_y, color="blue")
	ax.scatter(time_x,time_y, color="blue")
	for ix,t in enumerate(time_text):
		ax.text(time_x[ix], time_y[ix], t)

	# energy
	energy_x = []
	energy_y = []
	energy_text = []

	result_energys = task_result_energys(results)
	for energy in result_energys:
		x,y = abs2tricoord(energy, scale_func, angle_func)
		energy_x.append(x)
		energy_y.append(y)
	for res in results:
		energy_text.append(str(res[2]))
	ax.plot(energy_x, energy_y, color="green")
	ax.scatter(energy_x, energy_y, color="green")
	for ix,t in enumerate(energy_text):
		ax.text(energy_x[ix], energy_y[ix], t)

	fig.canvas.set_window_title(task)
	ax.set_title(task)
	fig.set_tight_layout(True)

	plt.show()


# Plot triangle and add points for the given tasks
# $tasks is a list of (task, size) tuples
def plot_taskset(tasks, filepath=None, show=False):
	
	# create figure
	#fig, ax = plt.subplots(figsize=FIGSIZE)
	fig = plt.figure(figsize=[5,5])
	#ax = fig.add_axes([0.0,0.0,1.0,1.0])
	ax = fig.subplots()

	# plot triangle
	ia_triplot(fig, ax,
		plot_scale=True, 
		plot_grid=True, 
		img_alpha=1.0)
#		img_alpha=1.0)

	# get available task sizes and task results
	msresults = msres.MSResults.load_results()
	results = []
	for t in tasks:
		task, size = t
		rresult = []
		for r in RES_LIST:
			result = msresults.result(task, size, r)
			rresult.append(result)
		results.append((rresult, task, size))

	angle_func = default_angle_func
	ratio_func = default_ratio_func
	scale_func = default_scale_func
	rescale_func = default_rescale_func

	time_x = []
	time_y = []
	result_times = task_result_times(results)
	for time in result_times:
		x,y = abs2tricoord(time, scale_func, angle_func)
		x += (random.random()-0.5)*0.005
		y += (random.random()-0.5)*0.005
		time_x.append(x)
		time_y.append(y)
	#ax.plot(time_x,time_y, color="blue")
	#ax.scatter(time_x,time_y, color="blue", edgecolor="black", zorder=2)
	ax.scatter(time_x,time_y, color=(0.0,0.0,1.0,0.0), edgecolor=(0.0,0.0,0.0,0.3), zorder=2)

	times_sum = taskset_ratios_sum(result_times)
	print("Sum:",times_sum)
	time_ratio = plot_tod_img.abs2ratio(times_sum)
	time_ratio = plot_tod_img.ratio_scale(scale_func, time_ratio)
	print("Sum ratio:",time_ratio)
	ang = angle_func(time_ratio)
	x,y = plot_tod_img.angle2tricoord(ang)
	ax.scatter(x,y, color="blue", edgecolor="black", marker="X", s=100, zorder=3)



	#fig.canvas.set_window_title(task)
	#ax.set_title(task)
	fig.set_tight_layout(True)

	if filepath != None:
		fig.savefig(filepath, format="svg") # save to buffer (file-like-object)

	if show == True:
		fig.show()
		show = plt._backend_mod.Show()
		show.mainloop()

if __name__ == "__main__":

	if len(sys.argv) < 2:
		print(sys.argv[0], "task-name")
		print("Plot resource affinity triangle for the given task")
		sys.exit(1)

	task = sys.argv[1]
	plot_mstask(task)
	
