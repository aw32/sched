// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#include <limits>
#include <list>
#include <algorithm>
#include "CScheduleAlgorithmMaxMin2Dyn.h"
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



CScheduleAlgorithmMaxMin2Dyn::CScheduleAlgorithmMaxMin2Dyn(std::vector<CResource*>& rResources)
	: CScheduleAlgorithm(rResources)
{
	mpEstimation = CEstimation::getEstimation();
}

CScheduleAlgorithmMaxMin2Dyn::~CScheduleAlgorithmMaxMin2Dyn() {
	delete mpEstimation;
	mpEstimation = 0;
}

int CScheduleAlgorithmMaxMin2Dyn::init() {
	return 0;
}

void CScheduleAlgorithmMaxMin2Dyn::fini() {
}

CSchedule* CScheduleAlgorithmMaxMin2Dyn::compute(std::vector<CTaskCopy>* pTasks, std::vector<CTaskCopy*>* runningTasks, volatile int* interrupt, int updated) {

	int tasks = pTasks->size();
	int machines = mrResources.size();

	CScheduleExt* sched = new CScheduleExt(tasks, machines, mrResources, pTasks);
	sched->setRunningTasks(runningTasks);


	// completion time matrix
	double* C = new double[tasks * machines]();

	
	// fill matrix
	for (int tix = 0; tix<tasks; tix++) {
		if (sched->taskDependencySatisfied(tix) == false) {
			continue;
		}
		CTask* task = (CTask*)  &((*pTasks)[tix]);
		for (int mix = 0; mix<machines; mix++) {
			CResource* res = mrResources[mix];

			if (task->validResource(res) == false) {
				continue;
			}

			// ready time for this task on this resource considering running tasks
			double ready = sched->taskReadyTimeResource(tix, res, mpEstimation);
			double init = mpEstimation->taskTimeInit(task, res);
			if (sched->taskRunningResource(tix) == mix) {
				// is task already running on resource
				// ignore init time for first slot on resource
				init = 0.0;
			}
			double compute = mpEstimation->taskTimeCompute(task, res, task->mProgress, task->mCheckpoints);
			double fini = mpEstimation->taskTimeFini(task, res);
			double complete = ready + init + compute + fini;
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
			if (sched->taskLastPartMapped(tix) == true) {
				continue;
			}
			// skip tasks whose predecessors are not ready
			if (sched->taskDependencySatisfied(tix) == false) {
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
		entry->startProgress = entry->taskcopy->mProgress;
		entry->stopProgress = entry->taskcopy->mCheckpoints;
		sched->addEntry(entry, mrResources[max_tix_mix], -1);

		unmappedTasks--;
		if (unmappedTasks == 0) {
			break;
		}

		CResource* max_res = mrResources[max_tix_mix];

		// update completion time matrix
		for (int tix = 0; tix<tasks; tix++) {
			if (sched->taskLastPartMapped(tix) == true) {
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
				if (sched->taskDependencySatisfied(tix) == true) {
					// task was just freed from dependencies
					// update ready times
					CLogger::mainlog->debug("MaxMin: freed task %d", tix);

					for (int mix = 0; mix<machines; mix++) {
						CResource* res = mrResources[mix];

						if (task->validResource(res) == false) {
							continue;
						}

						// ready time for this task on this resource considering running tasks
						double ready = sched->taskReadyTimeResource(tix, res, mpEstimation);
						double init = mpEstimation->taskTimeInit(task, res);
						if (sched->taskRunningResource(tix) == mix &&
							sched->resourceTasks(mix) == 0) {
							// task already running on resource and
							// slot is first slot
							init = 0.0;
						}

						double compute = mpEstimation->taskTimeCompute(task, res, task->mProgress, task->mCheckpoints);
						double fini = mpEstimation->taskTimeFini(task, res);
						double complete = ready + init + compute + fini;
						C[tix*machines + mix] = complete;
					}


				}
			} else {

				if (sched->taskDependencySatisfied(tix) == false) {
					continue;
				}

				if (task->validResource(max_res) == false) {
					continue;
				}

				// ready time for this task on this resource considering running tasks
				double ready = sched->taskReadyTimeResource(tix, max_res, mpEstimation);
				double init = mpEstimation->taskTimeInit(task, max_res);
				if (sched->taskRunningResource(tix) == max_tix_mix &&
					sched->resourceTasks(max_tix_mix) == 0) {
					// task already running on resource and
					// slot is first slot
					init = 0.0;
				}

				double compute = mpEstimation->taskTimeCompute(task, max_res, task->mProgress, task->mCheckpoints);
				double fini = mpEstimation->taskTimeFini(task, max_res);
				double complete = ready + init + compute + fini;
				C[tix*machines + max_tix_mix] = complete;

			}
		}
	}

	sched->computeTimes();

	// clean up
	delete[] C;

	return sched;
}
