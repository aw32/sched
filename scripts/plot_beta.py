#!/bin/env python3
# Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
# SPDX-License-Identifier: BSD-2-Clause


import sys
import scipy.stats
import matplotlib.pyplot as plt
import numpy

def help():
	print(sys.argv[0], "command", "options...")
	print("Commands:")
	print("\tpdf alpha beta")
	print("\t\tPlots the pdf/cdf for beta with given alpha/beta parameters")
	print("\tpdf-mean mean alpha-beta-sum")
	print("\t\tComputes alpha-beta so the given mean is achieved and plots the pdf")


# https://en.wikipedia.org/wiki/Beta_distribution
def beta_mode(alpha, beta):
	if alpha == 1.0 and beta == 1.0:
		return "(0,1)"
	elif alpha <= 1.0 and beta > 1.0:
		return 0.0
	elif alpha > 1.0 and beta <= 1.0:
		return 1.0
	elif alpha < 1.0 and beta < 1.0:
		return "{0.0,1.0}"
	else:
		return (alpha - 1.0) / (alpha + beta - 2.0)

def pdf():
	if len(sys.argv)<4:
		help()
		sys.exit(1)
	alpha = float(sys.argv[2])
	beta = float(sys.argv[3])

	beta_dist = scipy.stats.beta(alpha, beta)

	sample_points = numpy.linspace(0,1, 100)

	fig, ax1 = plt.subplots()
	pdf_line = ax1.plot(sample_points, beta_dist.pdf(sample_points), 'r-', label='pdf')
	ax1.set_ylabel('pdf', color="r")
	ax1.set_xlim([0,1])

	ax2 = ax1.twinx()
	cdf_line = ax2.plot(sample_points, beta_dist.cdf(sample_points), 'b-', label='cdf')
	ax2.set_ylabel('cdf', color="b")
	#ax1.legend(handles=[pdf_line[0], cdf_line[0]])

	ax1.set_title("alpha {}  beta {}\nmean {}  modus {}".format(alpha, beta, beta_dist.mean(), beta_mode(alpha, beta)))

	ax1.grid(True,"both")
	fig.canvas.set_window_title("Beta distribution")
	plt.show()


def pdf_mean():
	if len(sys.argv)<4:
		help()
		sys.exit(1)
	mean = float(sys.argv[2])
	absum = float(sys.argv[3])

	offset = 0.0000001 # is used for extreme value 0.0 and 1.0, so alpha/beta never reach invalid 0.0
	beta = (absum) - mean * (absum)
	if beta == 0.0:
		beta += offset
	alpha = absum - beta
	if alpha == 0.0:
		alpha += offset

	beta_dist = scipy.stats.beta(alpha, beta)
	realmean = beta_dist.mean()

	sample_points = numpy.linspace(0,1, 100)

	fig, ax1 = plt.subplots()
	pdf_line = ax1.plot(sample_points, beta_dist.pdf(sample_points), 'r-', label='pdf')
	ax1.set_ylabel('pdf', color="r")
	ax1.set_xlim([0,1])

	ax2 = ax1.twinx()
	cdf_line = ax2.plot(sample_points, beta_dist.cdf(sample_points), 'b-', label='cdf')
	ax2.set_ylabel('cdf', color="b")
	#ax1.legend(handles=[pdf_line[0], cdf_line[0]])

	ax1.set_title("mean {}  absum {}\nalpha {}  beta {}\nreal mean {}  modus {}".format(mean, absum, alpha, beta, beta_dist.mean(), beta_mode(alpha, beta)))

	ax1.grid(True,"both")
	fig.canvas.set_window_title("Beta distribution")
	plt.show()



if __name__ == "__main__":

	if len(sys.argv)<2:
		help()
		sys.exit(1)

	command = sys.argv[1]

	if command == "pdf":
		pdf()
	elif command == "pdf-mean":
		pdf_mean()
	else:
		help()



