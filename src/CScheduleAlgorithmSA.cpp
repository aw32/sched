// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#include <limits>
#include <list>
#include <algorithm>
#include "CConfig.h"
#include "CScheduleAlgorithmSA.h"
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


CScheduleAlgorithmSA::CScheduleAlgorithmSA(std::vector<CResource*>& rResources)
	: CScheduleAlgorithm(rResources)
{
	mpEstimation = CEstimation::getEstimation();
}

CScheduleAlgorithmSA::~CScheduleAlgorithmSA() {
	delete mpEstimation;
	mpEstimation = 0;
}

int CScheduleAlgorithmSA::init() {

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

void CScheduleAlgorithmSA::fini() {
}

void CScheduleAlgorithmSA::updateRatio(double* ready, int machines) {
	double min = 0.0;
	double max = 0.0;
	for (int mix=0; mix<machines; mix++) {
		if (mix == 0 || ready[mix]<min) {
			min = ready[mix];
		}
		if (mix == 0 || ready[mix]>max) {
			max = ready[mix];
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

CSchedule* CScheduleAlgorithmSA::compute(std::vector<CTaskCopy>* pTasks, std::vector<CTaskCopy*>* runningTasks, volatile int* interrupt, int updated) {

	int tasks = pTasks->size();
	int machines = mrResources.size();

	CSchedule* sched = new CSchedule(tasks, machines, mrResources);
	sched->mpOTasks = pTasks;

	// generate map for global id to local id conversion
	std::unordered_map<int,int> taskmap;
	for (unsigned int i=0; i<pTasks->size(); i++) {
		taskmap[(*pTasks)[i].mId] = i;
	}


	std::chrono::steady_clock::time_point currentTime = std::chrono::steady_clock::now();

	double* pResourceReady = new double[machines]();
	// copy previous schedule
	if (mpPreviousSchedule != 0) {
		for (int mix=0; mix<machines; mix++) {
			std::vector<STaskEntry*>* mList = (*mpPreviousSchedule->mpTasks)[mix];
			CResource* res = mrResources[mix];

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

				if (task->mState == ETaskState::RUNNING) {

					// task is running
					sched->updateEntryProgress(newentry, task, res, currentTime, updated);

				} else {

					// task has some other state
					newentry->startProgress = task->mProgress;

				}


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

	// update sched times
	sched->computeTimes();

	// update machine ready times
	for (int mix=0; mix<machines; mix++) {
		int mix_num = (*(sched->mpTasks))[mix]->size();
		if (mix_num == 0) {
			pResourceReady[mix] = 0.0;
		} else {
			STaskEntry* lastentry = (*(*(sched->mpTasks))[mix])[mix_num-1];
			pResourceReady[mix] = lastentry->timeFinish.count() / (double)1000000000.0;
		}
	}

	std::vector<STaskEntry*> newEntries;


	// update ratio
	updateRatio(pResourceReady, machines);

	// map tasks
	for (unsigned int tix = 0; tix<todoTasks.size(); tix++) {
		CTaskCopy* task = todoTasks[tix];

		int depNum = task->mPredecessorNum;
		double depReady = 0.0;
		if (depNum > 0) {
			// find latest finish time for predecessors
			// assumption: all predecessor tasks are in the todoTasks/newEntries list
			for (int six=0; six<depNum; six++) {
				int sid = task->mpPredecessorList[six];
				int sentryIx = -1;
				for (unsigned int todoIx=0; todoIx<tix; todoIx++) {
					CTaskCopy* stask = todoTasks[todoIx];
					if (stask->mId == sid) {
						sentryIx = todoIx;
						break;
					}
				}
				if (sentryIx == -1) {
					continue;
				}
				STaskEntry* sentry = newEntries[sentryIx];
				double sfinish = sentry->timeFinish.count() / (double)1000000000.0;
				if (sfinish > depReady) {
					depReady = sfinish;
				}
			}
		}

		double selectedComplete = 0.0;
		double selectedExec = 0.0;
		int selectedMix = -1;
		for (int mix = 0; mix<machines; mix++) {
			CResource* res = mrResources[mix];

			if (task->validResource(res) == false) {
				continue;
			}

			double resourceReady = pResourceReady[mix];
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

		pResourceReady[selectedMix] = selectedComplete;

		STaskEntry* entry = new STaskEntry();
		entry->taskcopy = task;
		entry->taskid = entry->taskcopy->mId;
		entry->stopProgress = entry->taskcopy->mCheckpoints;
		entry->timeFinish = std::chrono::duration<long long,std::nano>( (long long) (selectedComplete*1000000000.0));
		(*(sched->mpTasks))[selectedMix]->push_back(entry);
		newEntries.push_back(entry);

		mTaskMap[task->mId] = 1;
		updateRatio(pResourceReady, machines);
	}

	sched->mActiveTasks = tasks;
	mpPreviousSchedule = sched;
	delete[] pResourceReady;

	return sched;
}
