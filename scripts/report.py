#!/usr/bin/env python3
# Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
# SPDX-License-Identifier: BSD-2-Clause


import os
import os.path
import mako.template

script_path = os.path.dirname(os.path.realpath(__file__))
mako_report = mako.template.Template(filename=os.path.join(script_path,"report.md"), input_encoding="utf-8")

def create_report(test, name, log, wraplogs, testtype, output_prefix):

	mdfile = os.path.join(output_prefix + "_report.md")

	with open(mdfile, "w") as freport:
		freport.write(mako_report.render(testname=name, log=log, wraplogs=wraplogs, testtype=testtype, test=test))

	htmlfile = os.path.join(output_prefix + "_report.html")
	#pandoc -s -f markdown -t html -o sim_report.html sim_report.md
	os.system("pandoc -s -f markdown -t html -o "+htmlfile+" "+mdfile)
