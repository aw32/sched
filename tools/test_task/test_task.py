#!/usr/bin/env python3
# Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
# SPDX-License-Identifier: BSD-2-Clause


import os
import sys
import socket
import fcntl
import errno
import time
import traceback


def split_bytes(data):
	out = []
	cur = bytes()
	for b in data:
		#print(type(b))
		if b == 0x00:
			if len(cur) != 0:
				out += [cur]
			cur = bytes()
		else:
			cur += bytes([b])
	return out

def read_messages(client):
	# Read incoming message
	databuffer = bytes()
	income = True
	while income == True:
		data = bytes()
		try:
			data = client.recv(1024)
		except BlockingIOError as e:
		#except socket.error, e:
			#err = e.args[0]
			#if err == errno.EAGAIN or err == errno.EWOULDBLOCK:
			if e.errno == errno.EAGAIN or e.errno == errno.EWOULDBLOCK:
				break
			else:
				print(e)
				break
		if len(data) != 0:
			databuffer += data
		else:
			income = False

	inmessages = []
	if len(databuffer)>0:
		data_messages = split_bytes(databuffer)
		for inmsg in data_messages:
			inmessages += [inmsg.decode("ASCII")]
	return inmessages

if len(sys.argv) < 2:
	print(sys.argv[0], "messages.txt")
	sys.exit(1)

message_file = sys.argv[1]

messages = []

with open(message_file, "r") as f:
	for line in f.readlines():
		line = line.strip()
		split_index = line.find(" ")
		if split_index == -1:
			continue
		cmd = line[0:split_index]
		time_sec = 0.0
		msg = line[split_index+1:]
		if cmd.startswith("#"):
			continue
		if cmd == "wait":
			time_sec = float(msg)
		elif cmd == "recv":
			pass
		elif cmd == "send":
			pass
		else:
			print("!!!   Unknown command:", cmd)
		messages.append((cmd, time_sec, msg))



socket_name = "/tmp/test.socket"



server = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
server.bind(socket_name)
server.listen(1)

(client,client_addr) = server.accept()

print("# accepted",client_addr)
#fcntl.fcntl(s, fcntl.F_SETFL, os.O_NONBLOCK)
#client.setblocking(False)

try:
	done = False
	for (cmd, time_sec, msg) in messages:

		print("#", cmd)

		if cmd == "wait":
			# Wait
			if time_sec > 0.0:
				time.sleep(time_sec)
		elif cmd == "send":
			# Write message
			print(">", msg)
			client.send(msg.encode("ASCII") + bytes(1))
		elif cmd == "recv":
			found = False
			while found == False:
				inmsgs = read_messages(client)
				if inmsgs == None or len(inmsgs) == 0:
					done = True
					break
				for inmsg in inmsgs:
					print("<", inmsg)
					if inmsg == msg:
						found = True
		if done == True:
			break
except Exception as e:
	traceback.print_exc()

try:
	server.close()
except Exception as e:
	print(e)

os.remove(socket_name)
