#!/usr/bin/env python3
# Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
# SPDX-License-Identifier: BSD-2-Clause


import os
import os.path
import subprocess

import test as schedtest
import plot

def hostname():
	return subprocess.getoutput("hostname")

if __name__ == "__main__":

	cwd = os.getcwd()
	testname = os.path.basename(cwd)
	host = os.environ if "SCHED_HOST" in os.environ else hostname()


	for testtype in ["sim","exp"]:
		test = schedtest.SchedTest.loadTest(testtype, testname=testname, resultdir=cwd, host=host)
		if test != None and test.loadTestLog():
			test.generate_report()
		else:
			print("log for",testtype,"not found")
