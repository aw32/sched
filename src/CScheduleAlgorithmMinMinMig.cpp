// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#include <limits>
#include <list>
#include <algorithm>
#include "CScheduleAlgorithmMinMinMig.h"
#include "CTask.h"
#include "CTaskCopy.h"
#include "CEstimation.h"
#include "CSchedule.h"
#include "CLogger.h"
using namespace sched::algorithm;
using sched::task::CTask;
using sched::schedule::STaskEntry;
using sched::schedule::CSchedule;



CScheduleAlgorithmMinMinMig::CScheduleAlgorithmMinMinMig(std::vector<CResource*>& rResources)
	: CScheduleAlgorithm(rResources)
{
	mpEstimation = CEstimation::getEstimation();
}

CScheduleAlgorithmMinMinMig::~CScheduleAlgorithmMinMinMig() {
	delete mpEstimation;
	mpEstimation = 0;
}

int CScheduleAlgorithmMinMinMig::init() {
	return 0;
}

void CScheduleAlgorithmMinMinMig::fini() {
}

CSchedule* CScheduleAlgorithmMinMinMig::compute(std::vector<CTaskCopy>* pTasks, std::vector<CTaskCopy*>* runningTasks, volatile int* interrupt, int updated) {

	int tasks = pTasks->size();
	int machines = mrResources.size();

	CSchedule* sched = new CSchedule(tasks, machines, mrResources);
	sched->mpOTasks = pTasks;


	// completion time matrix
	double* C = new double[tasks * machines]();
	// list of unmapped tasks
	// 0 -> unmapped
	// 1 -> mapped
	int* active = new int[tasks]();
	// machine ready times
	double* R = new double[machines]();

	int dep[tasks];
	double depReady[tasks];
	for (int tix = 0; tix<tasks; tix++) {
		CTaskCopy* task = &((*pTasks)[tix]);
		CLogger::mainlog->debug("MinMin: new tix %d taskid %d", tix, task->mId);
		depReady[tix] = 0.0;
		if (task->mPredecessorNum == 0) {
			dep[tix] = 0;
		} else {
			int found = 0;
			for (int pix=0; pix<task->mPredecessorNum; pix++) {
				int pid = task->mpPredecessorList[pix];
				// check if predecessor is in tasks list
				for (unsigned int qix=0; qix<pTasks->size(); qix++) {
					if ((*pTasks)[qix].mId == pid) {
						found++;
						break;
					}
				}
			}
			dep[tix] = found;
		}
	}
	
	// fill matrix
	for (int tix = 0; tix<tasks; tix++) {
		if (dep[tix] > 0) {
			continue;
		}
		CTask* task = (CTask*)  &((*pTasks)[tix]);
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
				R[mix] +
				mpEstimation->taskTimeInit(task, res) +
				mpEstimation->taskTimeCompute(task, res, task->mProgress, task->mCheckpoints) + 
				mpEstimation->taskTimeFini(task, res);
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
			if (active[tix] == 1) {
				continue;
			}
			// skip tasks whose predecessors are not ready
			if (dep[tix] > 0) {
				continue;
			}
			double readyTask = depReady[tix];

			CTaskCopy* task = &((*pTasks)[tix]);
			m_min_mig.parts[0].mix = -1;
			for (int mixA = 0; mixA < machines; mixA++) {
				CResource* resA = mrResources[mixA];

				if (task->validResource(resA) == false) {
					continue;
				}

				double resReadyA = R[mixA];
				if (readyTask > resReadyA) {
					resReadyA = readyTask;
				}

				for (int mixB = 0; mixB < machines; mixB++) {
					CResource* resB = mrResources[mixB];

					if (task->validResource(resB) == false) {
						continue;
					}

					double resReadyB = R[mixB];
					if (readyTask > resReadyB) {
						resReadyB = readyTask;
					}

					if (resReadyB <= resReadyA) {
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

		// task with earliest completion time
		int min_tix = -1;
		int min_tix_mix = -1;
		double min_tix_comp = std::numeric_limits<double>::max();
		CTaskCopy* min_task = 0;

		for (int tix = 0; tix<tasks; tix++) {
			// skip already mapped tasks
			if (active[tix] == 1) {
				continue;
			}
			// skip tasks whose predecessors are not ready
			if (dep[tix] > 0) {
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
			entry->timeFinish = std::chrono::duration<long long,std::nano>( (long long) (min_tix_comp*1000000000.0));
			(*(sched->mpTasks))[min_tix_mix]->push_back(entry);

			// update ready time
			double new_ready = min_tix_comp;
			R[min_tix_mix] = new_ready;

			active[min_tix] = 1;


		} else {

			// add migration option
			R[min_mig.parts[0].mix] = min_mig.parts[0].complete;
			R[min_mig.parts[1].mix] = min_mig.parts[1].complete;
			active[min_mig.tix] = 1;

			CTaskCopy* task = &((*pTasks)[min_mig.tix]);

			STaskEntry* entry = new STaskEntry();
			entry->taskcopy = task;
			entry->taskid = entry->taskcopy->mId;
			entry->startProgress = min_mig.parts[0].startProgress;
			entry->stopProgress = min_mig.parts[0].stopProgress;
			entry->timeFinish = std::chrono::duration<long long,std::nano>( (long long) (min_mig.parts[0].complete*1000000000.0));
			(*(sched->mpTasks))[min_mig.parts[0].mix]->push_back(entry);

			CLogger::mainlog->debug("MinMin apply task %d mix %d start %d stop %d", task->mId, min_mig.parts[0].mix,
			min_mig.parts[0].startProgress,
			min_mig.parts[0].stopProgress);

			entry = new STaskEntry();
			entry->taskcopy = task;
			entry->taskid = entry->taskcopy->mId;
			entry->startProgress = min_mig.parts[1].startProgress;
			entry->stopProgress = min_mig.parts[1].stopProgress;
			entry->timeFinish = std::chrono::duration<long long,std::nano>( (long long) (min_mig.parts[1].complete*1000000000.0));
			(*(sched->mpTasks))[min_mig.parts[1].mix]->push_back(entry);

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

		double new_ready = 0.0;
		if (migrate == false) {
			new_ready = min_tix_comp;
		} else {
			new_ready = min_mig.complete;
		}

		// update completion time matrix
		for (int tix = 0; tix<tasks; tix++) {
			if (active[tix] == 1) {
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
				dep[tix]--;
				if (new_ready > depReady[tix]) {
					depReady[tix] = new_ready;
				}
				if (dep[tix] == 0) {
					// task was just freed from dependencies
					// update ready times
					CLogger::mainlog->debug("MinMin: freed task %d", tix);

					for (int mix = 0; mix<machines; mix++) {
						CResource* res = mrResources[mix];

						if (task->validResource(res) == false) {
							continue;
						}

						double resourceReady = R[mix];
						double ready = (depReady[tix] > resourceReady ? depReady[tix] : resourceReady);

						// completion time =
						// ready time +
						// init time +
						// compute time +
						// fini time
						double complete = 
							ready +
							mpEstimation->taskTimeInit(task, res) +
							mpEstimation->taskTimeCompute(task, res, task->mProgress, task->mCheckpoints) + 
							mpEstimation->taskTimeFini(task, res);
						C[tix*machines + mix] = complete;
					}


				}
			} else {

				if (dep[tix] > 0) {
					continue;
				}

				if (migrate == false) {

					if (task->validResource(min_res) == false) {
						continue;
					}
					double resourceReady = new_ready;
					
					double ready = (depReady[tix] > resourceReady ? depReady[tix] : resourceReady);
					double complete = 
						ready +
						mpEstimation->taskTimeInit(task, min_res) +
						mpEstimation->taskTimeCompute(task, min_res, task->mProgress, task->mCheckpoints) + 
						mpEstimation->taskTimeFini(task, min_res);
					C[tix*machines + min_tix_mix] = complete;
				} else {

					CResource* resA = mrResources[min_mig.parts[0].mix];
					if (task->validResource(resA) == false ) {
						continue;
					} else {
						double readyA = R[min_mig.parts[0].mix];
						if (depReady[tix] > readyA) {
							readyA = depReady[tix];
						}
						double completeA = 
							readyA +
							mpEstimation->taskTimeInit(task, resA) +
							mpEstimation->taskTimeCompute(task, resA, task->mProgress, task->mCheckpoints) + 
							mpEstimation->taskTimeFini(task, resA);
						C[tix*machines + min_mig.parts[0].mix] = completeA;
					}

					CResource* resB = mrResources[min_mig.parts[1].mix];
					if (task->validResource(resB) == false) {
						continue;
					} else {
						double readyB = R[min_mig.parts[1].mix];
						if (depReady[tix] > readyB) {
							readyB = depReady[tix];
						}
						double completeB = 
							readyB +
							mpEstimation->taskTimeInit(task, resB) +
							mpEstimation->taskTimeCompute(task, resB, task->mProgress, task->mCheckpoints) + 
							mpEstimation->taskTimeFini(task, resA);
						C[tix*machines + min_mig.parts[1].mix] = completeB;
					}
				}
			}
		}
	}

	sched->mActiveTasks = tasks;
	sched->computeTimes();

	// clean up
	delete[] C;
	delete[] active;
	delete[] R;

	return sched;
}
