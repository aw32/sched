// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#include <limits>
#include <list>
#include <algorithm>
#include "CScheduleAlgorithmSufferageMig2Dyn.h"
#include "CTask.h"
#include "CTaskCopy.h"
#include "CEstimation.h"
#include "CSchedule.h"
#include "CScheduleExt.h"
#include "CResource.h"
#include "CLogger.h"
using namespace sched::algorithm;
using sched::task::CTask;
using sched::schedule::STaskEntry;
using sched::schedule::CSchedule;
using sched::schedule::CScheduleExt;



CScheduleAlgorithmSufferageMig2Dyn::CScheduleAlgorithmSufferageMig2Dyn(std::vector<CResource*>& rResources)
	: CScheduleAlgorithm(rResources)
{
	mpEstimation = CEstimation::getEstimation();
}

CScheduleAlgorithmSufferageMig2Dyn::~CScheduleAlgorithmSufferageMig2Dyn() {
	delete mpEstimation;
	mpEstimation = 0;
}

int CScheduleAlgorithmSufferageMig2Dyn::init() {
	return 0;
}

void CScheduleAlgorithmSufferageMig2Dyn::fini() {
}

CSchedule* CScheduleAlgorithmSufferageMig2Dyn::compute(std::vector<CTaskCopy>* pTasks, std::vector<CTaskCopy*>* runningTasks, volatile int* interrupt, int updated) {

	int tasks = pTasks->size();
	int machines = mrResources.size();

	if (machines < 2) {
		CLogger::mainlog->error("Sufferage: less than 2 resources !!!");
	}

	CScheduleExt* sched = new CScheduleExt(tasks, machines, mrResources, pTasks);
	sched->setRunningTasks(runningTasks);

	int unmappedTasks = tasks;

	struct MigOpt min_suff_mig;
	struct MigOpt min_ct_mig;

	while (unmappedTasks > 0) {

		// reset best option
		min_suff_mig.parts[0].mix = -1;

		for (int tix = 0; tix < tasks; tix++) {
			// skip already mapped tasks
			if (sched->taskLastPartMapped(tix) == true) {
				continue;
			}
			// skip tasks whose predecessors are not ready
			if (sched->taskDependencySatisfied(tix) == false) {
				continue;
			}
			double readyTask = sched->taskReadyTime(tix);

			// reset best completion time option
			min_ct_mig.parts[0].mix = -1;
			double completeFirst = 0.0;
			double completeSecond = 0.0;

			CTaskCopy* task = &((*pTasks)[tix]);


			// check migration options
			for (int mixA = 0; mixA < machines; mixA++) {
				CResource* resA = mrResources[mixA];

				if (task->validResource(resA) == false) {
					continue;
				}

				double resReadyA = sched->taskReadyTimeResource(tix, resA, mpEstimation);
				if (readyTask > resReadyA) {
					resReadyA = readyTask;
				}
				CLogger::mainlog->debug("Sufferage ready mix %d mixtasks %d tix %d ready %lf",
					mixA, sched->resourceTasks(mixA),
					tix, resReadyA);

				for (int mixB = 0; mixB < machines; mixB++) {
					CResource* resB = mrResources[mixB];

					if (task->validResource(resB) == false) {
						continue;
					}

					double resReadyB = sched->taskReadyTimeResource(tix, resB, mpEstimation);
					if (readyTask > resReadyB) {
						resReadyB = readyTask;
					}
					CLogger::mainlog->debug("Sufferage ready mix %d mixtasks %d tix %d ready %lf",
						mixB, sched->resourceTasks(mixB),
						tix, resReadyB);

					if (resReadyB <= resReadyA) {
						continue;
					}

					double readyDiff = resReadyB-resReadyA;
					double initA = mpEstimation->taskTimeInit(task, resA);
					if (sched->taskRunningResource(tix) == mixA &&
						sched->resourceTasks(mixA) == 0) {
						// task already running on resource and
						// slot is first slot
						initA = 0.0;
					}
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
					double timeA = initA + finiA + mpEstimation->taskTimeCompute(task, resA, task->mProgress, task->mProgress+pointsA);
					double timeB = initB + finiB + mpEstimation->taskTimeCompute(task, resB, task->mProgress+pointsA, task->mCheckpoints);
					double completeA = resReadyA + timeA;
					double completeB = resReadyB + timeB;
					CLogger::mainlog->debug("Sufferage: possible mig taskid %d mixA %d mixB %d readyA %lf readyB %lf completeA %lf completeB %lf",
						task->mId,
						mixA,
						mixB,
						resReadyA,
						resReadyB,
						completeA,
						completeB
						);
					if (min_ct_mig.parts[0].mix == -1 || completeB < min_ct_mig.complete) {
						min_ct_mig.tix = tix;
						min_ct_mig.complete = completeB;
						min_ct_mig.parts[0].mix = mixA;
						min_ct_mig.parts[0].startProgress = task->mProgress;
						min_ct_mig.parts[0].stopProgress = task->mProgress+pointsA;
						min_ct_mig.parts[0].complete = completeA;
						min_ct_mig.parts[1].mix = mixB;
						min_ct_mig.parts[1].startProgress = task->mProgress+pointsA;
						min_ct_mig.parts[1].stopProgress = task->mCheckpoints;
						min_ct_mig.parts[1].complete = completeB;
					}
					// update completion time
					if (completeB < completeFirst) {
						completeSecond = completeFirst;
						completeFirst = completeB;
					} else
					if (completeB < completeSecond) {
						completeSecond = completeB;
					}
				} // end for mixB
			} // end for mix A

			// check normal options

			for (int mix = 0; mix<machines; mix++) {

				CResource* res = mrResources[mix];

				if (task->validResource(res) == false) {
					continue;
				}


				double ready = sched->taskReadyTimeResource(tix, res, mpEstimation);
					CLogger::mainlog->debug("Sufferage ready mix %d mixtasks %d tix %d ready %lf",
						mix, sched->resourceTasks(mix),
						tix, ready);
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

				if (min_ct_mig.parts[0].mix == -1 || min_ct_mig.complete > complete) {
					min_ct_mig.tix = tix;
					min_ct_mig.complete = complete;
					min_ct_mig.parts[0].mix = mix;
					min_ct_mig.parts[0].startProgress = task->mProgress;
					min_ct_mig.parts[0].stopProgress = task->mCheckpoints;
					min_ct_mig.parts[0].complete = complete;
					min_ct_mig.parts[1].mix = -1;
				}

				if (complete < completeFirst) {
					completeSecond = completeFirst;
					completeFirst = complete;
				} else
				if (complete < completeSecond) {
					completeSecond = complete;
				}
			} // end for mix

			// compute sufferage
			min_ct_mig.sufferage = completeSecond - completeFirst;

			// adopt option if sufferage is larger
			if (min_suff_mig.parts[0].mix == -1 || min_ct_mig.sufferage < min_suff_mig.sufferage) {
				min_suff_mig = min_ct_mig;
			}

		} // end for tix



		CTaskCopy* task = &((*pTasks)[min_suff_mig.tix]);
		STaskEntry* entry = new STaskEntry();
		entry->taskcopy = task;
		entry->taskid = entry->taskcopy->mId;
		entry->startProgress = min_suff_mig.parts[0].startProgress;
		entry->stopProgress = min_suff_mig.parts[0].stopProgress;
		sched->addEntry(entry, mrResources[min_suff_mig.parts[0].mix], -1);

		CLogger::mainlog->debug("Sufferage: apply task %d mix %d start %d stop %d", task->mId, min_suff_mig.parts[0].mix,
		min_suff_mig.parts[0].startProgress,
		min_suff_mig.parts[0].stopProgress);

		// migration option
		if (min_suff_mig.parts[1].mix != -1) {

			CTaskCopy* task = &((*pTasks)[min_suff_mig.tix]);
			STaskEntry* entry = new STaskEntry();
			entry->taskcopy = task;
			entry->taskid = entry->taskcopy->mId;
			entry->startProgress = min_suff_mig.parts[1].startProgress;
			entry->stopProgress = min_suff_mig.parts[1].stopProgress;
			sched->addEntry(entry, mrResources[min_suff_mig.parts[1].mix], -1);

			CLogger::mainlog->debug("Sufferage: apply task %d mix %d start %d stop %d", task->mId, min_suff_mig.parts[1].mix,
			min_suff_mig.parts[1].startProgress,
			min_suff_mig.parts[1].stopProgress);
			
		}

		unmappedTasks--;
		if (unmappedTasks == 0) {
			break;
		}

	}

	sched->computeTimes();

	// clean up

	return sched;
}
