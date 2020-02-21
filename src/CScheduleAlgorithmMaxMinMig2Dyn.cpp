// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#include <limits>
#include <list>
#include <algorithm>
#include "CScheduleAlgorithmMaxMinMig2Dyn.h"
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



CScheduleAlgorithmMaxMinMig2Dyn::CScheduleAlgorithmMaxMinMig2Dyn(std::vector<CResource*>& rResources)
	: CScheduleAlgorithm(rResources)
{
	mpEstimation = CEstimation::getEstimation();
}

CScheduleAlgorithmMaxMinMig2Dyn::~CScheduleAlgorithmMaxMinMig2Dyn() {
	delete mpEstimation;
	mpEstimation = 0;
}

int CScheduleAlgorithmMaxMinMig2Dyn::init() {
	return 0;
}

void CScheduleAlgorithmMaxMinMig2Dyn::fini() {
}

CSchedule* CScheduleAlgorithmMaxMinMig2Dyn::compute(std::vector<CTaskCopy>* pTasks, std::vector<CTaskCopy*>* runningTasks, volatile int* interrupt, int updated) {

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


	MigOpt max_mig;
	MigOpt m_min_mig;

	while (unmappedTasks > 0) {

		max_mig.parts[0].mix = -1;

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
					CLogger::mainlog->debug("MaxMin: budget %lf", budget);
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
					CLogger::mainlog->debug("MaxMin: possible mig taskid %d mixA %d mixB %d readyA %lf readyB %lf completeA %lf completeB %lf",
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
				if (max_mig.parts[0].mix == -1 || m_min_mig.complete > max_mig.complete) {
					max_mig = m_min_mig;
				}
			}
		}


		// task with latest completion time
		int max_tix = -1;
		int max_tix_mix = -1;
		double max_tix_comp = 0.0;
		CTaskCopy* max_task = 0;

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

				CLogger::mainlog->debug("MaxMin: tix %d mix %d completion time %lf", tix, mix, new_comp);

				if (first == 0 || new_comp < min_comp) {
					min_comp = new_comp;
					min_mix = mix;
					first = 1;
				}
			}
			if (min_comp > max_tix_comp) {
				max_tix = tix;
				max_tix_mix = min_mix;
				max_tix_comp = min_comp;
				max_task = task;
			}
		}

		CLogger::mainlog->debug("MaxMin: tix %d tid %d mix %d", max_tix, max_task->mId, max_tix_mix);

		bool migrate = false;
		if (max_mig.parts[0].mix != -1) {
			if (max_tix == max_mig.tix) {
				// same task, take option with earlier completion time
				if (max_mig.complete < max_tix_comp) {
					migrate = true;
				}
			} else {
				// different tasks, take option with later completion time
				if (max_mig.complete > max_tix_comp) {
					migrate = true;
				}

			}
		}

		if (migrate == false) {

			// add task to machine queue
			STaskEntry* entry = new STaskEntry();
			entry->taskcopy = &((*pTasks)[max_tix]);
			entry->taskid = entry->taskcopy->mId;
			entry->startProgress = entry->taskcopy->mProgress;
			entry->stopProgress = entry->taskcopy->mCheckpoints;
			sched->addEntry(entry, mrResources[max_tix_mix], -1);

		} else {

			// add migration option

			CTaskCopy* task = &((*pTasks)[max_mig.tix]);

			STaskEntry* entry = new STaskEntry();
			entry->taskcopy = task;
			entry->taskid = entry->taskcopy->mId;
			entry->startProgress = max_mig.parts[0].startProgress;
			entry->stopProgress = max_mig.parts[0].stopProgress;
			sched->addEntry(entry, mrResources[max_mig.parts[0].mix], -1);

			CLogger::mainlog->debug("MaxMin apply task %d mix %d start %d stop %d", task->mId, max_mig.parts[0].mix,
			max_mig.parts[0].startProgress,
			max_mig.parts[0].stopProgress);

			entry = new STaskEntry();
			entry->taskcopy = task;
			entry->taskid = entry->taskcopy->mId;
			entry->startProgress = max_mig.parts[1].startProgress;
			entry->stopProgress = max_mig.parts[1].stopProgress;
			sched->addEntry(entry, mrResources[max_mig.parts[1].mix], -1);

			CLogger::mainlog->debug("MaxMin apply task %d mix %d start %d stop %d", task->mId, max_mig.parts[1].mix,
			max_mig.parts[1].startProgress,
			max_mig.parts[1].stopProgress);
			max_task = &((*pTasks)[max_mig.tix]);
		}

		unmappedTasks--;
		if (unmappedTasks == 0) {
			break;
		}

		CResource* max_res = mrResources[max_tix_mix];

		// update completion time matrix
		for (int tix = 0; tix<tasks; tix++) {
			if (sched->taskLastPartMapped(tix) == true) {
				continue;
			}
			CTaskCopy* task = &((*pTasks)[tix]);
			CLogger::mainlog->debug("MaxMin: Check task %d", tix);
			bool succ = false;
			for (int six=0; six<max_task->mSuccessorNum; six++) {
				if (max_task->mpSuccessorList[six] == task->mId) {
					// successor task found
					succ = true;
					break;
				}
			}
			if (succ == true) {
				if (sched->taskDependencySatisfied(tix) == true) {
					// task was just freed from dependencies
					// update ready times
					CLogger::mainlog->debug("MaxMin: freed task %d", tix);

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

					if (task->validResource(max_res) == false) {
						continue;
					}

					// ready time for this task on this resource considering running tasks
					double ready = sched->taskReadyTimeResource(tix, max_res, mpEstimation);
					double init = mpEstimation->taskTimeInit(task, max_res);
					if (sched->taskRunningResource(tix) == max_tix_mix &&
						sched->resourceTasks(max_tix_mix) == 0) {
						// task already running on resource and
						// slot is first slot
						init = 0.0;
					}

					double compute = mpEstimation->taskTimeCompute(task, max_res, task->mProgress, task->mCheckpoints);
					double fini = mpEstimation->taskTimeFini(task, max_res);
					int nextSlot = sched->resourceTasks(max_tix_mix);
					double complete = sched->taskCompletionTime(tix, max_res, nextSlot, ready, init, compute, fini);
					C[tix*machines + max_tix_mix] = complete;

				} else {

					CResource* resA = mrResources[max_mig.parts[0].mix];
					if (task->validResource(resA) == false ) {
						continue;
					} else {

						// ready time for this task on this resource considering running tasks
						double ready = sched->taskReadyTimeResource(tix, resA, mpEstimation);
						double init = mpEstimation->taskTimeInit(task, resA);
						if (sched->taskRunningResource(tix) == max_mig.parts[0].mix &&
							sched->resourceTasks(max_mig.parts[0].mix) == 0) {
							// task already running on resource and
							// slot is first slot
							init = 0.0;
						}

						double compute = mpEstimation->taskTimeCompute(task, resA, task->mProgress, task->mCheckpoints);
						double fini = mpEstimation->taskTimeFini(task, resA);
						int nextSlot = sched->resourceTasks(resA->mId);
						double complete = sched->taskCompletionTime(tix, resA, nextSlot, ready, init, compute, fini);
						C[tix*machines + max_mig.parts[0].mix] = complete;

					}

					CResource* resB = mrResources[max_mig.parts[1].mix];
					if (task->validResource(resB) == false) {
						continue;
					} else {

						// ready time for this task on this resource considering running tasks
						double ready = sched->taskReadyTimeResource(tix, resB, mpEstimation);
						double init = mpEstimation->taskTimeInit(task, resB);
						if (sched->taskRunningResource(tix) == max_mig.parts[1].mix &&
							sched->resourceTasks(max_mig.parts[1].mix) == 0) {
							// task already running on resource and
							// slot is first slot
							init = 0.0;
						}

						double compute = mpEstimation->taskTimeCompute(task, resB, task->mProgress, task->mCheckpoints);
						double fini = mpEstimation->taskTimeFini(task, resB);
						int nextSlot = sched->resourceTasks(resB->mId);
						double complete = sched->taskCompletionTime(tix, resB, nextSlot, ready, init, compute, fini);
						C[tix*machines + max_mig.parts[1].mix] = complete;

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
