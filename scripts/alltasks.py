#!/usr/bin/env python3
# Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
# SPDX-License-Identifier: BSD-2-Clause


foo = {\
	"heat":[256,512,1024,2048,4096,8192,16384],\
	"correlation":[256,512,1024,2048,4096,8192,16384],\
	"bfs":[128,256,512,1024,2048,4096,8192,16384],\
	"markov":[128,256,512,1024,2048,4096],\
	"gaussblur":[256,512,1024,2048,4096,8192,16384]}


i = 0
for t in foo:
	for s in foo[t]:
		e = '"type":"TASKDEF","id":{0:d},"name":"{1:s}","size":{2:d},"checkpoints":256,"dependencies":[2],"resources":[]'.format(i, t, s)
		i += 1
		print("{",e,"},")
