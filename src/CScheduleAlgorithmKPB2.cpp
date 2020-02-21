// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#include <limits>
#include <list>
#include <algorithm>
#include "CConfig.h"
#include "CScheduleAlgorithmKPB2.h"
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


CScheduleAlgorithmKPB2::CScheduleAlgorithmKPB2(std::vector<CResource*>& rResources)
	: CScheduleAlgorithm(rResources)
{
	mpEstimation = CEstimation::getEstimation();
}

CScheduleAlgorithmKPB2::~CScheduleAlgorithmKPB2() {
	delete mpEstimation;
	mpEstimation = 0;
}

int CScheduleAlgorithmKPB2::init() {

    CConfig* config = CConfig::getConfig();                               
    int ret = config->conf->getDouble((char*)"kpb_percentage", &mPercentage);
    if (-1 == ret) {
        CLogger::mainlog->error("ScheduleAlgorithmSA: kpb_percentage not found in config"); 
        return -1;
    }
    CLogger::mainlog->info("ScheduleAlgorithmSA: kpb_percentage: %f", mPercentage);
	CLogger::eventlog->info("\"event\":\"ALGORITHM_PARAM\",\"name\":\"kpb_percentage\",\"value\":%f", mPercentage);


	return 0;
}

void CScheduleAlgorithmKPB2::fini() {
}

CSchedule* CScheduleAlgorithmKPB2::compute(std::vector<CTaskCopy>* pTasks, std::vector<CTaskCopy*>* runningTasks, volatile int* interrupt, int updated) {

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

	// completion times
	double* pResourceCT = new double[machines]();
	// execution times
	double* pResourceET = new double[machines]();
	// machine index
	int* pResourceIX = new int[machines]();


	// map tasks
	for (unsigned int tix = 0; tix<todoTasks.size(); tix++) {
		CTaskCopy* task = todoTasks[tix];

		double depReady = sched->taskReadyTime(task);

		// fill arrays
		for (int mix = 0; mix<machines; mix++) {
			CResource* res = mrResources[mix];

			if (task->validResource(res) == false) {
				pResourceCT[mix] = std::numeric_limits<double>::max();
				pResourceET[mix] = std::numeric_limits<double>::max();
				pResourceIX[mix] = -1;
				continue;
			}

			double resourceReady = sched->resourceReadyTime(mix);
			double ready = (depReady > resourceReady ? depReady : resourceReady);
			double complete = 
				mpEstimation->taskTimeInit(task, res) +
				mpEstimation->taskTimeCompute(task, res, task->mProgress, task->mCheckpoints) + 
				mpEstimation->taskTimeFini(task, res);

			pResourceCT[mix] = ready + complete;
			pResourceET[mix] = complete;
			pResourceIX[mix] = mix;
		}
		// sort by execution times, lower execution times are at the front
		std::sort(
			pResourceIX,
			pResourceIX+machines,
			[pResourceET](int i, int j) {
				if (i == -1) {
					return false;
				}
				if (j == -1) {
					return true;
				}
				return pResourceET[i] > pResourceET[j];
	        }
		);
		// find entry with best completion time in km/100 best entries
		int best_num = (mPercentage/100.0)*machines;
		double selectedCT = 0.0;
		int selectedMix = 0;
		int first = 0;
		for (int lix=0; lix<=best_num; lix++) {

			int mix = pResourceIX[lix];

			if (mix == -1) {
				// resource not available
				continue;
			}

			if (first == 0 || pResourceCT[mix] < selectedCT) {
				selectedCT = pResourceCT[mix];
				selectedMix = mix;
				first = 1;
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
	}

	mpPreviousSchedule = sched;
	delete[] pResourceCT;
	delete[] pResourceET;
	delete[] pResourceIX;

	sched->computeTimes();

	return sched;
}
