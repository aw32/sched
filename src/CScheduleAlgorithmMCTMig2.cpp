// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#include <limits>
#include <list>
#include <algorithm>
#include "CScheduleAlgorithmMCTMig2.h"
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


CScheduleAlgorithmMCTMig2::CScheduleAlgorithmMCTMig2(std::vector<CResource*>& rResources)
	: CScheduleAlgorithm(rResources)
{
	mpEstimation = CEstimation::getEstimation();
}

CScheduleAlgorithmMCTMig2::~CScheduleAlgorithmMCTMig2(){
	delete mpEstimation;
	mpEstimation = 0;
}

int CScheduleAlgorithmMCTMig2::init() {
	return 0;
}

void CScheduleAlgorithmMCTMig2::fini() {
}

CSchedule* CScheduleAlgorithmMCTMig2::compute(std::vector<CTaskCopy>* pTasks, std::vector<CTaskCopy*>* runningTasks, volatile int* interrupt, int updated) {

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

	struct MCTOpt* options = new struct MCTOpt[machines];

	// map tasks
	for (unsigned int tix = 0; tix<todoTasks.size(); tix++) {
		CTaskCopy* task = todoTasks[tix];

		double depReady = sched->taskReadyTime(tix);

		CLogger::mainlog->debug("MCT task id %d depReady %lf", task->mId, depReady);

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
				CLogger::mainlog->debug("MCT: budget %lf points %d", budget, pointsA);
				double timeA = initA + finiA + mpEstimation->taskTimeCompute(task, resA, task->mProgress, task->mProgress+pointsA);
				double timeB = initB + finiB + mpEstimation->taskTimeCompute(task, resB, task->mProgress+pointsA, task->mCheckpoints);
				double completeA = resReadyA + timeA;
				double completeB = resReadyB + timeB;
				CLogger::mainlog->debug("MCT: possible mig taskid %d mixA %d mixB %d readyA %lf readyB %lf completeA %lf completeB %lf",
					task->mId,
					mixA,
					mixB,
					resReadyA,
					resReadyB,
					completeA,
					completeB
					);
				if (min_mig.parts[0].mix == -1 || completeB < min_mig.complete) {
					min_mig.complete = completeB;
					min_mig.parts[0].mix = mixA;
					min_mig.parts[0].startProgress = task->mProgress;
					min_mig.parts[0].stopProgress = task->mProgress+pointsA;
					min_mig.parts[0].complete = completeA;
					min_mig.parts[1].mix = mixB;
					min_mig.parts[1].startProgress = task->mProgress+pointsA;
					min_mig.parts[1].stopProgress = task->mCheckpoints;
					min_mig.parts[1].complete = completeB;
				}
			}
		}


		for (int mix = 0; mix<machines; mix++) {
			CResource* res = mrResources[mix];

			if (task->validResource(res) == false) {
				options[mix].mix = -1;
				continue;
			}

			double resourceReady = sched->resourceReadyTime(mix);
			double ready = (depReady > resourceReady ? depReady : resourceReady);
			// completion time =
			// ready time +
			// init time +
			// compute time +
			// fini time
			double init = mpEstimation->taskTimeInit(task, res);
			double compute = mpEstimation->taskTimeCompute(task, res, task->mProgress, task->mCheckpoints);
			double fini = mpEstimation->taskTimeFini(task, res);
			double complete = ready + init + compute + fini;

			options[mix].mix = mix;
			options[mix].ready = ready;
			options[mix].complete = complete;
			options[mix].init = init;
			options[mix].compute = compute;
			options[mix].fini = fini;
			options[mix].startProgress = task->mProgress;
			options[mix].stopProgress = task->mCheckpoints;
			CLogger::mainlog->debug("MCT opt tid %d mix %d ready %lf complete %lf", task->mId, mix, ready, complete);
		}

		// sort by completion time
		std::sort(
			options,
			options+machines,
			[](MCTOpt a, MCTOpt b) { // -> true, sort a before b
				if (a.mix == -1) {
					return false;
				}
				if (b.mix == -1) {
					return true;
				}
				return a.complete < b.complete;
			}
		);

		CLogger::mainlog->debug("MCT chosen opt tid %d mix %d ready %lf complete %lf", task->mId, options[0].mix, options[0].ready, options[0].complete);


		bool migrate = false;
		if (min_mig.parts[0].mix >= 0) {
			CLogger::mainlog->debug("Mig mig %lf opt %lf",  min_mig.complete , options[0].complete);
			migrate = min_mig.complete < options[0].complete;
		}

		if (migrate == false) {

			// only add normal option

			STaskEntry* entry = new STaskEntry();
			entry->taskcopy = task;
			entry->taskid = entry->taskcopy->mId;
			entry->startProgress = entry->taskcopy->mProgress;
			entry->stopProgress = entry->taskcopy->mCheckpoints;
			sched->addEntry(entry, mrResources[options[0].mix], -1);
			newEntries.push_back(entry);


			CLogger::mainlog->debug("MCT apply task %d mix %d start %d stop %d", task->mId, options[0].mix,
			options[0].startProgress,
			options[0].stopProgress);

		} else {

			// add migration option

			STaskEntry* entry = new STaskEntry();
			entry->taskcopy = task;
			entry->taskid = entry->taskcopy->mId;
			entry->startProgress = min_mig.parts[0].startProgress;
			entry->stopProgress = min_mig.parts[0].stopProgress;
			sched->addEntry(entry, mrResources[min_mig.parts[0].mix], -1);

			CLogger::mainlog->debug("MCT apply task %d mix %d start %d stop %d", task->mId, min_mig.parts[0].mix,
			min_mig.parts[0].startProgress,
			min_mig.parts[0].stopProgress);

			entry = new STaskEntry();
			entry->taskcopy = task;
			entry->taskid = entry->taskcopy->mId;
			entry->startProgress = min_mig.parts[1].startProgress;
			entry->stopProgress = min_mig.parts[1].stopProgress;
			sched->addEntry(entry, mrResources[min_mig.parts[1].mix], -1);

			CLogger::mainlog->debug("MCT apply task %d mix %d start %d stop %d", task->mId, min_mig.parts[1].mix,
			min_mig.parts[1].startProgress,
			min_mig.parts[1].stopProgress);

			// add final part to newEntries for dependency computation
			newEntries.push_back(entry);
			
		}

		mTaskMap[task->mId] = 1;
	}

	mpPreviousSchedule = sched;
	delete[] options;

	sched->computeTimes();

	return sched;
}
