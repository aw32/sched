// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#include <limits>
#include <list>
#include <algorithm>
#include <unordered_map>
#include "CScheduleAlgorithmHEFT2Dyn.h"
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



CScheduleAlgorithmHEFT2Dyn::CScheduleAlgorithmHEFT2Dyn(std::vector<CResource*>& rResources)
	: CScheduleAlgorithm(rResources)
{
	mpEstimation = CEstimation::getEstimation();
}

CScheduleAlgorithmHEFT2Dyn::~CScheduleAlgorithmHEFT2Dyn() {
	delete mpEstimation;
	mpEstimation = 0;
}

int CScheduleAlgorithmHEFT2Dyn::init() {
	return 0;
}

void CScheduleAlgorithmHEFT2Dyn::fini() {
}

CSchedule* CScheduleAlgorithmHEFT2Dyn::compute(std::vector<CTaskCopy>* pTasks, std::vector<CTaskCopy*>* runningTasks, volatile int* interrupt, int updated) {

	int tasks = pTasks->size();
	int machines = mrResources.size();

	CScheduleExt* sched = new CScheduleExt(tasks, machines, mrResources, pTasks);
	sched->setRunningTasks(runningTasks);

	// average execution costs
	double* w = new double[tasks]();
	for (int tix = 0; tix < tasks; tix++) {
		double tix_w = 0.0;
		CTask* task = &(*pTasks)[tix];
		int validmachines = 0;
		for (int mix = 0; mix < machines; mix++) {
			CResource* res = mrResources[mix];
			if (task->validResource(res) == false) {
				continue;
			}
			tix_w += mpEstimation->taskTimeInit(task, res) +
					 mpEstimation->taskTimeCompute(task, res, task->mProgress, task->mCheckpoints) +
					 mpEstimation->taskTimeFini(task, res);
			validmachines++;
		}
		w[tix] = tix_w / validmachines;
	}

	// upward ranks
	double* upward = new double[tasks]();
	int changed = 0;
	do {
		changed = 0;
		for (int tix = 0; tix < tasks; tix++) {
			if (upward[tix] != 0) {
				continue;
			}
			CTask* task = &(*pTasks)[tix];
			// check successors
			int unmarked = 0;
			double max_succ_rank = 0.0;
			for (int succIx = 0; succIx < task->mSuccessorNum; succIx++) {
				int succId = task->mpSuccessorList[succIx];
				// find local task index
				int ix = -1;
				if (sched->mTaskMap.count(succId) == 0) {
					continue;
				}
				ix = sched->mTaskMap[succId];

				CLogger::mainlog->debug("HEFT tix %d succ %d", tix, ix);
				if (upward[ix] == 0.0) {
					unmarked++;
					break;
				}
				if (upward[ix] > max_succ_rank) {
					max_succ_rank = upward[ix];
				}
			}
			if (unmarked > 0) {
				continue;
			}
			// compute upward rank
			CLogger::mainlog->debug("HEFT compute upward rank for task %d w %f max_succ_rank %f succ_num %d", tix, w[tix], max_succ_rank, task->mSuccessorNum);
			upward[tix] = w[tix] + max_succ_rank;
			changed++;
		}
		
	} while(changed > 0);

	std::list<int> priorityListIx;
	for (int tix = 0; tix < tasks; tix++) {
		priorityListIx.push_back(tix);
	}

	// sort by decreasing upward rank
	priorityListIx.sort( 
		[upward](int i, int j) {
			return upward[i] > upward[j];
		}
	);


	int listTasks = tasks;
	// for all elements in priority list
	while (listTasks > 0) {

		int tix = priorityListIx.front();
		priorityListIx.pop_front();
		CTaskCopy* task = &(*pTasks)[tix];
		listTasks--;

		// get finish time of predecessor tasks
		double preFinish = 0.0;
		preFinish = sched->taskReadyTime(task);
		CLogger::mainlog->debug("HEFT: tid %d preFinish %lf", task->mId, preFinish);

		// find slot with earliest finishing time over all machines
		int eft_mix = -1;
		double finish = 0.0;
		int eft_mix_slot_index = 0;
		// iterate over every machine
		for (int mix = 0; mix < machines; mix++) {
			CResource* res = mrResources[mix];

			if (task->validResource(res) == false) {
				// task is not compatible with resource 
				continue;
			}

			double preFinish = sched->taskReadyTimeResource(tix, res, mpEstimation);
			double init = mpEstimation->taskTimeInit(task, res);
			double compute = mpEstimation->taskTimeCompute(task, res, task->mProgress, task->mCheckpoints);
			double fini = mpEstimation->taskTimeFini(task, res);
			double dur = init + compute + fini;

			// find first slot on this machine that
			// * is large enough for this task
			// * has a start time that is after the predecessor finishing time

			double slot_start = 0.0;
			double slot_stop = 0.0;
			int slot_index = sched->findSlot(mix, dur, preFinish, 0, &slot_start, &slot_stop);
			// check if task is already running and fits into first slot
			if (sched->taskRunningResource(tix) == mix &&
						sched->resourceTasks(mix) == 0) {
				double first_slot_start = 0.0;
				double first_slot_stop = 0.0;
				int first_slot_index = sched->findSlot(mix, compute + fini, preFinish, 0, &first_slot_start, &first_slot_stop);
				if (first_slot_index == 0) {
					// task actually fits into first slot, correct values
					slot_start = first_slot_start;
					slot_stop = first_slot_stop;
					slot_index = first_slot_index;
				}
			}

			CLogger::mainlog->debug("HEFT: tid %d mix %d stop %lf", task->mId, mix, slot_stop);

			// use found slot if it is better or the first machine
			if (eft_mix == -1 || slot_stop < finish) {
				eft_mix = mix;
				finish = slot_stop;
				eft_mix_slot_index = slot_index;
			}
		}

		STaskEntry* entry = new STaskEntry();
		entry->taskcopy = task;
		entry->taskid = entry->taskcopy->mId;
		entry->startProgress = entry->taskcopy->mProgress;
		entry->stopProgress = entry->taskcopy->mCheckpoints;
		sched->addEntry(entry, mrResources[eft_mix], eft_mix_slot_index);
		CLogger::mainlog->debug("HEFT: add tid %d to mix %d slot_index %d", task->mId, eft_mix, eft_mix_slot_index);


	}

	

	//clean up
	delete[] w;
	delete[] upward;

	sched->computeTimes();

	return sched;
}
