// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#include <limits>
#include <list>
#include <algorithm>
#include "CScheduleAlgorithmSufferage.h"
#include "CTask.h"
#include "CTaskCopy.h"
#include "CEstimation.h"
#include "CSchedule.h"
#include "CLogger.h"
using namespace sched::algorithm;
using sched::task::CTask;
using sched::schedule::STaskEntry;
using sched::schedule::CSchedule;



CScheduleAlgorithmSufferage::CScheduleAlgorithmSufferage(std::vector<CResource*>& rResources)
	: CScheduleAlgorithm(rResources)
{
	mpEstimation = CEstimation::getEstimation();
}

CScheduleAlgorithmSufferage::~CScheduleAlgorithmSufferage() {
	delete mpEstimation;
	mpEstimation = 0;
}

int CScheduleAlgorithmSufferage::init() {
	return 0;
}

void CScheduleAlgorithmSufferage::fini() {
}

CSchedule* CScheduleAlgorithmSufferage::compute(std::vector<CTaskCopy>* pTasks, std::vector<CTaskCopy*>* runningTasks, volatile int* interrupt, int updated) {

	int tasks = pTasks->size();
	int machines = mrResources.size();

	if (machines < 2) {
		CLogger::mainlog->error("Sufferage: less than 2 resources !!!");
	}

	CSchedule* sched = new CSchedule(tasks, machines, mrResources);
	sched->mpOTasks = pTasks;

	// completion time matrix
	double* C = new double[tasks * machines]();
	// list of unmapped tasks
	// 0 -> unmapped
	// 1 -> mapped
	int* activeTasks = new int[tasks]();
	// machine ready times
	double* R = new double[machines]();
	// array for chosen machine in first pass
	//int* chosenMachine = new int[tasks];
	int* activeMachines = new int[machines]();
	double* sufferage = new double[machines]();

	int dep[tasks];
	double depReady[tasks];
	for (int tix = 0; tix<tasks; tix++) {
		CTaskCopy* task = &((*pTasks)[tix]);
		CLogger::mainlog->debug("Sufferage: new tix %d taskid %d", tix, task->mId);
		depReady[tix] = 0.0;
		if (task->mPredecessorNum == 0) {
			dep[tix] = 0;
		} else {
			int found = 0;
			for (int pix=0; pix<task->mPredecessorNum; pix++) {
				int pid = task->mpPredecessorList[pix];
				// check if predecessor is in tasks list
				for (unsigned int qix=0; qix<pTasks->size(); qix++) {
					if ((*pTasks)[qix].mId == pid) {
						found++;
						break;
					}
				}
			}
			dep[tix] = found;
		}
	}
	
	// fill matrix
	for (int tix = 0; tix<tasks; tix++) {
		if (dep[tix] > 0) {
			continue;
		}
		CTask* task = (CTask*)  &((*pTasks)[tix]);
		for (int mix = 0; mix<machines; mix++) {
			CResource* res = mrResources[mix];

			if (task->validResource(res) == false) {
				continue;
			}

			// completion time =
			// ready time +
			// init time +
			// compute time +
			// fini time
			double complete = 
				R[mix] +
				mpEstimation->taskTimeInit(task, res) +
				mpEstimation->taskTimeCompute(task, res, task->mProgress, task->mCheckpoints) + 
				mpEstimation->taskTimeFini(task, res);
			C[tix*machines + mix] = complete;
		}
	}

	int unmappedTasks = tasks;

	while (unmappedTasks > 0) {

		// reset marked machines
		for (int mix = 0; mix < machines; mix++) {
			activeMachines[mix] = -1;
		}

		for (int tix = 0; tix<tasks; tix++) {
			// skip already mapped tasks
			if (activeTasks[tix] == 1) {
				continue;
			}
			// skip tasks whose predecessors are not ready
			if (dep[tix] > 0) {
				continue;
			}
			CTaskCopy* task = &((*pTasks)[tix]);
			// machine with earliest completion time
			int min_mix = 0;
			double min_comp = C[tix*machines];
			// machine with second earliest completion time
			double min_comp_2 = 0;
			int first = 0;
			int second = 0;

			for (int mix = 0; mix<machines; mix++) {

				CResource* res = mrResources[mix];

				if (task->validResource(res) == false) {
					continue;
				}

				double new_comp = C[tix*machines + mix];
				if (first == 0 || new_comp < min_comp) {
					min_comp_2 = min_comp;
					min_comp = new_comp;
					min_mix = mix;
					first = 1;
				} else
				if (second == 0 || new_comp < min_comp_2){
					min_comp_2 = new_comp;
					second = 1;
				}
			}
			double tix_sufferage = min_comp_2 - min_comp;
			if (machines<2) {
				tix_sufferage = std::numeric_limits<double>::max() - min_comp;
			}
			if (activeMachines[min_mix] == -1) {
				activeMachines[min_mix] = tix;
				sufferage[min_mix] = tix_sufferage;
			} else {
				if (sufferage[min_mix] < tix_sufferage) {
					activeMachines[min_mix] = tix;
					sufferage[min_mix] = tix_sufferage;
				}
			}
		}

		// assign tasks for all marked machines
		for (int mix = 0; mix < machines; mix++) {

			CResource* res = mrResources[mix];

			if (activeMachines[mix] == -1) {
				continue;
			}
			int tix = activeMachines[mix];
			double comp = C[tix*machines + mix];
			CTaskCopy* mapped_task = &((*pTasks)[tix]);
			CLogger::mainlog->debug("Sufferage: tix %d tid %d mix %d", tix, mapped_task->mId, mix);

			// add task to machine queue
			STaskEntry* entry = new STaskEntry();
			entry->taskcopy = &((*pTasks)[tix]);
			entry->taskid = entry->taskcopy->mId;
			entry->stopProgress = entry->taskcopy->mCheckpoints;
			(*(sched->mpTasks))[mix]->push_back(entry);

			// update ready time
			double new_ready = comp;
			R[mix] = new_ready;

			activeTasks[tix] = 1;

			unmappedTasks--;

			if (unmappedTasks == 0) {
				break;
			}

			// update completion time matrix
			for (int tix = 0; tix<tasks; tix++) {
				if (activeTasks[tix] == 1) {
					continue;
				}
				CTaskCopy* task = &((*pTasks)[tix]);
				CLogger::mainlog->debug("Sufferage: Check task %d", tix);
				if (dep[tix] > 0) {
					bool succ = false;
					for (int six=0; six<mapped_task->mSuccessorNum; six++) {
						if (mapped_task->mpSuccessorList[six] == task->mId) {
							// successor task found
							succ = true;
							break;
						}
					}
					if (succ == true) {
						dep[tix]--;
						if (new_ready > depReady[tix]) {
							depReady[tix] = new_ready;
						}
						if (dep[tix] == 0) {
							// task was just freed from dependencies
							// update ready times
							CLogger::mainlog->debug("Sufferage: freed task %d", tix);

							for (int mix = 0; mix<machines; mix++) {
								CResource* res = mrResources[mix];

								if (task->validResource(res) == false) {
									continue;
								}

								double resourceReady = R[mix];
								double ready = (depReady[tix] > resourceReady ? depReady[tix] : resourceReady);

								// completion time =
								// ready time +
								// init time +
								// compute time +
								// fini time
								double complete = 
									ready +
									mpEstimation->taskTimeInit(task, res) +
									mpEstimation->taskTimeCompute(task, res, task->mProgress, task->mCheckpoints) + 
									mpEstimation->taskTimeFini(task, res);
								C[tix*machines + mix] = complete;
							}


						}
					}
				} else {

					double resourceReady = new_ready;
					
					double ready = (depReady[tix] > resourceReady ? depReady[tix] : resourceReady);
					double complete = 
						ready +
						mpEstimation->taskTimeInit(task, res) +
						mpEstimation->taskTimeCompute(task, res, task->mProgress, task->mCheckpoints) + 
						mpEstimation->taskTimeFini(task, res);
					C[tix*machines + mix] = complete;

				}


			}
		}
	}

	sched->mActiveTasks = tasks;
	sched->computeTimes();

	// clean up
	delete[] C;
	delete[] activeTasks;
	delete[] R;
	delete[] activeMachines;
	delete[] sufferage;

	return sched;
}
