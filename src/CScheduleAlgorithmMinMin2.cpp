// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#include <limits>
#include <list>
#include <algorithm>
#include "CScheduleAlgorithmMinMin2.h"
#include "CTask.h"
#include "CTaskCopy.h"
#include "CEstimation.h"
#include "CSchedule.h"
#include "CScheduleExt.h"
#include "CLogger.h"
using namespace sched::algorithm;
using sched::task::CTask;
using sched::schedule::STaskEntry;
using sched::schedule::CSchedule;
using sched::schedule::CScheduleExt;



CScheduleAlgorithmMinMin2::CScheduleAlgorithmMinMin2(std::vector<CResource*>& rResources)
	: CScheduleAlgorithm(rResources)
{
	mpEstimation = CEstimation::getEstimation();
}

CScheduleAlgorithmMinMin2::~CScheduleAlgorithmMinMin2() {
	delete mpEstimation;
	mpEstimation = 0;
}

int CScheduleAlgorithmMinMin2::init() {
	return 0;
}

void CScheduleAlgorithmMinMin2::fini() {
}

CSchedule* CScheduleAlgorithmMinMin2::compute(std::vector<CTaskCopy>* pTasks, std::vector<CTaskCopy*>* runningTasks, volatile int* interrupt, int updated) {

	int tasks = pTasks->size();
	int machines = mrResources.size();

	CScheduleExt* sched = new CScheduleExt(tasks, machines, mrResources, pTasks);


	// completion time matrix
	double* C = new double[tasks * machines]();
	// list of unmapped tasks
	// 0 -> unmapped
	// 1 -> mapped
	//int* active = new int[tasks]();
/*
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
				for (int qix=0; qix<pTasks->size(); qix++) {
					if ((*pTasks)[qix].mId == pid) {
						found++;
						break;
					}
				}
			}
			dep[tix] = found;
		}
	}
*/


	
	// fill matrix
	for (int tix = 0; tix<tasks; tix++) {
		if (sched->taskDependencySatisfied(tix) == false) {
			continue;
		}
		CTask* task = (CTask*)  &((*pTasks)[tix]);
		for (int mix = 0; mix<machines; mix++) {
			CResource* res = mrResources[mix];

			if (task->validResource(res) == false) {
				// task is not compatible with resource	
				continue;
			}

			double resReady = sched->resourceReadyTime(res);

			// completion time =
			// ready time +
			// init time +
			// compute time +
			// fini time
			double complete = 
				resReady +
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
			if (sched->taskLastPartMapped(tix) == true) {
				continue;
			}
			// skip tasks whose predecessors are not ready
			if (sched->taskDependencySatisfied(tix) == false) {
				//CLogger::mainlog->debug("MinMin: tix %d deps!", tix);
				continue;
			}
			CTaskCopy* task = &((*pTasks)[tix]);
			// machine with earliest completion time
			int min_mix = 0;
			double min_comp = 0.0;
			int first = 0;

			//CLogger::mainlog->debug("MinMin: tix %d ", tix);
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
		entry->startProgress = entry->taskcopy->mProgress;
		entry->stopProgress = entry->taskcopy->mCheckpoints;
		sched->computeExecutionTime(entry, mrResources[min_tix_mix]);
		sched->addEntry(entry, mrResources[min_tix_mix], -1);

		// update ready time
		double new_ready = min_tix_comp;
		CLogger::mainlog->debug("MinMin: comp time %lf new res ready time %lf", new_ready, sched->resourceReadyTime(min_tix_mix));

		unmappedTasks--;
		if (unmappedTasks == 0) {
			break;
		}

		CResource* min_res = mrResources[min_tix_mix];

		// update completion time matrix
		for (int tix = 0; tix<tasks; tix++) {
			if (sched->taskLastPartMapped(tix) == true) {
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
				
				CLogger::mainlog->debug("MinMin: task %d was in succ list", tix);
				if (sched->taskDependencySatisfied(tix) == true) {
					CLogger::mainlog->debug("MinMin: task %d was freed", tix);
					// task was just freed from dependencies
					// update ready times
					CLogger::mainlog->debug("MinMin: freed task %d", tix);

					for (int mix = 0; mix<machines; mix++) {
						CResource* res = mrResources[mix];

						if (task->validResource(res) == false) {
							// task is not compatible with resource	
							continue;
						}

						double resourceReady = sched->resourceReadyTime(mix);
						double depReady = sched->taskReadyTime(tix);
						double ready = (depReady > resourceReady ? depReady : resourceReady);

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

				if (sched->taskDependencySatisfied(task) == false) {
					// task is not ready yet
					continue;
				}

				if (task->validResource(min_res) == false) {
					continue;
				}

				double resourceReady = sched->resourceReadyTime(min_tix_mix);
				double depReady = sched->taskReadyTime(tix);
				double ready = (depReady > resourceReady ? depReady : resourceReady);
				double complete = 
					ready +
					mpEstimation->taskTimeInit(task, min_res) +
					mpEstimation->taskTimeCompute(task, min_res, task->mProgress, task->mCheckpoints) + 
					mpEstimation->taskTimeFini(task, min_res);
				C[tix*machines + min_tix_mix] = complete;

			}
		}
	}

	sched->computeTimes();

	// clean up
	delete[] C;

	return sched;
}
