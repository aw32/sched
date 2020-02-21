// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#include <limits>
#include <list>
#include <algorithm>
#include "CScheduleAlgorithmReMinMinMig2Dyn.h"
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



CScheduleAlgorithmReMinMinMig2Dyn::CScheduleAlgorithmReMinMinMig2Dyn(std::vector<CResource*>& rResources)
	: CScheduleAlgorithm(rResources)
{
	mpEstimation = CEstimation::getEstimation();
}

CScheduleAlgorithmReMinMinMig2Dyn::~CScheduleAlgorithmReMinMinMig2Dyn() {
	delete mpEstimation;
	mpEstimation = 0;
}


int CScheduleAlgorithmReMinMinMig2Dyn::init() {
	return 0;
}

void CScheduleAlgorithmReMinMinMig2Dyn::fini() {
}

CSchedule* CScheduleAlgorithmReMinMinMig2Dyn::compute(std::vector<CTaskCopy>* pTasks, std::vector<CTaskCopy*>* runningTasks, volatile int* interrupt, int updated) {

	int tasks = pTasks->size();
	int machines = mrResources.size();

	CScheduleExt* sched = new CScheduleExt(tasks, machines, mrResources, pTasks);
	sched->setRunningTasks(runningTasks);

	// task resource energy
	//double* E = new double[tasks*machines]();
	// task resource execution time
	//double* ETC = new double[tasks*machines]();

/*
	// fill arrays
	for (int tix = 0; tix < tasks; tix++) {
		CTaskCopy* task = &((*pTasks)[tix]);
		for (int mix = 0; mix < machines; mix++) {
			CResource* res = mrResources[mix];

			if (task->validResource(res) == false) {
				continue;
			}

			// compute execution time
			double dur =
				mpEstimation->taskTimeInit(task, res) +
				mpEstimation->taskTimeCompute(task, res, task->mProgress, task->mCheckpoints) +
				mpEstimation->taskTimeFini(task, res);

			ETC[tasks*mix + tix] = dur;

			// compute energy
			double energy =
				mpEstimation->taskEnergyInit(task, res) +
				mpEstimation->taskEnergyCompute(task, res, task->mProgress, task->mCheckpoints) +
				mpEstimation->taskEnergyFini(task, res);

			E[tasks*mix + tix] = energy;

		}
	}
*/

	// remaining tasks
	int rtasks_num = tasks;
	// machine ready times
	double makespan = 0.0;
	double E_dynamic = 0.0;
	double res_idle_power = 0.0;

	for (int mix=0; mix < machines; mix++) {
		CResource* res = mrResources[mix];
		res_idle_power += mpEstimation->resourceIdlePower(res);
	}

	// option with minimal total energy
	struct MigOpt min_mig = {};

	while (rtasks_num > 0) {

		min_mig.parts[0].mix = -1;

		// check migration options
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

			for (int mixA = 0; mixA < machines; mixA++) {
				CResource* resA = mrResources[mixA];

				if (task->validResource(resA) == false) {
					continue;
				}

				double resReadyA = sched->taskReadyTimeResource(tix, resA, mpEstimation);
				if (readyTask > resReadyA) {
					resReadyA = readyTask;
				}

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

					CLogger::mainlog->debug("ReMinMin: possible mig taskid %d mixA %d mixB %d readyA %lf readyB %lf completeA %lf completeB %lf",
						task->mId,
						mixA,
						mixB,
						resReadyA,
						resReadyB,
						completeA,
						completeB
						);

					// compute energy values
					double new_makespan = completeB;
					if (new_makespan < makespan) {
						// other resource takes longer
						new_makespan = makespan;
					}

					double initEnergyA = mpEstimation->taskEnergyInit(task, resA);
					if (initA == 0.0) {
						// init time is 0 because task already running
						initEnergyA = 0.0;
					}
					double computeEnergyA = mpEstimation->taskEnergyCompute(task, resA, task->mProgress, task->mProgress+pointsA);
					double finiEnergyA = mpEstimation->taskEnergyFini(task, resA);
					double energyA = initEnergyA + computeEnergyA + finiEnergyA;
					double initEnergyB = mpEstimation->taskEnergyInit(task, resB);
					double computeEnergyB = mpEstimation->taskEnergyCompute(task, resB, task->mProgress+pointsA, task->mCheckpoints);
					double finiEnergyB = mpEstimation->taskEnergyFini(task, resB);
					double energyB = initEnergyB + computeEnergyB + finiEnergyB;

					double E_static = new_makespan * res_idle_power;
					double E_total = E_static + E_dynamic + energyA + energyB;

					
					CLogger::mainlog->debug("ReMinMin: check task %d on res %d and res %d E_total %lf E_task %lf E_static %lf E_dynamic %lf", task->mId, mixA, mixB, E_total, energyA+energyB, E_static, E_dynamic);

					if (min_mig.parts[0].mix == -1 || 
						min_mig.energyTotal > E_total) {

						min_mig.tix = tix;
						min_mig.makespan = new_makespan;
						min_mig.energyTotal = E_total;
						min_mig.energyDynamic = energyA + energyB;
						min_mig.complete = completeB;
						min_mig.parts[0].mix = mixA;
						min_mig.parts[0].startProgress = task->mProgress;
						min_mig.parts[0].stopProgress = task->mProgress+pointsA;
						min_mig.parts[0].complete = completeA;
						min_mig.parts[1].mix = mixA;
						min_mig.parts[1].startProgress = task->mProgress+pointsA;
						min_mig.parts[1].stopProgress = task->mCheckpoints;
						min_mig.parts[1].complete = completeB;
					}
				} // end for mixB
			} // end for mixA
		} // end for tix
					



		// find task/resource combination with smallest total energy
		for (int mix=0; mix < machines; mix++) {
			CResource* res = mrResources[mix];
			for (int tix=0; tix < tasks; tix++) {

				if (sched->taskLastPartMapped(tix) == true) {
					// skip done task
					continue;
				}
				// skip tasks whose predecessors are not ready
				if (sched->taskDependencySatisfied(tix) == false) {
					continue;
				}

				CTaskCopy* task = &((*pTasks)[tix]);

				if (task->validResource(res) == false) {
					continue;
				}

				// ready time for this task on this resource considering running tasks
				double ready = sched->taskReadyTimeResource(tix, res, mpEstimation);


				double init = mpEstimation->taskTimeInit(task, res);
				double compute = mpEstimation->taskTimeCompute(task, res, task->mProgress, task->mCheckpoints);
				double fini = mpEstimation->taskTimeFini(task, res);

				double initEnergy = mpEstimation->taskEnergyInit(task, res);
				double computeEnergy = mpEstimation->taskEnergyCompute(task, res, task->mProgress, task->mCheckpoints);
				double finiEnergy = mpEstimation->taskEnergyFini(task, res);
				if (sched->taskRunningResource(tix) == mix &&
					sched->resourceTasks(mix) == 0) {
					// task already running on resource and
					// slot is first slot
					init = 0.0;
					initEnergy = 0.0;
				}

				double complete = ready + init + compute + fini;
				double new_makespan = complete;
				if (new_makespan < makespan) {
					// other resource takes longer
					new_makespan = makespan;
				}

				double E_task = initEnergy + computeEnergy + finiEnergy;
				double E_static = new_makespan * res_idle_power;
				double E_total = E_static + E_dynamic + E_task;
CLogger::mainlog->debug("ScheduleAlgorithmReMinMin: check task %d on res %d E_total %lf E_task %lf E_static %lf E_dynamic %lf", task->mId, mix, E_total, E_task, E_static, E_dynamic);

				if (min_mig.parts[0].mix == -1 || 
					min_mig.energyTotal > E_total) {

					min_mig.tix = tix;
					min_mig.makespan = new_makespan;
					min_mig.energyTotal = E_total;
					min_mig.energyDynamic = E_task;
					min_mig.complete = complete;
					min_mig.parts[0].mix = mix;
					min_mig.parts[0].startProgress = task->mProgress;
					min_mig.parts[0].stopProgress = task->mCheckpoints;
					min_mig.parts[0].complete = complete;
					min_mig.parts[1].mix = -1;


CLogger::mainlog->debug("ScheduleAlgorithmReMinMin: pick task %d", tix);
				}
			} // end for tix
		} // end for mix

		// adopt selected task/resource combination
		makespan = min_mig.makespan;
		E_dynamic += min_mig.energyDynamic;
		rtasks_num--;

		// add entry to schedule

		CTaskCopy* task = &((*pTasks)[min_mig.tix]);

		STaskEntry* entry = new STaskEntry();
		entry->taskcopy = task;
		entry->taskid = entry->taskcopy->mId;
		entry->startProgress = min_mig.parts[0].startProgress;
		entry->stopProgress = min_mig.parts[0].stopProgress;
		sched->addEntry(entry, mrResources[min_mig.parts[0].mix], -1);

		CLogger::mainlog->debug("ReMinMin apply task %d mix %d start %d stop %d", task->mId, min_mig.parts[0].mix,
		min_mig.parts[0].startProgress,
		min_mig.parts[0].stopProgress);

		// migration option
		if (min_mig.parts[1].mix != -1) {

			STaskEntry* entry = new STaskEntry();
			entry->taskcopy = task;
			entry->taskid = entry->taskcopy->mId;
			entry->startProgress = min_mig.parts[1].startProgress;
			entry->stopProgress = min_mig.parts[1].stopProgress;
			sched->addEntry(entry, mrResources[min_mig.parts[1].mix], -1);
			CLogger::mainlog->debug("ReMinMin apply task %d mix %d start %d stop %d", task->mId, min_mig.parts[1].mix,
			min_mig.parts[1].startProgress,
			min_mig.parts[1].stopProgress);
		}

	}

	sched->computeTimes();

	// clean up
	//delete[] E;
	//delete[] ETC;

	return sched;
}
