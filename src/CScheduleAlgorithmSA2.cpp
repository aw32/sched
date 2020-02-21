// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#include <limits>
#include <list>
#include <algorithm>
#include "CConfig.h"
#include "CScheduleAlgorithmSA2.h"
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


CScheduleAlgorithmSA2::CScheduleAlgorithmSA2(std::vector<CResource*>& rResources)
	: CScheduleAlgorithm(rResources)
{
	mpEstimation = CEstimation::getEstimation();
}

CScheduleAlgorithmSA2::~CScheduleAlgorithmSA2() {
	delete mpEstimation;
	mpEstimation = 0;
}

int CScheduleAlgorithmSA2::init() {

    CConfig* config = CConfig::getConfig();                               
    int ret = config->conf->getDouble((char*)"sa_ratio_lower", &mRatioLower);
    if (-1 == ret) {
        CLogger::mainlog->error("ScheduleAlgorithmSA: sa_ratio_lower not found in config"); 
        return -1;
    }
    CLogger::mainlog->info("ScheduleAlgorithmSA: sa_ratio_lower: %f", mRatioLower);
	CLogger::eventlog->info("\"event\":\"ALGORITHM_PARAM\",\"name\":\"sa_ratio_lower\",\"value\":%f", mRatioLower);

    ret = config->conf->getDouble((char*)"sa_ratio_higher", &mRatioHigher);
    if (-1 == ret) {
        CLogger::mainlog->error("ScheduleAlgorithmSA: sa_ratio_higher not found in config"); 
        return -1;
    }
    CLogger::mainlog->info("ScheduleAlgorithmSA: sa_ratio_higher: %f", mRatioHigher);
	CLogger::eventlog->info("\"event\":\"ALGORITHM_PARAM\",\"name\":\"sa_ratio_higher\",\"value\":%f", mRatioHigher);


	return 0;
}

void CScheduleAlgorithmSA2::fini() {
}

void CScheduleAlgorithmSA2::updateRatio(CScheduleExt* sched, int machines) {
	double min = 0.0;
	double max = 0.0;
	for (int mix=0; mix<machines; mix++) {
		double rReady = sched->resourceReadyTime(mix);
		if (mix == 0 || rReady<min) {
			min = rReady;
		}
		if (mix == 0 || rReady>max) {
			max = rReady;
		}
	}
	mRatio = min/max;
	if (mRatio >= mRatioHigher) {
		mMode = SA_MODE_MET;
	} else
	if (mRatio <= mRatioLower) {
		mMode = SA_MODE_MCT;
	}
}

CSchedule* CScheduleAlgorithmSA2::compute(std::vector<CTaskCopy>* pTasks, std::vector<CTaskCopy*>* runningTasks, volatile int* interrupt, int updated) {

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
		std::unordered_map<int,int>::const_iterator ix_it = mTaskMap.find(task->mId);
		if (mTaskMap.end() == ix_it) {
			// task not in map
			todoTasks.push_back(task);
		}
	}


	std::vector<STaskEntry*> newEntries;

	// update ratio
	updateRatio(sched, machines);

	// map tasks
	for (unsigned int tix = 0; tix<todoTasks.size(); tix++) {
		CTaskCopy* task = todoTasks[tix];

		double depReady = sched->taskReadyTime(task);

		double selectedComplete = 0.0;
		double selectedExec = 0.0;
		int selectedMix = -1;
		for (int mix = 0; mix<machines; mix++) {
			CResource* res = mrResources[mix];

			if (task->validResource(res) == false) {
				continue;
			}

			double resourceReady = sched->resourceReadyTime(res);
			double ready = (depReady > resourceReady ? depReady : resourceReady);
			// completion time =
			// ready time +
			// init time +
			// compute time +
			// fini time
			double exec =
				mpEstimation->taskTimeInit(task, res) +
				mpEstimation->taskTimeCompute(task, res, task->mProgress, task->mCheckpoints) + 
				mpEstimation->taskTimeFini(task, res);
			double complete = 
				ready + exec;

			if (mix == 0 || 
					(mMode == SA_MODE_MCT && complete < selectedComplete) ||
					(mMode == SA_MODE_MET && exec < selectedExec)
				) {
				selectedMix = mix;
				selectedComplete = complete;
				selectedExec = exec;
			}
		}


		STaskEntry* entry = new STaskEntry();
		entry->taskcopy = task;
		entry->taskid = entry->taskcopy->mId;
		entry->startProgress = entry->taskcopy->mProgress;
		entry->stopProgress = entry->taskcopy->mCheckpoints;
		sched->computeExecutionTime(entry, mrResources[selectedMix]);
		sched->addEntry(entry, mrResources[selectedMix], -1);
		newEntries.push_back(entry);

		mTaskMap[task->mId] = 1;
		updateRatio(sched, machines);
	}

	mpPreviousSchedule = sched;

	sched->computeTimes();

	return sched;
}
