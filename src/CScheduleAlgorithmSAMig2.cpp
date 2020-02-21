// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#include <limits>
#include <list>
#include <algorithm>
#include "CConfig.h"
#include "CScheduleAlgorithmSAMig2.h"
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


CScheduleAlgorithmSAMig2::CScheduleAlgorithmSAMig2(std::vector<CResource*>& rResources)
	: CScheduleAlgorithm(rResources)
{
	mpEstimation = CEstimation::getEstimation();
}

CScheduleAlgorithmSAMig2::~CScheduleAlgorithmSAMig2() {
	delete mpEstimation;
	mpEstimation = 0;
}

int CScheduleAlgorithmSAMig2::init() {

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

void CScheduleAlgorithmSAMig2::fini() {
}

void CScheduleAlgorithmSAMig2::updateRatio(CScheduleExt* sched, int machines) {
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

CSchedule* CScheduleAlgorithmSAMig2::compute(std::vector<CTaskCopy>* pTasks, std::vector<CTaskCopy*>* runningTasks, volatile int* interrupt, int updated) {

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

		// test migration options
		MigOpt min_mig = {};
		min_mig.parts[0].mix = -1;

		for (int mixA = 0; mixA<machines; mixA++) {
			CResource* resA = mrResources[mixA];

			if (task->validResource(resA) == false) {
				continue;
			}

			double resReadyA = sched->resourceReadyTime(mixA);
			if (depReady > resReadyA) {
				resReadyA = depReady;
			}
			for (int mixB = 0; mixB<machines; mixB++) {
				CResource* resB = mrResources[mixB];

				if (task->validResource(resB) == false) {
					continue;
				}
			
				double resReadyB = sched->resourceReadyTime(mixB);	
				if (depReady > resReadyB) {
					resReadyB = depReady;
				}
				
				if (resReadyA >= resReadyB) {
					// no gain
					continue;
				}

				double readyDiff = resReadyB-resReadyA;
				double initA = mpEstimation->taskTimeInit(task, resA);
				double initB = mpEstimation->taskTimeInit(task, resB);
				double finiA = mpEstimation->taskTimeFini(task, resA);
				double finiB = mpEstimation->taskTimeFini(task, resB);
				double budget = readyDiff - initA - finiA;
				int pointsA = mpEstimation->taskTimeComputeCheckpoint(task, resA, task->mProgress, budget);
				if (budget <= 0.0 || pointsA <= 0) {
					// no gain
					continue;
				}
				if (pointsA >= task->mCheckpoints - task->mProgress) {
					// migration unnecessary
					continue;
				}
				CLogger::mainlog->debug("SA: budget %lf points %d", budget, pointsA);
				double timeA = initA + finiA + mpEstimation->taskTimeCompute(task, resA, task->mProgress, task->mProgress+pointsA);
				double timeB = initB + finiB + mpEstimation->taskTimeCompute(task, resB, task->mProgress+pointsA, task->mCheckpoints);
				double completeA = resReadyA + timeA;
				double completeB = resReadyB + timeB;
				CLogger::mainlog->debug("SA: possible mig taskid %d mixA %d mixB %d readyA %lf readyB %lf completeA %lf completeB %lf",
					task->mId,
					mixA,
					mixB,
					resReadyA,
					resReadyB,
					completeA,
					completeB
					);
				if (min_mig.parts[0].mix == -1 || 

					(mMode == SA_MODE_MCT && completeB < min_mig.parts[0].complete) ||
					(mMode == SA_MODE_MET && timeA + timeB < min_mig.parts[0].exec) ) {

					min_mig.complete = completeB;
					min_mig.exec = timeA + timeB;
					min_mig.parts[0].mix = mixA;
					min_mig.parts[0].startProgress = task->mProgress;
					min_mig.parts[0].stopProgress = task->mProgress+pointsA;
					min_mig.parts[0].complete = completeA;
					min_mig.parts[0].exec = timeA;
					min_mig.parts[1].mix = mixB;
					min_mig.parts[1].startProgress = task->mProgress+pointsA;
					min_mig.parts[1].stopProgress = task->mCheckpoints;
					min_mig.parts[1].complete = completeB;
					min_mig.parts[1].exec = timeB;
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


		bool migrate = false;
		if (min_mig.parts[0].mix >= 0) {
			if (mMode == SA_MODE_MCT && min_mig.complete < selectedComplete) {
				migrate = true;
			}
			if (mMode == SA_MODE_MET && min_mig.exec < selectedExec) {
				migrate = true;
			}
		}

		if (migrate == false) {

			// only add normal option

			STaskEntry* entry = new STaskEntry();
			entry->taskcopy = task;
			entry->taskid = entry->taskcopy->mId;
			entry->startProgress = entry->taskcopy->mProgress;
			entry->stopProgress = entry->taskcopy->mCheckpoints;
			sched->computeExecutionTime(entry, mrResources[selectedMix]);
			sched->addEntry(entry, mrResources[selectedMix], -1);
			newEntries.push_back(entry);

			CLogger::mainlog->debug("SA apply task %d mix %d start %d stop %d", task->mId, selectedMix,
			entry->startProgress,
			entry->stopProgress);
		} else {

			// add migration option

			STaskEntry* entry = new STaskEntry();
			entry->taskcopy = task;
			entry->taskid = entry->taskcopy->mId;
			entry->startProgress = min_mig.parts[0].startProgress;
			entry->stopProgress = min_mig.parts[0].stopProgress;
			sched->addEntry(entry, mrResources[min_mig.parts[0].mix], -1);

			CLogger::mainlog->debug("SA apply task %d mix %d start %d stop %d", task->mId, min_mig.parts[0].mix,
			min_mig.parts[0].startProgress,
			min_mig.parts[0].stopProgress);

			entry = new STaskEntry();
			entry->taskcopy = task;
			entry->taskid = entry->taskcopy->mId;
			entry->startProgress = min_mig.parts[1].startProgress;
			entry->stopProgress = min_mig.parts[1].stopProgress;
			sched->addEntry(entry, mrResources[min_mig.parts[1].mix], -1);

			CLogger::mainlog->debug("SA apply task %d mix %d start %d stop %d", task->mId, min_mig.parts[1].mix,
			min_mig.parts[1].startProgress,
			min_mig.parts[1].stopProgress);

		}

		mTaskMap[task->mId] = 1;
		updateRatio(sched, machines);
	}

	mpPreviousSchedule = sched;

	sched->computeTimes();

	return sched;
}
