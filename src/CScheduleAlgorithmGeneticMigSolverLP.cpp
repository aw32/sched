// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#include <chrono>
#include <cstdio>
#include <unistd.h>
#include <sys/wait.h>
#include <fstream>
#include <iostream>
#include "CScheduleAlgorithmGeneticMigSolverLP.h"
#include "CEstimation.h"
#include "CLogger.h"
#include "CConfig.h"
using namespace sched::algorithm;
using sched::task::CTask;
using sched::algorithm::CEstimation;

/*
Objective: min (  max(r_1, .., r_n)  )
Minimize largest of all machine ready times
Therefore create dummy variable m, that is bound
by the single machine ready times.
Minimize m
	min ( m )
	m >= r_1, .. , m >= r_n

Machine ready time is defined by the finish time of the last task
	r_1 = f_n



Task finish time = finish time of second part
	f_1 = f_1_2

Part active boolean variables
	1 <= b_1_a + b_1_b
	b_1_a + b_1_b <= 2
Inverted variables
	bi_1_a = 1 - b_1_a
	bi_1_b = 1 - b_1_b

Ratio of checkpoints
	0 <= t_1_a
	t_1_a <= b_1_a	// if b_1_a = 0 then t_1_a = 0
	0 <= t_1_b
	t_1_b <= b_1_b
	t_1_a + t_1_b = 1

Dynamic time: percentage of checkpoints executed on the given machine
(for the given LP a constant factor)
	d_1_a
	d_1_b

Constant time: init + fini time on given machine if part is active
(for the given LP a constant factor)
	c_1_a
	c_1_b

Part finish time = ready time + constant time + dynamic time
Two kinds of dependencies: machine order dependency + task order dependency
	fm_1_a = rm_1_a + c_1_a * b_1_a + d_1_a * t_1_a
	fd_1_a = rd_1_a + c_1_a * b_1_a + d_1_a * t_1_a

	fm_1_b = rm_1_b + c_1_b * b_1_b + d_1_b * t_1_b
	fd_1_b = rd_1_b + c_1_b * b_1_b + d_1_b * t_1_b


M is a constant larger than the makespan m

Machine order dependency:
	fm_0_a <= rm_1_a
	fm_0_a - M*bi_1_a <= rd_1_a

Task order dependency:
	fd_0_a - M*bi_1_a <= rm_1_a
	fd_0_a <= rd_1_a

 If part 1_a is inactive
 => rm_1_a fallsback to fm_0_a
 => rd_1_a fallsback to fd_0_a


Intra task dependency is also a task dependency:
	fd_1_a <= rd_1_b

The part next in machine order is dependent on fm_1_a.



Simple case:
In case the task's two parts are neighbors, simple rules are used
	f_1_b = r_1_a + c_1_a + d_1_a
(f_1_a and r_1_b are omitted!)

*/
void CScheduleAlgorithmGeneticMigSolverLP::solve(CScheduleAlgorithmGeneticMig::CGeneticSchedule* schedule,
	std::vector<CResource*>& rResources,
	CEstimation* est,
	std::vector<CTaskCopy>* pTasks,
	std::unordered_map<int,int>* taskmap,
	double max_double,
	std::vector<CTaskCopy*>* runningTasks,
	char* solverPath) {

	if (schedule->check() == false) {
		CLogger::mainlog->debug("Solver: sched check failed");
	} else {
		CLogger::mainlog->debug("Solver: sched check ok");
	}

	// create timestamp
	std::chrono::time_point<std::chrono::high_resolution_clock> now = std::chrono::high_resolution_clock::now();
	long long timestamp = now.time_since_epoch().count();

	CConfig* conf = CConfig::getConfig();
	std::string* lpdest = 0;
	int ret = conf->conf->getString((char*)"lp_destination", &lpdest);
	if (ret != 0 || lpdest == 0) {		
		CLogger::mainlog->error("Solver: lp_destination not defined");
		return;
	}

	std::ostringstream lpfilename;
	lpfilename << *lpdest << "/" << timestamp << ".lp";

	std::ostringstream lpsolutionname;
	lpsolutionname << *lpdest << "/" << timestamp << ".out";

	std::ofstream lpfile;

	//lpfile.open("/tmp/lpfile.lp");
	lpfile.open(lpfilename.str());

	lpfile << std::fixed;
	lpfile << "min: m;\n";

//	unsigned long long lp_max = 0xffffffffffffffff; //accuracy errors in lp_solve
//	unsigned long long lp_max = 0x7fffffffffffff; //2^55 -1
//	unsigned long long lp_max = 0x3fffffffffffff; //2^54 -1
//	unsigned long long lp_max = 0xffffffffff; //2^40 -1
	unsigned long long lp_max = 0;

	double lp_max_double = 0.0;

	// pre compute maximal runtime
	for (unsigned int tix=0; tix < pTasks->size(); tix++) {
			CTask* task = &((*pTasks)[tix]);
			for (unsigned int mix=0; mix<rResources.size(); mix++) {
				CResource* res = rResources[mix];
				double init = est->taskTimeInit(task, res);
				double fini = est->taskTimeFini(task, res);
				double compute = est->taskTimeCompute(task, res, task->mProgress, task->mCheckpoints);
				lp_max_double += init + init +fini +fini + compute;
			}
	}

	lp_max = lp_max_double;

	CLogger::mainlog->debug("LP start");

	std::vector<CScheduleAlgorithmGeneticMigSolverLP::Task> tasklist(pTasks->size());

	// find parts
	for (unsigned int mix=0; mix<rResources.size(); mix++) {
		for (int rix=0; rix<schedule->num[mix]; rix++) {
			int tix = schedule->get(mix, rix)->taskid;
			int part = schedule->get(mix, rix)->part;
				CLogger::mainlog->debug("GeneticSolver: find tix %d part %d mix %d rix %d\n", tix, part, mix, rix);
			if (part == 0) {
				tasklist[tix].mix_a = mix;
				tasklist[tix].slot_a = rix;
			} else {
				tasklist[tix].mix_b = mix;
				tasklist[tix].slot_b = rix;
			}
		}
	}

	for (unsigned int mix=0; mix<rResources.size(); mix++) {
		CResource* res = rResources[mix];
		for (int rix=0; rix<schedule->num[mix]; rix++) {
			int tix = schedule->get(mix, rix)->taskid;
			int part = schedule->get(mix, rix)->part;

			CTask* task = &((*pTasks)[tix]);
			double init = est->taskTimeInit(task, res);
			double fini = est->taskTimeFini(task, res);
			double compute = est->taskTimeCompute(task, res, task->mProgress, task->mCheckpoints);


			// simple case: parts are on same machine direct neighbors (-> part a -> part b ->)
			if (tasklist[tix].mix_a == tasklist[tix].mix_b &&
				(tasklist[tix].slot_a - tasklist[tix].slot_b == 1 ||
				tasklist[tix].slot_b - tasklist[tix].slot_a == 1)
				) {

				if (part == 1) {
					continue;
				}

				// fm_x_b = rm_x_a + C;
				CLogger::mainlog->debug("LP fm_%d_b == rm_%d_a + %lf;",
					tix, tix, (init + fini + compute));
				lpfile << "fm_" << tix << "_b" << " = " <<
					"rm_" << tix << "_a + " << (init + fini + compute) << ";\n";

				// fd_x_b = rd_x_a + C;
				CLogger::mainlog->debug("LP fd_%d_b == rd_%d_a + %lf;",
					tix, tix, (init + fini + compute));

				lpfile << "fd_" << tix << "_b" << " = " <<
					"rd_" << tix << "_a + " << (init + fini + compute) << ";\n";

				tasklist[tix].merge = true;

				// simple case machine order
				if (rix > 0) {

					int pre_tix = schedule->get(mix, rix-1)->taskid;
					int pre_part = schedule->get(mix, rix-1)->part;
					if (pre_part == 0) {

						// fm_y_a <= rm_x_a;
						CLogger::mainlog->debug("LP fm_%d_a <= rm_%d_a;", pre_tix, tix);
						lpfile << "fm_" << pre_tix << "_a <= rm_" << tix << "_a;\n";

						// fm_y_a <= rd_x_a;
						CLogger::mainlog->debug("LP fm_%d_a <= rd_%d_a;", pre_tix, tix);
						lpfile << "fm_" << pre_tix << "_a <= rd_" << tix << "_a;\n";

					} else {
						// fm_y_b <= rm_x_a;
						CLogger::mainlog->debug("LP fm_%d_b <= rm_%d_a;", pre_tix, tix);
						lpfile << "fm_" << pre_tix << "_b <= rm_" << tix << "_a;\n";

						// fm_y_b <= rd_x_a;
						CLogger::mainlog->debug("LP fm_%d_b <= rd_%d_a;", pre_tix, tix);
						lpfile << "fm_" << pre_tix << "_b <= rd_" << tix << "_a;\n";
					}

				}


				// simple case task dependency
				if (rix>0 && part == 0) {
					for (int pix=0; pix < task->mPredecessorNum; pix++) {
						int pid = task->mpPredecessorList[pix];
						if (taskmap->count(pid) == 0) {
							continue;
						}
						int pre_ix = (*taskmap)[pid];

						// add dependency constraint:
						// part a ready time >= predecessor part b finish time
						// fd_y_b <= rm_x_a;
						CLogger::mainlog->debug("LP fd_%d_b <= rm_%d_a;", pre_ix, tix);
						lpfile << "fd_" << pre_ix << "_b <= rm_" << tix << "_a;\n";

						// fd_y_b <= rd_x_a;
						CLogger::mainlog->debug("LP fd_%d_b <= rd_%d_a;", pre_ix, tix);
						lpfile << "fd_" << pre_ix << "_b <= rd_" << tix << "_a;\n";
					}
				}

				// simple case machine ready
				if (tasklist[tix].slot_b == schedule->num[mix]-1) {
					// fm_x_b <= m;
					CLogger::mainlog->debug("LP fm_%d_b <= m;", tix);
					lpfile << "fm_" << tix << "_b <= m;\n";

					// fd_x_b <= m;
					CLogger::mainlog->debug("LP fd_%d_b <= m;", tix);
					lpfile << "fd_" << tix << "_b <= m;\n";
				}

				// 0 <= rm_x_a;
				CLogger::mainlog->debug("LP 0 <= rm_%d_a;", tix);
				lpfile << "0 <= rm_" << tix << "_a;\n";

				// 0 <= rd_x_a;
				CLogger::mainlog->debug("LP 0 <= rd_%d_a;", tix);
				lpfile << "0 <= rd_" << tix << "_a;\n";

				continue;
			} // simple case end

			tasklist[tix].merge = false;

			if (part == 0) {

				// 0 <= rm_x_a;
				CLogger::mainlog->debug("LP 0 <= rm_%d_a;", tix);
				lpfile << "0 <= rm_" << tix << "_a;\n";
				// 0 <= rd_x_a;
				CLogger::mainlog->debug("LP 0 <= rd_%d_a;", tix);
				lpfile << "0 <= rd_" << tix << "_a;\n";

				// 0 <= rm_x_b;
				CLogger::mainlog->debug("LP 0 <= rm_%d_b;", tix);
				lpfile << "0 <= rm_" << tix << "_b;\n";
				// 0 <= rd_x_b;
				CLogger::mainlog->debug("LP 0 <= rd_%d_b;", tix);
				lpfile << "0 <= rd_" << tix << "_b;\n";

				// intra task dependency (no machine order fm <= rm !)
				// fd_x_a <= rd_x_b;
				CLogger::mainlog->debug("LP fd_%d_a <= rd_%d_b;", tix, tix);
				lpfile << "fd_" << tix << "_a <= rd_" << tix << "_b;\n";

				// fd_x_a - M * bi_x_b <= rm_x_b;
				CLogger::mainlog->debug("LP fd_%d_a - lp_max * bi_%d_b <= rd_%d_b;", tix, tix, tix);
				lpfile << "fd_" << tix << "_a - " << lp_max << " * bi_"<< tix << "_b <= rm_" << tix << "_b;\n";

				double constant_res_a = init + fini;
				double duration_res_a = compute;
				CLogger::mainlog->debug("Solver: def dura");

				// 1 <= b_x_a + b_x_b <= 2;
				CLogger::mainlog->debug("LP 1 <= b_%d_a + b_%d_b <= 2;", tix, tix);
				lpfile << "1 <= b_" << tix << "_a + b_" << tix << "_b <= 2;\n";

				// bi_x_a = 1 - b_x_a;
				CLogger::mainlog->debug("LP bi_%d_a = 1- b_%d_a;", tix, tix);
				lpfile << "bi_" << tix << "_a = 1- b_" << tix << "_a;\n";

				// bi_x_b = 1 - b_x_b;
				CLogger::mainlog->debug("LP bi_%d_b = 1- b_%d_b;", tix, tix);
				lpfile << "bi_" << tix << "_b = 1- b_" << tix << "_b;\n";

				// 0 <= t_x_a;
				CLogger::mainlog->debug("LP 0 <= t_%d_a;", tix);
				lpfile << "0 <= t_" << tix << "_a;\n";

				// t_x_a <= b_x_a;
				CLogger::mainlog->debug("LP t_%d_a <= b_%d_a;", tix, tix);
				lpfile << "t_" << tix << "_a <= b_" << tix << "_a;\n";

				// t_x_a + t_x_b = 1;
				CLogger::mainlog->debug("LP t_%d_a + t_%d_b = 1;",
					tix, tix);
				lpfile << "t_" << tix << "_a + t_" << tix << "_b = 1;\n";

				// fm_x_a = rm_x_a + const * b_x_a + dyn * t_x_a;
				CLogger::mainlog->debug("LP fm_%d_a = rm_%d_a + %lf * b_%d_a + %lf * t_%d_a;",
					tix, tix, constant_res_a, tix, duration_res_a, tix);
				lpfile << "fm_" << tix << "_a = rm_" << tix << "_a + " <<
					constant_res_a << " * b_" << tix << "_a + " << duration_res_a << " * t_" << tix << "_a;\n";

				// fd_x_a = rd_x_a + const * b_x_a + dyn * t_x_a;
				CLogger::mainlog->debug("LP fd_%d_a = rd_%d_a + %lf * b_%d_a + %lf * t_%d_a;",
					tix, tix, constant_res_a, tix, duration_res_a, tix);
				lpfile << "fd_" << tix << "_a = rd_" << tix << "_a + " <<
					constant_res_a << " * b_" << tix << "_a + " << duration_res_a << " * t_" << tix << "_a;\n";
				
			} else {

				// 0 <= t_x_b;
				CLogger::mainlog->debug("LP 0 <= t_%d_b;", tix);
				lpfile << "0 <= t_" << tix << "_b;\n";

				// t_x_b <= b_x_b;
				CLogger::mainlog->debug("LP t_%d_b <= b_%d_b;", tix, tix);
				lpfile << "t_" << tix << "_b <= b_" << tix << "_b;\n";

				double constant_res_b = init + fini;
				double duration_res_b = compute;
				CLogger::mainlog->debug("Solver: def durb tix %d",tix);

				// fm_x_b = rm_x_b + const * b_x_b + dyn * t_x_b;
				CLogger::mainlog->debug("LP fm_%d_b == rm_%d_b + %lf * b_%d_b + %lf * t_%d_b;",
					tix, tix, constant_res_b, tix, duration_res_b, tix);
				lpfile << "fm_" << tix << "_b = rm_" << tix << "_b + " <<
					constant_res_b << " * b_" << tix << "_b + " << duration_res_b << " * t_" << tix << "_b;\n";

				// fd_x_b = rd_x_b + const * b_x_b + dyn * t_x_b;
				CLogger::mainlog->debug("LP fd_%d_b == rd_%d_b + %lf * b_%d_b + %lf * t_%d_b;",
					tix, tix, constant_res_b, tix, duration_res_b, tix);
				lpfile << "fd_" << tix << "_b = rd_" << tix << "_b + " <<
					constant_res_b << " * b_" << tix << "_b + " << duration_res_b << " * t_" << tix << "_b;\n";

			}

			// machine order
			if (rix>0) {
				int pre_tix = schedule->get(mix, rix-1)->taskid;
				int pre_part = schedule->get(mix, rix-1)->part;
				if (part == 0) {
					if (pre_part == 0) {
						// fm_x_a <= rm_x_a;
						CLogger::mainlog->debug("LP fm_%d_a <= rm_%d_a;", pre_tix, tix);
						lpfile << "fm_" << pre_tix << "_a <= rm_" << tix << "_a;\n";

						// fm_x_a - M * bi_x_a <= rd_x_a;
						CLogger::mainlog->debug("LP fm_%d_a - lp_max * bi_%d_a <= rd_%d_a;", pre_tix, tix, tix);
						lpfile << "fm_" << pre_tix << "_a - " << lp_max << " * bi_" << tix << 
						"_a <= rd_" << tix << "_a;\n";

					} else {
						// fm_x_b <= rm_x_a;
						CLogger::mainlog->debug("LP fm_%d_b <= rm_%d_a;", pre_tix, tix);
						lpfile << "fm_" << pre_tix << "_b <= rm_" << tix << "_a;\n";

						// fm_x_b - M * bi_x_a <= rd_x_a;
						CLogger::mainlog->debug("LP fm_%d_b - lp_max * bi_%d_a <= rd_%d_a;", pre_tix, tix, tix);
						lpfile << "fm_" << pre_tix << "_b - " << lp_max << " * bi_" << tix << "_a <= rd_" << tix << "_a;\n";

					}
				} else {
					if (pre_part == 0) {
						// fm_x_a <= rm_x_b;
						CLogger::mainlog->debug("LP fm_%d_a <= rm_%d_b;", pre_tix, tix);
						lpfile << "fm_" << pre_tix << "_a <= rm_" << tix << "_b;\n";

						// fm_x_a - M * bi_x_b <= rd_x_b;
						CLogger::mainlog->debug("LP fm_%d_a - lp_max * bi_%d_b <= rd_%d_b;", pre_tix, tix, tix);
						lpfile << "fm_" << pre_tix << "_a - " << lp_max << " * bi_" << tix << "_b <= rd_" << tix << "_b;\n";

					} else {
						// fm_x_b <= rm_x_b;
						CLogger::mainlog->debug("LP fm_%d_b <= rm_%d_b;", pre_tix, tix);
						lpfile << "fm_" << pre_tix << "_b <= rm_" << tix << "_b;\n";

						// fm_x_b - M * bi_x_b <= rd_x_b;
						CLogger::mainlog->debug("LP fm_%d_b - lp_max * bi_%d_b <= rd_%d_b;", pre_tix, tix, tix);
						lpfile << "fm_" << pre_tix << "_b - " << lp_max << " * bi_" << tix << "_b <= rd_" << tix << "_b;\n";

					}
				}
			}

			// task dependency
			if (rix>0 && part == 0) {
				for (int pix=0; pix < task->mPredecessorNum; pix++) {
					int pid = task->mpPredecessorList[pix];
					if (taskmap->count(pid) == 0) {
						continue;
					}
					int pre_ix = (*taskmap)[pid];

					// add dependency constraint:
					// part a ready time >= predecessor part b finish time

					// fd_x_b - M * bi_x_a <= rm_x_a;
					CLogger::mainlog->debug("LP fd_%d_b - lp_max * bi_%d_a <= rm_%d_a;", pre_ix, tix, tix);
					lpfile << "fd_" << pre_ix << "_b - " << lp_max << " * bi_" << tix << "_a <= rm_" << tix << "_a;\n";

					// fd_x_b <= rd_x_a;
					CLogger::mainlog->debug("LP fd_%d_b <= rd_%d_a;", pre_ix, tix);
					lpfile << "fd_" << pre_ix << "_b <= rd_" << tix << "_a;\n";

				}
			}

			// machine ready
			if (rix == schedule->num[mix]-1) {
				if (part == 0) {
					// fm_x_a <= m;
					CLogger::mainlog->debug("LP fm_%d_a <= m;", tix);
					lpfile << "fm_" << tix << "_a <= m;\n";

					// fd_x_a <= m;
					CLogger::mainlog->debug("LP fd_%d_a <= m;", tix);
					lpfile << "fd_" << tix << "_a <= m;\n";
				} else {
					// fm_x_b <= m;
					CLogger::mainlog->debug("LP fm_%d_b <= m;", tix);
					lpfile << "fm_" << tix << "_b <= m;\n";

					// fd_x_b <= m;
					CLogger::mainlog->debug("LP fd_%d_b <= m;", tix);
					lpfile << "fd_" << tix << "_b <= m;\n";
				}
			}
		}
	}

	CLogger::mainlog->debug("LP 0 <= m;");
	lpfile << "0 <= m;\n";

	for (unsigned int tix=0; tix<pTasks->size(); tix++) {
		// special case: parts are on same machine direct neighbors
			if (tasklist[tix].mix_a == tasklist[tix].mix_b &&
				(tasklist[tix].slot_a - tasklist[tix].slot_b == 1 ||
				tasklist[tix].slot_b - tasklist[tix].slot_a == 1)
				) {

				// no binary variables	

			} else {
				// bin b_x_a, b_x_b, bi_x_a, bi_x_b
				CLogger::mainlog->debug("LP bin b_%d_a, b_%d_b, bi_%d_a, bi_%d_b;", tix, tix, tix, tix);
				lpfile << "bin b_" << tix << "_a, b_" << tix << "_b, bi_" << tix << "_a, bi_" << tix << "_b;\n";
			}
	}

	lpfile.close();
	CLogger::mainlog->debug("LP min: m;");
	CLogger::mainlog->debug("LP stop");

	// create lp execution arguments
	std::string lpfil_str = lpfilename.str();
	std::string lpsol_str = lpsolutionname.str();


	const char* arguments[] = {
		solverPath,
		lpfil_str.c_str(),
		lpsol_str.c_str(),
		0
	};


	// start lp
	int status = 0; 

	pid_t pid = fork();
	if (pid == -1) {
		// fork failed
		int error = errno;
		errno = 0;
		CLogger::mainlog->debug("GeneticSolver: execute lp_solve failed %d %s", error, strerror(error));
	} else
	if (pid == 0) {
		// child
		execv(arguments[0], (char* const*) arguments);
		// execv should never return
		_exit(1);

	} else {
		// parent
		int status_info = 0;
		pid_t ret = waitpid(pid, &status_info, 0);
		if (-1 == ret) {
			int error = errno;
			errno = 0;
			CLogger::mainlog->debug("GeneticSolver: execute lp_solve ok, waitpid failed %d %s", error, strerror(error));
		} else {
			status = WEXITSTATUS(status_info);
		}
	}

	CLogger::mainlog->info("GeneticSolver: execute lp_solve = %d", status);

	// read solution
	std::ifstream infile(lpsolutionname.str().c_str());
	std::string line;
	while (std::getline(infile, line))
	{
		if (line.size() == 0) {
			continue;
		}
		if (line[0] == 'x') {
			CLogger::mainlog->debug("GeneticSolver: %s\n", line.c_str());
		} else
		if (line[0] == 'm') {
			double m = std::stod(line.substr(1));
			CLogger::mainlog->debug("GeneticSolver: m %lf\n", m);
			schedule->fitness = m;
		} else
		if (line[0] == 'b') {
			CLogger::mainlog->debug("GeneticSolver: %s\n", line.c_str());
		} else
		if (line[0] == 't') {
			CLogger::mainlog->debug("GeneticSolver: %s\n", line.c_str());
			size_t next = 0;
			int tix = std::stoi(line.substr(2), &next);
			int part = 0;
			if (line[next+3] == 'b') {
				part = 1;
			}
			
			double ratio = std::stod(line.substr(next+4));
			if (part == 0) {
				CLogger::mainlog->debug("GeneticSolver: set tix %d part a mix %d slot %d ratio %lf\n", tix, tasklist[tix].mix_a, tasklist[tix].slot_a, ratio);
				schedule->get(tasklist[tix].mix_a, tasklist[tix].slot_a)->ratio = ratio;
			} else {
				CLogger::mainlog->debug("GeneticSolver: set tix %d part b mix %d slot %d ratio %lf\n", tix, tasklist[tix].mix_b, tasklist[tix].slot_b, ratio);
				schedule->get(tasklist[tix].mix_b, tasklist[tix].slot_b)->ratio = ratio;
			}
		}
		else
		if (line[0] == 'r') {
			CLogger::mainlog->debug("GeneticSolver: %s\n", line.c_str());
			size_t next = 0;
			int rd = 0; // 0 => rm, 1 => rd
			if (line[1] == 'd') {
				rd = 1;
			}
			int tix = std::stoi(line.substr(3), &next);
			int part = 0;
			if (line[next+4] == 'b') {
				part = 1;
			}
			
			double ready = std::stod(line.substr(next+5));
			if (part == 0) {
				if (rd == 0) {
					tasklist[tix].rm_a = ready;
				} else {
					tasklist[tix].rd_a = ready;
				}
			} else {
				if (rd == 0) {
					tasklist[tix].rm_b = ready;
				} else {
					tasklist[tix].rd_b = ready;
				}
			}
		}
		else
		if (line[0] == 'f') {
			CLogger::mainlog->debug("GeneticSolver: %s\n", line.c_str());
			size_t next = 0;
			int fd = 0; // 0 => fm, 1 => fd
			if (line[1] == 'd') {
				fd = 1;
			}
			int tix = std::stoi(line.substr(3), &next);
			int part = 0;
			if (line[next+4] == 'b') {
				part = 1;
			}
			
			double finish = std::stod(line.substr(next+5));
			if (part == 0) {
				if (fd == 0) {
					tasklist[tix].fm_a = finish;
				} else {
					tasklist[tix].fd_a = finish;
				}
			} else {
				if (fd == 0) {
					tasklist[tix].fm_b = finish;
				} else {
					tasklist[tix].fd_b = finish;
				}
			}
		}

	}

	// apply setup for merged parts
	for (unsigned int mix=0; mix<rResources.size(); mix++) {
		for (int rix=0; rix<schedule->num[mix]; rix++) {
			int tix = schedule->get(mix, rix)->taskid;
			int part = schedule->get(mix, rix)->part;

			
			CLogger::mainlog->debug("GeneticSolver: APPLY mix %d rix %d tix %d part %d ready rm %lf rd %lf finish fm %lf fd %lf",
			mix, rix, tix, part,
			(part == 0 ? tasklist[tix].rm_a : tasklist[tix].rm_b),
			(part == 0 ? tasklist[tix].rd_a : tasklist[tix].rd_b),
			(part == 0 ? tasklist[tix].fm_a : tasklist[tix].fm_b),
			(part == 0 ? tasklist[tix].fd_a : tasklist[tix].fd_b));

			if (tasklist[tix].merge == true) {
				if (part == 0) {
					schedule->get(mix, rix)->ratio = 1.0;
				} else {
					schedule->get(mix, rix)->ratio = 0.0;
				}
			}
		}
	}

	if (status > 1) {
		// invalid schedule
		schedule->fitness = max_double;
	}
	CLogger::mainlog->info("GeneticSolver: LPFile timestamp: %ld", timestamp);
	schedule->timestamp = timestamp;

}
