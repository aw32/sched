// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#include <limits>
#include <list>
#include <algorithm>
#include "CScheduleAlgorithmOLBMig2.h"
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


CScheduleAlgorithmOLBMig2::CScheduleAlgorithmOLBMig2(std::vector<CResource*>& rResources)
	: CScheduleAlgorithm(rResources)
{
	mpEstimation = CEstimation::getEstimation();
}

CScheduleAlgorithmOLBMig2::~CScheduleAlgorithmOLBMig2(){
	delete mpEstimation;
	mpEstimation = 0;
}

int CScheduleAlgorithmOLBMig2::init() {
	return 0;
}

void CScheduleAlgorithmOLBMig2::fini() {
}

CSchedule* CScheduleAlgorithmOLBMig2::compute(std::vector<CTaskCopy>* pTasks, std::vector<CTaskCopy*>* runningTasks, volatile int* interrupt, int updated) {

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

	// map tasks
	for (unsigned int tix = 0; tix<todoTasks.size(); tix++) {
		CTaskCopy* task = todoTasks[tix];

		double depReady = sched->taskReadyTime(task);

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
				CLogger::mainlog->debug("OLB: budget %lf points %d", budget, pointsA);
				double timeA = initA + finiA + mpEstimation->taskTimeCompute(task, resA, task->mProgress, task->mProgress+pointsA);
				double timeB = initB + finiB + mpEstimation->taskTimeCompute(task, resB, task->mProgress+pointsA, task->mCheckpoints);
				double completeA = resReadyA + timeA;
				double completeB = resReadyB + timeB;
				CLogger::mainlog->debug("OLB: possible mig taskid %d mixA %d mixB %d readyA %lf readyB %lf completeA %lf completeB %lf",
					task->mId,
					mixA,
					mixB,
					resReadyA,
					resReadyB,
					completeA,
					completeB
					);
				if (min_mig.parts[0].mix == -1) {
					// Select new option if ready time smaller or if equal select minimal completion time
					if (resReadyA < min_mig.ready || (resReadyA == min_mig.ready && completeB < min_mig.complete)) {
						min_mig.complete = completeB;
						min_mig.ready = resReadyA;
						min_mig.parts[0].mix = mixA;
						min_mig.parts[0].startProgress = task->mProgress;
						min_mig.parts[0].stopProgress = task->mProgress+pointsA;
						min_mig.parts[0].complete = completeA;
						min_mig.parts[0].ready = resReadyA;
						min_mig.parts[1].mix = mixB;
						min_mig.parts[1].startProgress = task->mProgress+pointsA;
						min_mig.parts[1].stopProgress = task->mCheckpoints;
						min_mig.parts[1].complete = completeB;
						min_mig.parts[1].ready = resReadyB;
					}
				}
			}
		}

		double selectedComplete = 0.0;
		double selectedReady = 0.0;
		int selectedMix = -1;
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
			double resourceReady = sched->resourceReadyTime(res);
			double ready = (depReady > resourceReady ? depReady : resourceReady);
			double complete = 
				ready +
				mpEstimation->taskTimeInit(task, res) +
				mpEstimation->taskTimeCompute(task, res, task->mProgress, task->mCheckpoints) + 
				mpEstimation->taskTimeFini(task, res);
			if (mix == 0 || ready < selectedReady) {
				selectedMix = mix;
				selectedComplete = complete;
				selectedReady = ready;
			}
		}

		bool migrate = false;
		if (min_mig.parts[0].mix >= 0) {
			// select option with smaller ready time or if equal select minimal completion time
			if (min_mig.ready < selectedReady || (min_mig.ready == selectedReady && min_mig.complete < selectedComplete)) {
				migrate = true;
			}
		}

		if (migrate == false) {

			STaskEntry* entry = new STaskEntry();
			entry->taskcopy = task;
			entry->taskid = entry->taskcopy->mId;
			entry->startProgress = entry->taskcopy->mProgress;
			entry->stopProgress = entry->taskcopy->mCheckpoints;
			sched->computeExecutionTime(entry, mrResources[selectedMix]);
			sched->addEntry(entry, mrResources[selectedMix], -1);

			CLogger::mainlog->debug("OLB apply task %d mix %d start %d stop %d", task->mId, selectedMix,
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

			CLogger::mainlog->debug("OLB apply task %d mix %d start %d stop %d", task->mId, min_mig.parts[0].mix,
			min_mig.parts[0].startProgress,
			min_mig.parts[0].stopProgress);

			entry = new STaskEntry();
			entry->taskcopy = task;
			entry->taskid = entry->taskcopy->mId;
			entry->startProgress = min_mig.parts[1].startProgress;
			entry->stopProgress = min_mig.parts[1].stopProgress;
			sched->addEntry(entry, mrResources[min_mig.parts[1].mix], -1);

			CLogger::mainlog->debug("OLB apply task %d mix %d start %d stop %d", task->mId, min_mig.parts[1].mix,
			min_mig.parts[1].startProgress,
			min_mig.parts[1].stopProgress);

		}

		mTaskMap[task->mId] = 1;
	}

	mpPreviousSchedule = sched;

	sched->computeTimes();

	return sched;
}
