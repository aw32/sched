// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#include <limits>
#include <list>
#include <algorithm>
#include "CScheduleAlgorithmMET.h"
#include "CTask.h"
#include "CTaskCopy.h"
#include "CEstimation.h"
#include "CSchedule.h"
#include "CLogger.h"
using namespace sched::algorithm;
using sched::task::CTask;
using sched::schedule::STaskEntry;
using sched::schedule::CSchedule;
using sched::task::ETaskState;


CScheduleAlgorithmMET::CScheduleAlgorithmMET(std::vector<CResource*>& rResources)
	: CScheduleAlgorithm(rResources)
{
	mpEstimation = CEstimation::getEstimation();
}

CScheduleAlgorithmMET::~CScheduleAlgorithmMET() {
	delete mpEstimation;
	mpEstimation = 0;
}

int CScheduleAlgorithmMET::init() {
	return 0;
}

void CScheduleAlgorithmMET::fini() {
}

CSchedule* CScheduleAlgorithmMET::compute(std::vector<CTaskCopy>* pTasks, std::vector<CTaskCopy*>* runningTasks, volatile int* interrupt, int updated) {

	int tasks = pTasks->size();
	int machines = mrResources.size();

	CSchedule* sched = new CSchedule(tasks, machines, mrResources);
	sched->mpOTasks = pTasks;

	// generate map for global id to local id conversion
	std::unordered_map<int,int> taskmap;
	for (unsigned int i=0; i<pTasks->size(); i++) {
		taskmap[(*pTasks)[i].mId] = i;
	}

	// copy previous schedule
	if (mpPreviousSchedule != 0) {
		for (int mix=0; mix<machines; mix++) {
			std::vector<STaskEntry*>* mList = (*mpPreviousSchedule->mpTasks)[mix];

			for (unsigned int tix=0; tix<mList->size(); tix++) {
				STaskEntry* entry = (*mList)[tix];
				std::unordered_map<int,int>::const_iterator ix_it = taskmap.find(entry->taskid);
				if (taskmap.end() == ix_it) {
					// task already done
					continue;
				}
				int taskix = ix_it->second;

				CTaskCopy* task = &((*pTasks)[taskix]);

				STaskEntry* newentry = new STaskEntry();
				newentry->taskcopy = task;
				newentry->taskid = newentry->taskcopy->mId;
				newentry->stopProgress = newentry->taskcopy->mCheckpoints;
				(*(sched->mpTasks))[mix]->push_back(newentry);
			}
		}
	}


	std::vector<CTaskCopy*> todoTasks;
	// get tasks that are not mapped
	for (unsigned int tix = 0; tix<pTasks->size(); tix++) {
		CTaskCopy* task = &((*pTasks)[tix]);
		std::unordered_map<int,int>::const_iterator ix_it = mTaskMap.find(task->mId);
		if (mTaskMap.end() == ix_it) {
			// task not in map
			todoTasks.push_back(task);
		}
	}

	// map tasks
	for (unsigned int tix = 0; tix<todoTasks.size(); tix++) {
		CTaskCopy* task = todoTasks[tix];
		double selectedComplete = 0.0;
		int selectedMix = -1;
		// select machine
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
				mpEstimation->taskTimeInit(task, res) +
				mpEstimation->taskTimeCompute(task, res, task->mProgress, task->mCheckpoints) + 
				mpEstimation->taskTimeFini(task, res);
			if (mix == 0 || complete < selectedComplete) {
				selectedMix = mix;
				selectedComplete = complete;
			}
		}

		STaskEntry* entry = new STaskEntry();
		entry->taskcopy = task;
		entry->taskid = entry->taskcopy->mId;
		entry->stopProgress = entry->taskcopy->mCheckpoints;
		(*(sched->mpTasks))[selectedMix]->push_back(entry);

		mTaskMap[task->mId] = 1;
	}

	sched->mActiveTasks = tasks;
	mpPreviousSchedule = sched;

	sched->computeTimes();

	return sched;
}
