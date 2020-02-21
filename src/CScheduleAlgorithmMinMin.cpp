// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#include <limits>
#include <list>
#include <algorithm>
#include "CScheduleAlgorithmMinMin.h"
#include "CTask.h"
#include "CTaskCopy.h"
#include "CEstimation.h"
#include "CSchedule.h"
#include "CLogger.h"
using namespace sched::algorithm;
using sched::task::CTask;
using sched::schedule::STaskEntry;
using sched::schedule::CSchedule;



CScheduleAlgorithmMinMin::CScheduleAlgorithmMinMin(std::vector<CResource*>& rResources)
	: CScheduleAlgorithm(rResources)
{
	mpEstimation = CEstimation::getEstimation();
}

CScheduleAlgorithmMinMin::~CScheduleAlgorithmMinMin() {
	delete mpEstimation;
	mpEstimation = 0;
}

int CScheduleAlgorithmMinMin::init() {
	return 0;
}

void CScheduleAlgorithmMinMin::fini() {
}

CSchedule* CScheduleAlgorithmMinMin::compute(std::vector<CTaskCopy>* pTasks, std::vector<CTaskCopy*>* runningTasks, volatile int* interrupt, int updated) {

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
		CLogger::mainlog->debug("MinMin: new tix %d taskid %d", tix, task->mId);
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

			if (std::find(task->mpResources->begin(),
					task->mpResources->end(), res)
				== task->mpResources->end()) {
				// task is not compatible with resource	
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
		int min_tix = -1;
		int min_tix_mix = -1;
		double min_tix_comp = std::numeric_limits<double>::max();
		CTaskCopy* min_task = 0;

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

				CLogger::mainlog->debug("MinMin: tix %d mix %d completion time %lf", tix, mix, new_comp);

				if (first == 0 || new_comp < min_comp) {
					min_comp = new_comp;
					min_mix = mix;
					first = 1;
				}
			}
			if (min_comp < min_tix_comp) {
				min_tix = tix;
				min_tix_mix = min_mix;
				min_tix_comp = min_comp;
				min_task = task;
			}
		}

		CLogger::mainlog->debug("MinMin: select tix %d tid %d mix %d", min_tix, min_task->mId, min_tix_mix);
		// add task to machine queue
		STaskEntry* entry = new STaskEntry();
		entry->taskcopy = &((*pTasks)[min_tix]);
		entry->taskid = entry->taskcopy->mId;
		entry->stopProgress = entry->taskcopy->mCheckpoints;
		(*(sched->mpTasks))[min_tix_mix]->push_back(entry);

		// update ready time
		double new_ready = min_tix_comp;
		R[min_tix_mix] = new_ready;

		active[min_tix] = 1;

		unmappedTasks--;
		if (unmappedTasks == 0) {
			break;
		}

		CResource* min_res = mrResources[min_tix_mix];

		// update completion time matrix
		for (int tix = 0; tix<tasks; tix++) {
			if (active[tix] == 1) {
				continue;
			}
			CTaskCopy* task = &((*pTasks)[tix]);
			CLogger::mainlog->debug("MinMin: Check task %d", tix);
			bool succ = false;
			for (int six=0; six<min_task->mSuccessorNum; six++) {
				if (min_task->mpSuccessorList[six] == task->mId) {
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
					CLogger::mainlog->debug("MinMin: freed task %d", tix);

					for (int mix = 0; mix<machines; mix++) {
						CResource* res = mrResources[mix];

						if (std::find(task->mpResources->begin(),
								task->mpResources->end(), res)
							== task->mpResources->end()) {
							// task is not compatible with resource	
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

				if (task->validResource(min_res) == false) {
					continue;
				}

				double resourceReady = new_ready;
				
				double ready = (depReady[tix] > resourceReady ? depReady[tix] : resourceReady);
				double complete = 
					ready +
					mpEstimation->taskTimeInit(task, min_res) +
					mpEstimation->taskTimeCompute(task, min_res, task->mProgress, task->mCheckpoints) + 
					mpEstimation->taskTimeFini(task, min_res);
				C[tix*machines + min_tix_mix] = complete;

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
