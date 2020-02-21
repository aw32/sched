// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#include <limits>
#include <list>
#include <algorithm>
#include "CScheduleAlgorithmMinMinMig2Dyn.h"
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



CScheduleAlgorithmMinMinMig2Dyn::CScheduleAlgorithmMinMinMig2Dyn(std::vector<CResource*>& rResources)
	: CScheduleAlgorithm(rResources)
{
	mpEstimation = CEstimation::getEstimation();
}

CScheduleAlgorithmMinMinMig2Dyn::~CScheduleAlgorithmMinMinMig2Dyn() {
	delete mpEstimation;
	mpEstimation = 0;
}

int CScheduleAlgorithmMinMinMig2Dyn::init() {
	return 0;
}

void CScheduleAlgorithmMinMinMig2Dyn::fini() {
}

CSchedule* CScheduleAlgorithmMinMinMig2Dyn::compute(std::vector<CTaskCopy>* pTasks, std::vector<CTaskCopy*>* runningTasks, volatile int* interrupt, int updated) {

	int tasks = pTasks->size();
	int machines = mrResources.size();

	CScheduleExt* sched = new CScheduleExt(tasks, machines, mrResources, pTasks);
	sched->setRunningTasks(runningTasks);


	// completion time matrix
	double* C = new double[tasks * machines]();

	// fill matrix
	for (int tix = 0; tix<tasks; tix++) {
		if (sched->taskDependencySatisfied(tix) == false) {
			// task not ready, matrix will be filled once dependencies are done
			continue;
		}
		CTask* task = (CTask*)  &((*pTasks)[tix]);
		for (int mix = 0; mix<machines; mix++) {
			CResource* res = mrResources[mix];

			if (task->validResource(res) == false) {
				// resource not valid for this task
				continue;
			}

			// ready time for this task on this resource considering running tasks
			double ready = sched->taskReadyTimeResource(tix, res, mpEstimation);
			double init = mpEstimation->taskTimeInit(task, res);
			if (sched->taskRunningResource(tix) == mix) {
				// is task already running on resource
				// ignore init time for first slot on resource
				init = 0.0;
			}
			double compute = mpEstimation->taskTimeCompute(task, res, task->mProgress, task->mCheckpoints);
			double fini = mpEstimation->taskTimeFini(task, res);
			double complete = sched->taskCompletionTime(tix, res, 0, ready, init, compute, fini);
			C[tix*machines + mix] = complete;
		}
	}

	int unmappedTasks = tasks;


	MigOpt min_mig;
	MigOpt m_min_mig;

	while (unmappedTasks > 0) {

		min_mig.parts[0].mix = -1;

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

			CTaskCopy* task = &((*pTasks)[tix]);
			m_min_mig.parts[0].mix = -1;
			for (int mixA = 0; mixA < machines; mixA++) {
				CResource* resA = mrResources[mixA];

				if (task->validResource(resA) == false) {
					continue;
				}

				double resReadyA = sched->taskReadyTimeResource(tix, resA, mpEstimation);

				if (readyTask > resReadyA) {
					resReadyA = readyTask;
				}

				int runMix = sched->taskRunningResource(tix);
				int slotA = sched->resourceTasks(mixA);

				for (int mixB = 0; mixB < machines; mixB++) {
					CResource* resB = mrResources[mixB];

					if (task->validResource(resB) == false) {
						continue;
					}

					double resReadyB = sched->taskReadyTimeResource(tix, resB, mpEstimation);
					if (readyTask > resReadyB) {
						resReadyB = readyTask;
					}

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
					if (runMix == mixA && slotA == 0) {
						// task A is already running on mixA and continues running
						budget = readyDiff - finiA;
					}
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
					CLogger::mainlog->debug("MCT: possible mig taskid %d mixA %d mixB %d readyA %lf readyB %lf completeA %lf completeB %lf",
						task->mId,
						mixA,
						mixB,
						resReadyA,
						resReadyB,
						completeA,
						completeB
						);
					if (m_min_mig.parts[0].mix == -1 || completeB < m_min_mig.complete) {
						m_min_mig.tix = tix;
						m_min_mig.complete = completeB;
						m_min_mig.parts[0].mix = mixA;
						m_min_mig.parts[0].startProgress = task->mProgress;
						m_min_mig.parts[0].stopProgress = task->mProgress+pointsA;
						m_min_mig.parts[0].complete = completeA;
						m_min_mig.parts[1].mix = mixB;
						m_min_mig.parts[1].startProgress = task->mProgress+pointsA;
						m_min_mig.parts[1].stopProgress = task->mCheckpoints;
						m_min_mig.parts[1].complete = completeB;
					}
				} // end for mixB
			} // end for mix A

			if (m_min_mig.parts[0].mix != -1) {
				if (min_mig.parts[0].mix == -1 || m_min_mig.complete < min_mig.complete) {
					min_mig = m_min_mig;
				}
			}
		}

		// task without migration with earliest completion time
		int min_tix = -1;
		int min_tix_mix = -1;
		double min_tix_comp = std::numeric_limits<double>::max();
		CTaskCopy* min_task = 0;

		for (int tix = 0; tix<tasks; tix++) {
			// skip already mapped tasks
			if (sched->taskLastPartMapped(tix) == true) {
				continue;
			}
			// skip tasks whose predecessors are not ready
			if (sched->taskDependencySatisfied(tix) == false) {
				continue;
			}
			CTaskCopy* task = &((*pTasks)[tix]);
			// machine with earliest completion time
			int min_mix = 0;
			double min_comp = 0.0;
			int first = 0;

			for (int mix = 0; mix<machines; mix++) {

				CResource* res = mrResources[mix];

				if (task->validResource(res) == false) {
					continue;
				}

				double new_comp = C[tix*machines + mix];

				CLogger::mainlog->debug("MinMin: tix %d mix %d completion time %lf", tix, mix, new_comp);

				if (first == 0 || new_comp < min_comp) {
					min_comp = new_comp;
					min_mix = mix;
					first = 1;
				}
			}
			if (min_comp < min_tix_comp) {
				min_tix = tix;
				min_tix_mix = min_mix;
				min_tix_comp = min_comp;
				min_task = task;
			}
		}

		CLogger::mainlog->debug("MinMin: tix %d tid %d mix %d", min_tix, (min_task == 0?-1:min_task->mId), min_tix_mix);

		bool migrate = false;
		if (min_mig.parts[0].mix != -1) {
			if (min_mig.complete < min_tix_comp) {
				migrate = true;
			}
		}

		if (migrate == false) {

			// add task to machine queue
			STaskEntry* entry = new STaskEntry();
			entry->taskcopy = &((*pTasks)[min_tix]);
			entry->taskid = entry->taskcopy->mId;
			entry->startProgress = entry->taskcopy->mProgress;
			entry->stopProgress = entry->taskcopy->mCheckpoints;
			sched->addEntry(entry, mrResources[min_tix_mix], -1);

		} else {

			// add migration option

			CTaskCopy* task = &((*pTasks)[min_mig.tix]);

			STaskEntry* entry = new STaskEntry();
			entry->taskcopy = task;
			entry->taskid = entry->taskcopy->mId;
			entry->startProgress = min_mig.parts[0].startProgress;
			entry->stopProgress = min_mig.parts[0].stopProgress;
			sched->addEntry(entry, mrResources[min_mig.parts[0].mix], -1);

			CLogger::mainlog->debug("MinMin apply task %d mix %d start %d stop %d", task->mId, min_mig.parts[0].mix,
			min_mig.parts[0].startProgress,
			min_mig.parts[0].stopProgress);

			entry = new STaskEntry();
			entry->taskcopy = task;
			entry->taskid = entry->taskcopy->mId;
			entry->startProgress = min_mig.parts[1].startProgress;
			entry->stopProgress = min_mig.parts[1].stopProgress;
			sched->addEntry(entry, mrResources[min_mig.parts[1].mix], -1);

			CLogger::mainlog->debug("MinMin apply task %d mix %d start %d stop %d", task->mId, min_mig.parts[1].mix,
			min_mig.parts[1].startProgress,
			min_mig.parts[1].stopProgress);
			min_task = &((*pTasks)[min_mig.tix]);
		}

		unmappedTasks--;
		if (unmappedTasks == 0) {
			break;
		}

		CResource* min_res = mrResources[min_tix_mix];

		// update completion time matrix
		for (int tix = 0; tix<tasks; tix++) {
			if (sched->taskLastPartMapped(tix)) {
				continue;
			}
			CTaskCopy* task = &((*pTasks)[tix]);
			CLogger::mainlog->debug("MinMin: Check task %d", tix);
			bool succ = false;
			for (int six=0; six<min_task->mSuccessorNum; six++) {
				if (min_task->mpSuccessorList[six] == task->mId) {
					// successor task found
					succ = true;
					break;
				}
			}
			if (succ == true) {
				if (sched->taskDependencySatisfied(tix) == true) {
					// task was just freed from dependencies
					// update ready times
					CLogger::mainlog->debug("MinMin: freed task %d", tix);

					for (int mix = 0; mix<machines; mix++) {
						CResource* res = mrResources[mix];

						if (task->validResource(res) == false) {
							continue;
						}


						// ready time for this task on this resource considering running tasks
						double ready = sched->taskReadyTimeResource(tix, res, mpEstimation);
						double init = mpEstimation->taskTimeInit(task, res);
						if (sched->taskRunningResource(tix) == mix &&
							sched->resourceTasks(mix) == 0) {
							// task already running on resource and
							// slot is first slot
							init = 0.0;
						}

						double compute = mpEstimation->taskTimeCompute(task, res, task->mProgress, task->mCheckpoints);
						double fini = mpEstimation->taskTimeFini(task, res);
						int nextSlot = sched->resourceTasks(mix);
						double complete = sched->taskCompletionTime(tix, res, nextSlot, ready, init, compute, fini);
						C[tix*machines + mix] = complete;

					}


				}
			} else {

				if (sched->taskDependencySatisfied(tix) == false) {
					continue;
				}

				if (migrate == false) {

					if (task->validResource(min_res) == false) {
						continue;
					}

					// ready time for this task on this resource considering running tasks
					double ready = sched->taskReadyTimeResource(tix, min_res, mpEstimation);
					double init = mpEstimation->taskTimeInit(task, min_res);
					if (sched->taskRunningResource(tix) == min_tix_mix &&
						sched->resourceTasks(min_tix_mix) == 0) {
						// task already running on resource and
						// slot is first slot
						init = 0.0;
					}

					double compute = mpEstimation->taskTimeCompute(task, min_res, task->mProgress, task->mCheckpoints);
					double fini = mpEstimation->taskTimeFini(task, min_res);
					int nextSlot = sched->resourceTasks(min_tix_mix);
					double complete = sched->taskCompletionTime(tix, min_res, nextSlot, ready, init, compute, fini);
					C[tix*machines + min_tix_mix] = complete;

				} else {

					CResource* resA = mrResources[min_mig.parts[0].mix];
					if (task->validResource(resA) == false ) {
						continue;
					} else {

						// ready time for this task on this resource considering running tasks
						double ready = sched->taskReadyTimeResource(tix, resA, mpEstimation);
						double init = mpEstimation->taskTimeInit(task, resA);
						if (sched->taskRunningResource(tix) == min_mig.parts[0].mix &&
							sched->resourceTasks(min_mig.parts[0].mix) == 0) {
							// task already running on resource and
							// slot is first slot
							init = 0.0;
						}

						double compute = mpEstimation->taskTimeCompute(task, resA, task->mProgress, task->mCheckpoints);
						double fini = mpEstimation->taskTimeFini(task, resA);
						int nextSlot = sched->resourceTasks(resA->mId);
						double complete = sched->taskCompletionTime(tix, resA, nextSlot, ready, init, compute, fini);
						C[tix*machines + min_mig.parts[0].mix] = complete;

					}

					CResource* resB = mrResources[min_mig.parts[1].mix];
					if (task->validResource(resB) == false) {
						continue;
					} else {

						// ready time for this task on this resource considering running tasks
						double ready = sched->taskReadyTimeResource(tix, resB, mpEstimation);
						double init = mpEstimation->taskTimeInit(task, resB);
						if (sched->taskRunningResource(tix) == min_mig.parts[1].mix &&
							sched->resourceTasks(min_mig.parts[1].mix) == 0) {
							// task already running on resource and
							// slot is first slot
							init = 0.0;
						}

						double compute = mpEstimation->taskTimeCompute(task, resB, task->mProgress, task->mCheckpoints);
						double fini = mpEstimation->taskTimeFini(task, resB);
						int nextSlot = sched->resourceTasks(resB->mId);
						double complete = sched->taskCompletionTime(tix, resB, nextSlot, ready, init, compute, fini);
						C[tix*machines + min_mig.parts[1].mix] = complete;

					}
				}
			}
		}
	}

	sched->computeTimes();

	// clean up
	delete[] C;

	return sched;
}
