#!/usr/bin/env python3
# Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
# SPDX-License-Identifier: BSD-2-Clause


# Fills a triangle shape with the resource affinity color map.
# First resource is in bottom left corner.
# Second resource is in bottom right corner.
# Third resource is in top corner.
#
# Use create_tod() to get an image array for plotting.
# Use save_as_png() to save the array to a PNG file.
# Execute this python file to create an image file.

import sys
import math
import functools

import numpy
import matplotlib.pyplot

# Apply scale_func to ratio tuple
def ratio_scale(scale_func, rat):
	return (
		scale_func(rat[0]),
		scale_func(rat[1]),
		scale_func(rat[2]),
		)

# Convert tuple with absolute values to ratio tuple.
# Values are normalized relative to lowest value.
# 0.0 values are ignored.
def abs2ratio(absval):
	if absval[0] == 0.0 and absval[1] == 0.0:
		return (0.0, 0.0, 1.0)
	if absval[1] == 0.0 and absval[2] == 0.0:
		return (1.0, 0.0, 0.0)
	if absval[0] == 0.0 and absval[2] == 0.0:
		return (0.0, 1.0, 0.0)
	if absval[0] == 0.0:
		if absval[1] < absval[2]:
			return (0.0, 1.0, absval[1]/absval[2])
		else:
			return (0.0, absval[2]/absval[1], 1.0)
	if absval[1] == 0.0:
		if absval[0] < absval[2]:
			return (1.0, 0.0, absval[0]/absval[2])
		else:
			return (absval[2]/absval[0], 0.0, 1.0)
	if absval[2] == 0.0:
		if absval[0] < absval[1]:
			return (1.0, absval[0]/absval[1], 0.0)
		else:
			return (absval[1]/absval[0], 1.0, 0.0)
	if absval[0] < absval[1] and absval[0] < absval[2]:
		# a is min
		return (1.0, absval[0]/absval[1], absval[0]/absval[2])
	elif absval[1] < absval[0] and absval[1] < absval[2]:
		# b is min
		return (absval[1]/absval[0], 1.0, absval[1]/absval[2])
	else:
		# c is min
		return (absval[2]/absval[0], absval[2]/absval[1], 1.0)

# Convert ratio tuple to alpha, beta, gamme angle tuple for the triangle
# The angle is calculated using the percentage of the smaller affinity of the sum of both affinities.
def ratio2angle_factor(ratio):
	if ratio[0] == 0.0:
		r0 = 0.0
	else:
		r0 = 1.0/ratio[0] if ratio[0] < 1.0 else ratio[0]
	if ratio[1] == 0.0:
		r1 = 0.0
	else:
		r1 = 1.0/ratio[1] if ratio[1] < 1.0 else ratio[1]
	if ratio[2] == 0.0:
		r2 = 0.0
	else:
		r2 = 1.0/ratio[2] if ratio[2] < 1.0 else ratio[2]
	if r0 == 0.0:
		alpha = 60 * (r1 / (r1 + r2))
		beta = 60.0
		gamma = 60.0
	elif r1 == 0.0:
		alpha = 60.0
		beta = 60 * (r0 / (r0 + r2))
		gamma = 0.0
	elif r2 == 0.0:
		alpha = 0.0
		beta = 0.0
		gamma = 60 * (r0 / (r0 + r1))
	else:
		alpha = 60 * (r1 / (r1 + r2))
		beta = 60 * (r0 / (r0 + r2))
		gamma = 60 * (r0 / (r0 + r1))
	return (alpha, beta, gamma)

# Convert triangle angle tuple to ratio tuple.
# The angle is calculated using the percentage of the smaller affinity of the sum of both affinities.
def angle2ratio_factor(angle):
	# corners are:
	#   bottom left: a
	#   bottom right: b
	#   top: c
	# ratio is between corners
	alpha, beta, gamma = angle
	c_b = (1.0 / (alpha / 60.0)) - 1.0
	c_a = (1.0 / (beta / 60.0)) - 1.0
	b_a = (1.0 / (gamma / 60.0)) - 1.0
	if alpha < 30.0 and gamma > 30.0:
		# b is favorite
		return (b_a ,1.0 ,1.0/c_b)
	elif beta < 30.0 and gamma < 30.0:
		# a is favorite
		return (1.0, 1.0/b_a, 1.0/c_a)
	else:
		# c is favorite
		return (c_a, c_b, 1.0)
	

# Convert triangle angle tuple to ratio tuple.
# The angle is calculated using the percentage of the smaller affinity of the greater affinity.
def angle2ratio_sum(angle):
	# corners are:
	#   bottom left: a
	#   bottom right: b
	#   top: c
	# ratio is between corners
	alpha, beta, gamma = angle
	if alpha > 30.0:
		b_c = (60.0 - alpha) / 30.0
	else:
		b_c = 1.0 / (alpha / 30.0)
	if beta > 30.0:
		a_c = (60.0 - beta) / 30.0
	else:
		a_c = 1.0 / (beta / 30.0)
	if gamma > 30.0:
		a_b = (60.0 - gamma) / 30.0
	else:
		a_b = 1.0 / (gamma / 30.0)
	if b_c > 1.0 and a_b < 1.0:
		# b is favorite
		return (a_b, 1.0, 1.0 / b_c)
	elif a_b > 1.0 and a_c > 1.0:
		# a is favorite
		return (1.0, 1.0 / a_b, 1.0 / a_c)
	else:
		# c is favorite
		return (a_c, b_c,1.0)

# Convert ratio tuple to alpha, beta, gamme angle tuple for the triangle
# The angle is calculated using the percentage of the smaller affinity of the greater affinity.
def ratio2angle_sum(ratio):
	if ratio[1] < ratio[2]:
		alpha = 60.0 - 30.0 * (ratio[1] / ratio[2])
	else:
		alpha = 30.0 * (ratio[2] / ratio[1])
	if ratio[0] < ratio[2]:
		beta = 60.0 - 30.0 * (ratio[0] / ratio[2])
	else:
		beta = 30.0 * (ratio[2] / ratio[0])
	if ratio[0] < ratio[1]:
		gamma = 60.0 - 30.0 * (ratio[0] / ratio[1])
	else:
		gamma = 30.0 * (ratio[1] / ratio[0])
	return (alpha, beta, gamma)

# Convert triangle angle tuple to x,y coordinate tuple
def angle2tricoord(angle):
	# e.g. 0, 0. 40 
	c = (0.5, math.sqrt(3.0)/2.0)
	alpha, beta, gamma = angle
	if alpha == 0.0 and beta == 0.0:
		x = gamma / 60.0
		y = 0.0
	elif beta == 60.0 and gamma == 60.0:
		x = 1.0 - (alpha/60.0)*0.5
		y = (alpha/60.0)*c[1]
	elif alpha == 60.0 and gamma == 0.0:
		x = (beta/60.0)*0.5
		y = (beta/60.0)*c[1]
	else:
		a = math.tan(math.radians(alpha))
		b = math.tan(math.radians(beta))
		y = 1.0 / ( 1.0/a + 1.0/b )
		x = b / (a + b)
#		# check with third angle
#		# compute expected angle
#		gamma = angle[2]
#		# compute real angle
#		g_y = (math.sqrt(3.0)/2.0) - y
#		g_x = x-0.5
#		g_gamma = math.degrees(math.atan(g_x / g_y)) + 30.0
#		print(gamma, g_gamma)
	return (x,y)

# Convert x,y coordinate tuple to triangle angle tuple	
def tricoord2angle(coord):
	# corner points for equilateral triangle
	a = (0.0, 0.0)
	b = (1.0, 0.0)
	c = (0.5, math.sqrt(3.0)/2.0)
	# compute angles alpha, beta, gamma
	# alpha = angle in left bottom corner
	#       /|
	# a_d  / |y
	#     /  |
	#    /___|
	#      x

	# beta = angle in right bottom corner
	#   |\
	# y | \ b_d
	#   |  \
	#   |___\
	#    w-x 

	# gamma = angle in top corner
	# for computation use angle between bisector and point
	# and add 30.0 degrees
	# 
	#      /|	
	# g_d / | c[1]-y
	#    /  |
	#   /___|
	#    x - c[0]

	# hypotenuse for alpha angle
	a_d = math.sqrt(coord[0]**2 + coord[1]**2)
	# hypotenuse for beta angle
	b_d = math.sqrt((1.0-coord[0])**2 + coord[1]**2)
	# hypotenuse for gamma angle
	g_d = math.sqrt((coord[0]-c[0])**2 + (c[1]-coord[1])**2)

	# compute angles
	alpha = math.degrees(math.asin(coord[1]/a_d))
	beta = math.degrees(math.asin(coord[1]/b_d))
	g_x = coord[0]-0.5
	gamma = math.degrees(math.asin(g_x/g_d)) + 30.0
	return (alpha, beta, gamma)

# Apply the function f to the all values in the tuple r
def scale_ratio(r, f):
	s = []
	for v in r:
		s.append(f(v))
	return tuple(s)

# Return the given value
def ratio_identity(x):
	return x

# Return a function calling ratio_log() with the given base
def bind_ratio_log(base):
	return functools.partial(ratio_log, base)

# Logarithm of x according to base
def ratio_log(base,x):
	if x == 0.0:
		return x
	if x == 1.0:
		return x
	y = math.log(x, base)
	return y

# Return a function calling ratio_exp() with the given base
def bind_ratio_exp(base):
	return functools.partial(ratio_exp, base)

# Base to the power of x
def ratio_exp(base,x):
	if x == 0.0:
		return x
	if x == 1.0:
		return x
	#y = math.pow(x, base)
	y = math.pow(base, x)
	return y


# Return a function calling ratio_power() with the given base
def bind_ratio_power(base):
	return functools.partial(ratio_power, base)

# X to the power of base
def ratio_power(base, x):
	if x == 0.0:
		return x
	if x == 1.0:
		return x
	y = math.pow(x, base)
	return y


# Create a triangle colored with the resource affinity color map
# Returns an size x size array containing the equilateral triangle a the bottom.
#
# If alpha is used the array contains 4 components, else 3 components.
# Color values receive the $alpha value, while the background pixels receive 0.0 as alpha value.
def create_tod(size, use_alpha=True, alpha=1.0, bg=(0.0,0.0,0.0), scale_func=ratio_identity, ratio_func=angle2ratio_factor):
	w = float(size)
	h = float(size)

	# corner points for equilateral triangle
	a = (0.0, 0.0)
	b = (1.0, 0.0)
	c = (0.5, math.sqrt(3.0)/2.0)

	if use_alpha == True:
		bitmap = numpy.ndarray((size,size,4))
	else:
		bitmap = numpy.ndarray((size,size,3))

	for i in range(size):    # for every col:
		for j in range(size):    # For every row

			# compute pixel coordinate
			# use center of pixel
			x = (i/w + 0.5/w, (h-j-1)/h + 0.5/h)
			a_alpha, a_beta, a_gamma = tricoord2angle(x)

			if a_alpha < 0.0 or a_alpha > 60.0 or a_beta < 0.0 or a_beta > 60.0 or a_gamma < 0.0 or a_gamma > 60.0:
				# pixel outside of triangle
				r = bg[0]
				g = bg[1]
				b = bg[2]
				a = 0.0
			else:
				# pixel inside triangle
				a = alpha
				r,g,b = ratio_func((a_alpha,a_beta,a_gamma))

				r = scale_func(r)
				g = scale_func(g)
				b = scale_func(b)

				# switch colors for resource color mapping
				# bottom left CPU blue
				# bottom right GPU red
				# top FPGA green
				r,g,b = g,b,r

			if use_alpha == True:
				bitmap[j,i] = numpy.array([r, g, b, a])
			else:
				bitmap[j,i] = numpy.array([r, g, b])
	return bitmap

# Save array as returned by create_tod() to filename
def save_as_png(arr, filename):
	matplotlib.pyplot.imsave(filename, arr)


if __name__ == "__main__":

	if len(sys.argv) < 3:
		print(sys.argv[0], "size", "filename.png")
		print("Create resource affinity colored triangle.")
		print("Options:")
		print("\t\t","size:","Image width/height")
		print("\t\t","filename.png","Path to output image file")
		sys.exit(1)

	size = int(sys.argv[1])
	filename = sys.argv[2]

	arr = create_tod(size)
	save_as_png(arr, filename)

