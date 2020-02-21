// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#include <limits>
#include <list>
#include <algorithm>
#include "CScheduleAlgorithmMaxMin.h"
#include "CTask.h"
#include "CTaskCopy.h"
#include "CEstimation.h"
#include "CSchedule.h"
#include "CLogger.h"
using namespace sched::algorithm;
using sched::task::CTask;
using sched::schedule::STaskEntry;
using sched::schedule::CSchedule;



CScheduleAlgorithmMaxMin::CScheduleAlgorithmMaxMin(std::vector<CResource*>& rResources)
	: CScheduleAlgorithm(rResources)
{
	mpEstimation = CEstimation::getEstimation();
}

CScheduleAlgorithmMaxMin::~CScheduleAlgorithmMaxMin() {
	delete mpEstimation;
	mpEstimation = 0;
}

int CScheduleAlgorithmMaxMin::init() {
	return 0;
}

void CScheduleAlgorithmMaxMin::fini() {
}

CSchedule* CScheduleAlgorithmMaxMin::compute(std::vector<CTaskCopy>* pTasks, std::vector<CTaskCopy*>* runningTasks, volatile int* interrupt, int updated) {

	int tasks = pTasks->size();
	int machines = mrResources.size();

	CSchedule* sched = new CSchedule(tasks, machines, mrResources);
	sched->mpOTasks = pTasks;


	// completion time matrix
	double* C = new double[tasks * machines]();
	// list of unmapped tasks
	// 0 -> unmapped
	// 1 -> mapped
	int* active = new int[tasks]();
	// machine ready times
	double* R = new double[machines]();

	int dep[tasks];
	double depReady[tasks];
	for (int tix = 0; tix<tasks; tix++) {
		CTaskCopy* task = &((*pTasks)[tix]);
		CLogger::mainlog->debug("MaxMin: new tix %d taskid %d", tix, task->mId);
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

		// task with earliest completion time
		int max_tix = -1;
		int max_tix_mix = -1;
		double max_tix_comp = 0.0;
		CTaskCopy* max_task = 0;

		for (int tix = 0; tix<tasks; tix++) {
			// skip already mapped tasks
			if (active[tix] == 1) {
				continue;
			}
			// skip tasks whose predecessors are not ready
			if (dep[tix] > 0) {
				continue;
			}
			CTaskCopy* task = &((*pTasks)[tix]);
			// machine with earliest completion time
			int min_mix = 0;
			double min_comp = 0.0;
			int first = 0;

			for (int mix = 0; mix<machines; mix++) {

				CResource* res = mrResources[mix];

				if (task->validResource(res) == false) {
					continue;
				}

				double new_comp = C[tix*machines + mix];

				CLogger::mainlog->debug("MaxMin: tix %d mix %d completion time %lf", tix, mix, new_comp);

				if (first == 0 || new_comp < min_comp) {
					min_comp = new_comp;
					min_mix = mix;
					first = 1;
				}
			}
			if (min_comp > max_tix_comp) {
				max_tix = tix;
				max_tix_mix = min_mix;
				max_tix_comp = min_comp;
				max_task = task;
			}
		}

		CLogger::mainlog->debug("MaxMin: tix %d tid %d mix %d", max_tix, max_task->mId, max_tix_mix);
		// add task to machine queue
		STaskEntry* entry = new STaskEntry();
		entry->taskcopy = &((*pTasks)[max_tix]);
		entry->taskid = entry->taskcopy->mId;
		entry->stopProgress = entry->taskcopy->mCheckpoints;
		(*(sched->mpTasks))[max_tix_mix]->push_back(entry);

		// update ready time
		double new_ready = max_tix_comp;
		R[max_tix_mix] = new_ready;

		active[max_tix] = 1;

		unmappedTasks--;
		if (unmappedTasks == 0) {
			break;
		}

		CResource* max_res = mrResources[max_tix_mix];

		// update completion time matrix
		for (int tix = 0; tix<tasks; tix++) {
			if (active[tix] == 1) {
				continue;
			}
			CTaskCopy* task = &((*pTasks)[tix]);
			CLogger::mainlog->debug("MaxMin: Check task %d", tix);
			bool succ = false;
			for (int six=0; six<max_task->mSuccessorNum; six++) {
				if (max_task->mpSuccessorList[six] == task->mId) {
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
					CLogger::mainlog->debug("MaxMin: freed task %d", tix);

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
			} else {

				if (dep[tix] > 0) {
					continue;
				}

				if (task->validResource(max_res) == false) {
					continue;
				}

				double resourceReady = new_ready;
				
				double ready = (depReady[tix] > resourceReady ? depReady[tix] : resourceReady);
				double complete = 
					ready +
					mpEstimation->taskTimeInit(task, max_res) +
					mpEstimation->taskTimeCompute(task, max_res, task->mProgress, task->mCheckpoints) + 
					mpEstimation->taskTimeFini(task, max_res);
				C[tix*machines + max_tix_mix] = complete;

			}
		}
	}

	sched->mActiveTasks = tasks;
	sched->computeTimes();

	// clean up
	delete[] C;
	delete[] active;
	delete[] R;

	return sched;
}
