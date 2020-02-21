// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#include <limits>
#include <list>
#include <algorithm>
#include <unordered_map>
#include "CScheduleAlgorithmHEFTMig2Dyn.h"
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



CScheduleAlgorithmHEFTMig2Dyn::CScheduleAlgorithmHEFTMig2Dyn(std::vector<CResource*>& rResources)
	: CScheduleAlgorithm(rResources)
{
	mpEstimation = CEstimation::getEstimation();
}

CScheduleAlgorithmHEFTMig2Dyn::~CScheduleAlgorithmHEFTMig2Dyn() {
	delete mpEstimation;
	mpEstimation = 0;
}

int CScheduleAlgorithmHEFTMig2Dyn::init() {
	return 0;
}

void CScheduleAlgorithmHEFTMig2Dyn::fini() {
}

CSchedule* CScheduleAlgorithmHEFTMig2Dyn::compute(std::vector<CTaskCopy>* pTasks, std::vector<CTaskCopy*>* runningTasks, volatile int* interrupt, int updated) {

	int tasks = pTasks->size();
	int machines = mrResources.size();

	CScheduleExt* sched = new CScheduleExt(tasks, machines, mrResources, pTasks);
	sched->setRunningTasks(runningTasks);

	// average execution costs
	double* w = new double[tasks]();
	for (int tix = 0; tix < tasks; tix++) {
		double tix_w = 0.0;
		CTask* task = &(*pTasks)[tix];
		int validmachines = 0;
		for (int mix = 0; mix < machines; mix++) {
			CResource* res = mrResources[mix];
			if (task->validResource(res) == false) {
				continue;
			}
			tix_w += mpEstimation->taskTimeInit(task, res) +
					 mpEstimation->taskTimeCompute(task, res, task->mProgress, task->mCheckpoints) +
					 mpEstimation->taskTimeFini(task, res);
			validmachines++;
		}
		w[tix] = tix_w / validmachines;
	}

	// upward ranks
	double* upward = new double[tasks]();
	int changed = 0;
	do {
		changed = 0;
		for (int tix = 0; tix < tasks; tix++) {
			if (upward[tix] != 0) {
				continue;
			}
			CTask* task = &(*pTasks)[tix];
			// check successors
			int unmarked = 0;
			double max_succ_rank = 0.0;
			for (int succIx = 0; succIx < task->mSuccessorNum; succIx++) {
				int succId = task->mpSuccessorList[succIx];
				// find local task index
				int ix = -1;
				if (sched->mTaskMap.count(succId) == 0) {
					continue;
				}
				ix = sched->mTaskMap[succId];

				CLogger::mainlog->debug("HEFT tix %d succ %d", tix, ix);
				if (upward[ix] == 0.0) {
					unmarked++;
					break;
				}
				if (upward[ix] > max_succ_rank) {
					max_succ_rank = upward[ix];
				}
			}
			if (unmarked > 0) {
				continue;
			}
			// compute upward rank
			CLogger::mainlog->debug("HEFT compute upward rank for task %d w %f max_succ_rank %f succ_num %d", tix, w[tix], max_succ_rank, task->mSuccessorNum);
			upward[tix] = w[tix] + max_succ_rank;
			changed++;
		}
		
	} while(changed > 0);

	for (int tix=0; tix<tasks; tix++) {
		CTask* task = &(*pTasks)[tix];
		CLogger::mainlog->debug("HEFT: upward rank tix %d tid %d upward %lf",tix, task->mId, upward[tix]);
	}

	std::list<int> priorityListIx;
	for (int tix = 0; tix < tasks; tix++) {
		priorityListIx.push_back(tix);
	}

	// sort by decreasing upward rank
	priorityListIx.sort( 
		[upward](int i, int j) {
			return upward[i] > upward[j];
		}
	);

	std::list<int>::iterator pit = priorityListIx.begin();
	while(pit != priorityListIx.end()) {
		int pix = (*pit);
		CTaskCopy* task = &(*pTasks)[pix];
		CLogger::mainlog->debug("HEFT: sorted upward rank tix %d tid %d upward %lf",pix, task->mId, upward[pix]);
		pit++;
	}

	int listTasks = tasks;
	// for all elements in priority list
	while (listTasks > 0) {

		int tix = priorityListIx.front();
		priorityListIx.pop_front();
		CTaskCopy* task = &(*pTasks)[tix];
		listTasks--;

		// get finish time of predecessor tasks
/*
		double preFinish = 0.0;
		preFinish = sched->taskReadyTime(task);
		// check if task is already running
		int preMix = sched->taskRunningResource(tix);
		if (preMix != -1) {
			double fini = mpEstimation->taskTimeFini(task, mrResources[preMix]);
			if (fini > preFinish) {
				preFinish = fini;
			}
		}
		CLogger::mainlog->debug("HEFT: tid %d preFinish %lf", task->mId, preFinish);
*/


		// find slot with earliest finishing time over all machines
		int eft_mix = -1;
		double finish = 0.0;
		int eft_mix_slot_index = 0;
		// iterate over every machine
		for (int mix = 0; mix < machines; mix++) {
			CResource* res = mrResources[mix];

			if (task->validResource(res) == false) {
				// task is not compatible with resource 
				continue;
			}
			double preFinish = sched->taskReadyTimeResource(tix, res, mpEstimation);
			CLogger::mainlog->debug("HEFT: preFinish tid %d preFinish %lf", task->mId, preFinish);

			// compute task duration for this machine
			// (independent of slot)
			double init = mpEstimation->taskTimeInit(task, res);
			double compute = mpEstimation->taskTimeCompute(task, res, task->mProgress, task->mCheckpoints);
			double fini = mpEstimation->taskTimeFini(task, res);
			double dur = init + compute + fini;

			// find first slot on this machine that
			// * is large enough for this task
			// * has a start time that is after the predecessor finishing time

			double slot_start = 0.0;
			double slot_stop = 0.0;
			int slot_index = sched->findSlot(mix, dur, preFinish, 0, &slot_start, &slot_stop);
			// check if task is already running and fits into first slot
			if (sched->taskRunningResource(tix) == mix &&
						sched->resourceTasks(mix) == 0) {
				double first_slot_start = 0.0;
				double first_slot_stop = 0.0;
				int first_slot_index = sched->findSlot(mix, compute + fini, preFinish, 0, &first_slot_start, &first_slot_stop);
				if (first_slot_index == 0) {
					// task actually fits into first slot, correct values
					slot_start = first_slot_start;
					slot_stop = first_slot_stop;
					slot_index = first_slot_index;
				}
			}

			CLogger::mainlog->debug("HEFT: tid %d mix %d stop %lf slot %d", task->mId, mix, slot_stop, slot_index);

			// use found slot if it is better or the first machine
			if (eft_mix == -1 || slot_stop < finish) {
				eft_mix = mix;
				finish = slot_stop;
				eft_mix_slot_index = slot_index;
			}
		}

		// check migration options
		int first_mix = -1;
		int second_mix = -1;
		double first_eft = 0.0;
		double second_eft = 0.0;
		int first_start_progress = 0;
		int first_stop_progress = 0;
		int second_start_progress = 0;
		int second_stop_progress = 0;
		int first_slot = -1;
		int second_slot = -1;
		for (int mixA = 0; mixA < machines; mixA++) {
			CResource* resA = mrResources[mixA];

			if (task->validResource(resA) == false) {
				continue;
			}

			CLogger::mainlog->debug("HEFT2Mig: mixA %d", mixA);

			double preFinish = sched->taskReadyTimeResource(tix, resA, mpEstimation);

			// compute task duration for this machine
			// (independent of slot)
			// duration for one checkpoint
			double initA = mpEstimation->taskTimeInit(task, resA);
			double oneA = mpEstimation->taskTimeCompute(task, resA, task->mProgress, task->mProgress+1);
			double finiA = mpEstimation->taskTimeFini(task, resA);
			double durOneA = initA + oneA + finiA;
			// duration for whole task on resource A
			double computeA = mpEstimation->taskTimeCompute(task, resA, task->mProgress, task->mCheckpoints);
			double durA =
				initA +
				computeA +
				finiA;

			CLogger::mainlog->debug("HEFT2Mig: mixA %d durOneA %lf durA %lf initA %lf oneA %lf finiA %lf", mixA, durOneA, durA, initA, oneA, finiA);

			// check slots, that fit at least one checkpoint
			int cur_slot_a = 0;
			int max_slot_a = (*(sched->mpTasks))[mixA]->size();
			do {
				CLogger::mainlog->debug("HEFT2Mig: mixA %d slot %d / %d", mixA, cur_slot_a, max_slot_a);
				double startTimeA = 0.0;
				double stopTimeA = 0.0;
				int new_slot_a = sched->findSlot(mixA, durOneA, preFinish, cur_slot_a, &startTimeA, &stopTimeA);

				// check if task is already running and fits into first slot
				if (cur_slot_a == 0 && sched->taskRunningResource(tix) == mixA &&
						sched->resourceTasks(mixA) == 0) {
					double first_startTimeA = 0.0;
					double first_stopTimeA = 0.0;
					int first_new_slot_a = sched->findSlot(mixA, oneA + finiA, preFinish, cur_slot_a, &first_startTimeA, &first_stopTimeA);
					if (first_new_slot_a == 0) {
						// task actually fits into first slot, correct values
						startTimeA = first_startTimeA;
						stopTimeA = first_stopTimeA;
						new_slot_a = first_new_slot_a;
						initA = 0.0;
					}
				}
				cur_slot_a = new_slot_a+1;

				//double max_slot_time = stopTimeA;
				// is there a following task?, can the slot be extended?
				if (new_slot_a < max_slot_a) {
					STaskEntry* entry = (*((*(sched->mpTasks))[mixA]))[new_slot_a];
					stopTimeA = entry->timeReady.count() / 1000000000.0;
				} else {
					// open end, choose full duration
					// same start time, change stop time
					stopTimeA = startTimeA + durA;
				}
				double slotDurA = stopTimeA - startTimeA - initA - finiA;
				// how man checkpoints fit into first slot?
				int slotPointsA =
					mpEstimation->taskTimeComputeCheckpoint(task, resA, task->mProgress, slotDurA);	

				CLogger::mainlog->debug("HEFT2Mig: mixA %d starttime %lf stoptime %lf", mixA, startTimeA, stopTimeA);

				// now check other machines for second migration part
				for (int mixB = 0; mixB < machines; mixB++) {

					// same resource - slot a is already the LAST slot
					if (mixB == mixA && new_slot_a == max_slot_a) {
						continue;
					}

					CResource* resB = mrResources[mixB];
					if (task->validResource(resB) == false) {
						continue;
					}
					CLogger::mainlog->debug("HEFT2Mig: mixA %d slot %d / %d mixB %d", mixA, new_slot_a, max_slot_a, mixB);

					double initB = mpEstimation->taskTimeInit(task, resB);
					double finiB = mpEstimation->taskTimeFini(task, resB);
					double oneB = mpEstimation->taskTimeCompute(task, resB, task->mProgress, task->mProgress+1);
					double durOneB = initB + oneB + finiB;
					double durB = mpEstimation->taskTimeCompute(task, resB, task->mProgress, task->mCheckpoints);

					int cur_slot_b = 0;

					// same resource - check slots after the first one
					if (mixB == mixA) {
						cur_slot_b = new_slot_a;
					}

					int max_slot_b = (*(sched->mpTasks))[mixB]->size();
					do {
						CLogger::mainlog->debug("HEFT2Mig: mixA %d slot %d / %d mixB %d slot %d / %d", mixA, new_slot_a, max_slot_a, mixB, cur_slot_b, max_slot_b);
						double startTimeB = 0.0;
						double stopTimeB = 0.0;
						int new_slot_b = sched->findSlot(mixB, durOneB, startTimeA, cur_slot_b, &startTimeB, &stopTimeB);
						cur_slot_b = new_slot_b + 1;
						double maxStopTimeB = stopTimeB;
						if (new_slot_b == max_slot_b) {
							// open end
							maxStopTimeB = startTimeB + durB;
						} else {
							STaskEntry* entry = (*((*(sched->mpTasks))[mixB]))[new_slot_b];
							maxStopTimeB = entry->timeReady.count() / 1000000000.0;
						}
						// first case: use complete slot a
						if (stopTimeA >= maxStopTimeB) {
							// this case makes no sense if slot b ends before slot a ends
							CLogger::mainlog->debug("HEFT2Mig: case one: astop %lf >= bmaxstop %lf", stopTimeA, maxStopTimeB);
						} else {
							// what would be the finish time?
							double readyB = startTimeB;
							if (readyB < stopTimeA) {
								readyB = stopTimeA;
							}
							double caseOneFinish = readyB + initB + finiB +
								mpEstimation->taskTimeCompute(task, resB, task->mProgress + slotPointsA, task->mCheckpoints);
							CLogger::mainlog->debug("HEFT2Mig: case one: aready %lf aeft %lf bready %lf beft %lf",
								startTimeA, stopTimeA, readyB, caseOneFinish);
							int pointsB = task->mCheckpoints - task->mProgress - slotPointsA;
							CLogger::mainlog->debug("HEFT2Mig: case one: prog %d pointsA %d pointsB %d max %d",
								task->mProgress, slotPointsA, pointsB, task->mCheckpoints);
							if (caseOneFinish <= maxStopTimeB && pointsB > 0) {
								// case one fits into slot b
								if (first_mix == -1 || second_eft > caseOneFinish) {
									first_mix = mixA;
									second_mix = mixB;
									first_eft = stopTimeA;
									second_eft = caseOneFinish;
									first_start_progress = task->mProgress;
									first_stop_progress = task->mProgress + slotPointsA;
									second_start_progress = task->mProgress + slotPointsA;
									second_stop_progress = task->mCheckpoints;
									first_slot = new_slot_a;
									second_slot = new_slot_b;
								}
							}
						}

						// second case: use as much of slot b as possible
						if (startTimeA >= startTimeB) {
							// this case makes no sense if b starts before a
							// -> simply use b without a
							CLogger::mainlog->debug("HEFT2Mig: case two: astart %lf >= bstart %lf", startTimeA, startTimeB);
							continue;
						}
						int slotPointsACaseTwo =
							mpEstimation->taskTimeComputeCheckpoint(task, resA, task->mProgress, startTimeB - startTimeA - initA - finiA);
						CLogger::mainlog->debug("HEFT2Mig: mixa time %lf results in %d points", startTimeB - startTimeA - initA - finiA, slotPointsACaseTwo);
						if (slotPointsACaseTwo < 1) {
							// no checkpoints for slot a
							CLogger::mainlog->debug("HEFT2Mig: case two: apoints %d < 1", slotPointsACaseTwo);
							continue;
						}
						if (slotPointsACaseTwo > task->mCheckpoints - task->mProgress - 1) {
							// slot a finishes task completely, no reason for migration
							CLogger::mainlog->debug("HEFT2Mig: case two: apoints %d > restpoints %d", task->mCheckpoints - task->mProgress - 1);
							continue;
						}
						double caseTwoFinish = startTimeB + initB + finiB +
							mpEstimation->taskTimeCompute(task, resB, task->mProgress + slotPointsACaseTwo, task->mCheckpoints);
						double newStopTimeA = startTimeA + initA + finiA +
							mpEstimation->taskTimeCompute(task, resA, task->mProgress, task->mProgress + slotPointsACaseTwo);
						CLogger::mainlog->debug("HEFT2Mig: newStopTimeA %lf = startTimeA %lf + initA %lf + finiA %lf + compute %lf",
							newStopTimeA, startTimeA, initA, finiA,
							mpEstimation->taskTimeCompute(task, resA, task->mProgress, task->mProgress + slotPointsACaseTwo));
						CLogger::mainlog->debug("HEFT2Mig: case two: aready %lf aeft %lf bready %lf beft %lf",
							startTimeA, newStopTimeA, startTimeB, caseTwoFinish);
						int pointsB = task->mCheckpoints - task->mProgress - slotPointsACaseTwo;
						CLogger::mainlog->debug("HEFT2Mig: case two: prog %d pointsA %d pointsB %d max %d",
							task->mProgress, slotPointsACaseTwo, pointsB, task->mCheckpoints);
						if (caseTwoFinish < maxStopTimeB && pointsB > 0) {
							// case two fits into slot b
							if (first_mix == -1 || second_eft > caseTwoFinish) {
								first_mix = mixA;
								second_mix = mixB;
								first_eft = newStopTimeA;
								second_eft = caseTwoFinish;
								first_start_progress = task->mProgress;
								first_stop_progress = task->mProgress + slotPointsACaseTwo;
								second_start_progress = task->mProgress + slotPointsACaseTwo;
								second_stop_progress = task->mCheckpoints;
								first_slot = new_slot_a;
								second_slot = new_slot_b;
							}
						}

					} while(cur_slot_b <= max_slot_b);
				} // for mixB

			} while(cur_slot_a <= max_slot_a);
		} // for mixA
		

		CLogger::mainlog->debug("HEFT: nonmig mix %d slot %d eft %lf", eft_mix, eft_mix_slot_index, finish);
		CLogger::mainlog->debug("HEFT: mig mixA %d slot %d start %d stop %d eft %lf",
			first_mix, first_slot, first_start_progress, first_stop_progress, first_eft);
		CLogger::mainlog->debug("HEFT: mig mixB %d slot %d start %d stop %d eft %lf",
			second_mix, second_slot, second_start_progress, second_stop_progress, second_eft);

		bool migrate = false;
		if (first_mix != -1 && second_eft < finish) {
			migrate = true;
		}

		if (migrate == false) {

			STaskEntry* entry = new STaskEntry();
			entry->taskcopy = task;
			entry->taskid = entry->taskcopy->mId;
			entry->startProgress = entry->taskcopy->mProgress;
			entry->stopProgress = entry->taskcopy->mCheckpoints;
			sched->addEntry(entry, mrResources[eft_mix], eft_mix_slot_index);
			CLogger::mainlog->debug("HEFT: add tid %d to mix %d slot_index %d", task->mId, eft_mix, eft_mix_slot_index);
			CLogger::mainlog->debug("HEFT: add tid %d real ready %lf real stop %lf",
				task->mId, entry->timeReady.count() / 1000000000.0, entry->timeFinish.count() / 1000000000.0);

		} else {

			STaskEntry* entry = new STaskEntry();
			entry->taskcopy = task;
			entry->taskid = entry->taskcopy->mId;
			entry->startProgress = first_start_progress;
			entry->stopProgress = first_stop_progress;
			sched->addEntry(entry, mrResources[first_mix], first_slot);
			CLogger::mainlog->debug("HEFT: add tid %d to mix %d slot_index %d #1", task->mId, first_mix, first_slot);
			CLogger::mainlog->debug("HEFT: add tid %d #1 real ready %lf real stop %lf",
				task->mId, entry->timeReady.count() / 1000000000.0, entry->timeFinish.count() / 1000000000.0);
		
			entry = new STaskEntry();
			entry->taskcopy = task;
			entry->taskid = entry->taskcopy->mId;
			entry->startProgress = second_start_progress;
			entry->stopProgress = second_stop_progress;
			sched->addEntry(entry, mrResources[second_mix], second_slot);
			CLogger::mainlog->debug("HEFT: add tid %d to mix %d slot_index %d #2", task->mId, second_mix, second_slot);
			CLogger::mainlog->debug("HEFT: add tid %d #2 real ready %lf real stop %lf",
				task->mId, entry->timeReady.count() / 1000000000.0, entry->timeFinish.count() / 1000000000.0);
		}

	}

	

	//clean up
	delete[] w;
	delete[] upward;

	sched->computeTimes();

	return sched;
}
