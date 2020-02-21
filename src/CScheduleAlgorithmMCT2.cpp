// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#include <limits>
#include <list>
#include <algorithm>
#include "CScheduleAlgorithmMCT2.h"
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
using sched::task::ETaskState;


CScheduleAlgorithmMCT2::CScheduleAlgorithmMCT2(std::vector<CResource*>& rResources)
	: CScheduleAlgorithm(rResources)
{
	mpEstimation = CEstimation::getEstimation();
}

CScheduleAlgorithmMCT2::~CScheduleAlgorithmMCT2(){
	delete mpEstimation;
	mpEstimation = 0;
}

int CScheduleAlgorithmMCT2::init() {
	return 0;
}

void CScheduleAlgorithmMCT2::fini() {
}

CSchedule* CScheduleAlgorithmMCT2::compute(std::vector<CTaskCopy>* pTasks, std::vector<CTaskCopy*>* runningTasks, volatile int* interrupt, int updated) {

	int tasks = pTasks->size();
	int machines = mrResources.size();

	CScheduleExt* sched = new CScheduleExt(tasks, machines, mrResources, pTasks);

	// copy previous schedule
	if (mpPreviousSchedule != 0) {
		sched->copyEntries(mpPreviousSchedule, updated);
	}


	std::vector<CTaskCopy*> todoTasks;
	// get tasks that are not mapped
	for (unsigned int tix = 0; tix<pTasks->size(); tix++) {
		CTaskCopy* task = &((*pTasks)[tix]);
		if (mTaskMap.count(task->mId) == 0) {
			// task not in map
			todoTasks.push_back(task);
		}
	}

	std::vector<STaskEntry*> newEntries;

	// map tasks
	for (unsigned int tix = 0; tix<todoTasks.size(); tix++) {
		CTaskCopy* task = todoTasks[tix];

		double depReady = sched->taskReadyTime(task);

		double selectedComplete = 0.0;
		int selectedMix = -1;
		for (int mix = 0; mix<machines; mix++) {
			CResource* res = mrResources[mix];

			if (task->validResource(res) == false) {
				continue;
			}

			double resourceReady = sched->resourceReadyTime(mix);
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
			if (mix == 0 || complete < selectedComplete) {
				selectedMix = mix;
				selectedComplete = complete;
			}
			CLogger::mainlog->debug("MCT: opt taskid %d mix %d ready %lf complete %lf", task->mId, mix, ready, complete);
		}

		STaskEntry* entry = new STaskEntry();
		entry->taskcopy = task;
		entry->taskid = entry->taskcopy->mId;
		entry->stopProgress = entry->taskcopy->mCheckpoints;
		entry->timeFinish = std::chrono::duration<long long,std::nano>( (long long) (selectedComplete*1000000000.0));
		sched->addEntry(entry, mrResources[selectedMix], -1);
		newEntries.push_back(entry);

		mTaskMap[task->mId] = 1;
	}

	sched->mActiveTasks = tasks;
	mpPreviousSchedule = sched;

	sched->computeTimes();

	return sched;
}
