#!/usr/bin/env python3
# Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
# SPDX-License-Identifier: BSD-2-Clause


import os
import scipy.stats


cdfpath = os.path.join(os.path.dirname(os.path.realpath(__file__)),"cdf")

class CDF:
	def __init__(self):
		self.lines = []
	def sample(self, y):
		matching = []
		#print(y)
		for l in self.lines:
			#print(l)
			if l[1] <= y and y <= l[3]:
				matching.append(l)
		xfirst = None
		xlast = None
		for m in matching:
			# if y coordinates of start and end of the line are the same
			if m[1] == m[3]:
				if xfirst == None or m[0]< xfirst:
					xfirst = m[0]
				if xlast == None or m[2]< xlast:
					xlast = m[0]
			else:
				# relative position in line segment for queried y value
				d = (y-m[1])/(m[3]-m[1])
				# use relative position for y to get value on x-axis
				x = d*(m[2]-m[0]) + m[0]
				if xfirst == None or x < xfirst:
					xfirst = x
				if xlast == None or x < xlast:
					xlast = x
		return (xlast+xfirst) / 2.0
	def loadCDF(cdfname):
		cdffilepath = os.path.join(cdfpath, cdfname)
		lines = []
		with open(cdffilepath, "r") as f:
			for l in f:
				parts = l.split(" ")
				lines.append((float(parts[0]),float(parts[1]),float(parts[2]),float(parts[3])))
		c = CDF()
		c.lines = lines
		return c

class CDFBeta:
	def __init__(self, mean, betasum):
		self.betasum = betasum
		self.mean = mean
		self.beta = (betasum-0.0000001) - mean * (betasum-0.0000001) + 0.0000001
		self.alpha = betasum - self.beta
		self.beta_dist = scipy.stats.beta(self.alpha, self.beta)
	def loadCDF(cdfpath):
		pass
	def sample(self, y):
		return self.beta_dist.ppf(y)

