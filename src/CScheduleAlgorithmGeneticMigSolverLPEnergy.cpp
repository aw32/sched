// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#include <cstdio>
#include <unistd.h>
#include <sys/wait.h>
#include <fstream>
#include <iostream>
#include <ostream>
#include "CScheduleAlgorithmGeneticMigSolverLPEnergy.h"
#include "CEstimation.h"
#include "CLogger.h"
#include "CConfig.h"
using namespace sched::algorithm;
using sched::task::CTask;
using sched::algorithm::CEstimation;

/*
New objective function in comparison to the definitions in the normal LP:

Minimize Objective Function
Static energy for all machines (Power * Makespan) +
Dynamic energy for all tasks
	min ( m * idle_1 + .. + m * idle_n +
		ce_1a * b_1a + de_1a * t_1a
	)

Constant energy per part
	ce_1a

Dynamic energy per part
	de_1a
*/
void CScheduleAlgorithmGeneticMigSolverLPEnergy::solve(CScheduleAlgorithmGeneticMig::CGeneticSchedule* schedule,
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

	std::ostringstream lpfile;

	std::ostringstream objective;

	lpfile << std::fixed;
	objective << std::fixed;

	objective << "min: ";

	// add static energy to objective
	for (unsigned int mix=0; mix<rResources.size(); mix++) {
		if (mix > 0) {
			objective << " + ";
		}
		objective << est->resourceIdlePower(rResources[mix]) << " * m";
	}

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

	std::vector<CScheduleAlgorithmGeneticMigSolverLPEnergy::Task> tasklist(pTasks->size());

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

			double initE = est->taskEnergyInit(task, res);
			double finiE = est->taskEnergyFini(task, res);
			double computeE = est->taskEnergyCompute(task, res, task->mProgress, task->mCheckpoints);


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

				// add dynamic energy to objective
				objective << " + " << (initE + finiE + computeE);

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

				// add dynamic energy to objective
				objective << " + " << (initE + finiE) << " * b_" <<
					tix << "_a + " << computeE << " * t_" << tix << "_a";

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

				// add dynamic energy to objective
				objective << " + " << (initE + finiE) << " * b_" <<
					tix << "_b + " << computeE << " * t_" << tix << "_b";

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


	objective << ";\n";

	CLogger::mainlog->debug("LP %s", objective.str().c_str());
	CLogger::mainlog->debug("LP stop");

	// create lp execution arguments
	std::string lpfil_str = lpfilename.str();
	std::string lpsol_str = lpsolutionname.str();

	// write lp to file
	std::ofstream lprealfile;
	lprealfile.open(lpfil_str);
	lprealfile << objective.str();
	lprealfile << lpfile.str();
	lprealfile.close();


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
		// Example: "Value of objective function: 20199.79796197"
		if (line[0] == 'V') {
			double o = std::stod(line.substr(28));
			CLogger::mainlog->debug("GeneticSolver: objective %lf\n", o);
			schedule->fitness = o;
		} else
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
