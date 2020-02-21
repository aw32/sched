#!/bin/python3
# Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
# SPDX-License-Identifier: BSD-2-Clause


import sys


if len(sys.argv)<6:
	print(sys.argv[0],"file","offsetx","offsety","realminx","realmaxx")
	print("\t","offsetx - arbitrary x offset for all points")
	print("\t","offsety - arbitrary y offset for all points")
	print("\t","realmin/maxx - real lower/upper limit for x, x values will be mapped to the real x range")
	print("\t","file - containing svg d commands with one command per line")
	sys.exit(1)


infile=sys.argv[1]
offsetx=float(sys.argv[2])
offsety=float(sys.argv[3])
state=""
currentx = 0.0
currenty = 0.0

minrealx=float(sys.argv[4])
maxrealx=float(sys.argv[5])
minrealy=0.0
maxrealy=1.0

lines = []

with open(infile,"r") as f:
	count = 0
	for l in f.readlines():
		count += 1
		#print(l)
		if l[0] == "m":
			state = "l"
			parts = l[2:].split(",")
			currentx = offsetx + float(parts[0])
			currenty = offsety + float(parts[1])
			#print("new %f %f" % (currentx, currenty))
			continue
		elif l[0] == "l":
			state = "l"
			parts = l[2:].split(",")
			newx = currentx + float(parts[0])
			newy = currenty + float(parts[1])
			lines.append((currentx,currenty,newx,newy))
			#print("%f %f -> %f %f" % (currentx, currenty, newx, newy))
			currentx = newx
			currenty = newy
			continue
		elif l[0] == "L":
			state = "L"
			parts = l[2:].split(",")
			newx = float(parts[0])
			newy = float(parts[1])
			lines.append((currentx,currenty,newx,newy))
			#print("%f %f -> %f %f" % (currentx, currenty, newx, newy))
			currentx = newx
			currenty = newy
			continue
		elif l[0] == "h":
			state = "h"
			newx = currentx + float(l[2:])
			lines.append((currentx,currenty,newx,currenty))
			#print("%f %f -> %f %f" % (currentx, currenty, newx, currenty))
			currentx = newx
			continue
		else:
			if state=="l":
				parts = l.split(",")
				newx = currentx + float(parts[0])
				newy = currenty + float(parts[1])
				lines.append((currentx,currenty,newx,newy))
				#print("%f %f -> %f %f" % (currentx, currenty, newx, newy))
				currentx = newx
				currenty = newy
			elif state=="L":
				parts = l.split(",")
				newx = float(parts[0])
				newy = float(parts[1])
				lines.append((currentx,currenty,newx,newy))
				#print("%f %f -> %f %f" % (currentx, currenty, newx, newy))
				currentx = newx
				currenty = newy
			
			elif state=="h":
				newx = currentx + float(l)
				lines.append((currentx,currenty,newx,currenty))
				#print("%f %f -> %f %f" % (currentx, currenty, newx, currenty))
				currentx = newx
			else:
				print("oops line", count)
				break

minx = 0.0
maxx = 0.0
maxy = 0.0
miny = float("inf")
for l in lines:
	if l[0]>maxx:
		maxx = l[0]
	if l[2]>maxx:
		maxx = l[2]
	if l[1]>maxy:
		maxy = l[1]
	if l[3]>maxy:
		maxy = l[3]
	if l[1]<miny:
		miny = l[1]
	if l[3]<miny:
		miny = l[3]
maxy += -offsety
maxyy = maxy
minyy = miny
#print("miny %f maxy %f minyy %f maxyy %f" % (miny,maxy,minyy,maxyy))
newlines = []
count = 0
for ix,l in enumerate(lines):
	count += 1
	if ix == 0:
		#newlines.append((0.0,maxy,l[0],l[1]))
		pass
	else:
		if l[0] != currentx or l[1] != currenty:
			print("oops line", count, "current %f %f" % (currentx, currenty), "line %f %f" % (l[0],l[1]))
			newlines.append((currentx,currenty,l[0],l[1]))
	currentx = l[2]
	currenty = l[3]
	newlines.append(l)
lines = newlines

# reverse order
if lines[0][0] > lines[0][2]:
	newlines = []
	for l in lines:
		newlines.append((l[2],l[3],l[0],l[1]))
	newlines.reverse()
	lines = newlines

def realx(x):
	return ( ( (x-minx)/ (maxx-minx)) * (maxrealx-minrealx) ) + minrealx
def realy(y):
	return (((y-minyy)/(maxyy-minyy))*(maxrealy-minrealy))+minrealy

#with open("google.out","w") as f:
f = sys.stdout
for l in lines:
	correct = (l[0],maxy-l[1]+miny,l[2],maxy-l[3]+miny)
	correct = (  realx(correct[0]), realy(correct[1]), realx(correct[2]), realy(correct[3]) )
	f.write("%f %f %f %f\n" % correct)

#print(minyy)
#print(realy(minyy))
#print(maxyy)
#print(realy(maxyy))
#print(lines[0])
#print(maxy)
#print(maxy - lines[0][1])
#print(realy(maxy - lines[0][1]))
