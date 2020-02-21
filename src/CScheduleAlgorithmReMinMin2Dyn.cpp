// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#include <limits>
#include <list>
#include <algorithm>
#include "CScheduleAlgorithmReMinMin2Dyn.h"
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



CScheduleAlgorithmReMinMin2Dyn::CScheduleAlgorithmReMinMin2Dyn(std::vector<CResource*>& rResources)
	: CScheduleAlgorithm(rResources)
{
	mpEstimation = CEstimation::getEstimation();
}

CScheduleAlgorithmReMinMin2Dyn::~CScheduleAlgorithmReMinMin2Dyn() {
	delete mpEstimation;
	mpEstimation = 0;
}


int CScheduleAlgorithmReMinMin2Dyn::init() {
	return 0;
}

void CScheduleAlgorithmReMinMin2Dyn::fini() {
}

CSchedule* CScheduleAlgorithmReMinMin2Dyn::compute(std::vector<CTaskCopy>* pTasks, std::vector<CTaskCopy*>* runningTasks, volatile int* interrupt, int updated) {

	int tasks = pTasks->size();
	int machines = mrResources.size();

	CScheduleExt* sched = new CScheduleExt(tasks, machines, mrResources, pTasks);
	sched->setRunningTasks(runningTasks);

	// task resource energy
	double* E = new double[tasks*machines]();
	// task resource execution time
	double* ETC = new double[tasks*machines]();

	// fill arrays
	for (int tix = 0; tix < tasks; tix++) {
		CTaskCopy* task = &((*pTasks)[tix]);
		for (int mix = 0; mix < machines; mix++) {
			CResource* res = mrResources[mix];

			if (task->validResource(res) == false) {
				continue;
			}

			// compute execution time
			double init = mpEstimation->taskTimeInit(task, res);
			double compute = mpEstimation->taskTimeCompute(task, res, task->mProgress, task->mCheckpoints);
			double fini = mpEstimation->taskTimeFini(task, res);

			// compute energy
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

			double dur = init + compute + fini;
			ETC[tasks*mix + tix] = dur;

			double energy = initEnergy + computeEnergy + finiEnergy;
			E[tasks*mix + tix] = energy;

		}
	}

	// remaining tasks
	// 0 -> remaining, 1 -> done
	int rtasks_num = tasks;
	// machine ready times
	double makespan = 0.0;
	double E_dynamic = 0.0;
	double res_idle_power = 0.0;

	for (int mix=0; mix < machines; mix++) {
		CResource* res = mrResources[mix];
		res_idle_power += mpEstimation->resourceIdlePower(res);
	}
	while (rtasks_num > 0) {

		int min_mix = 0;
		int min_tix = 0;
		double min_E_total = 0.0;
		double min_makespan = 0.0;
		double min_task_energy = 0.0;
		int first = 0;

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

				double ready = sched->taskReadyTimeResource(tix, res, mpEstimation);

				// task time including delay because of dependencies
				double task_time = ETC[tasks*mix + tix];
				double new_makespan = ready + task_time;
				if (new_makespan < makespan) {
					// other resource takes longer
					new_makespan = makespan;
				}

				double E_task = E[tasks*mix + tix];
				double E_static = new_makespan * res_idle_power;
				double E_total = E_static + E_dynamic + E_task;
CLogger::mainlog->debug("ScheduleAlgorithmReMinMin: check task %d on res %d E_total %lf E_task %lf E_static %lf E_dynamic %lf", task->mId, mix, E_total, E_task, E_static, E_dynamic);

				if ((first == 0) || E_total < min_E_total) {
					// first entry or energy smallest until now
					min_mix = mix;
					min_tix = tix;
					min_E_total = E_total;
					min_makespan = new_makespan;
					min_task_energy = E_task;
CLogger::mainlog->debug("ScheduleAlgorithmReMinMin: pick task %d", tix);
					first = 1;
				}
			}
		}

		// adopt selected task/resource combination
		makespan = min_makespan;
		E_dynamic += min_task_energy;
		rtasks_num--;

		// add entry to schedule
		CTaskCopy* task = &((*pTasks)[min_tix]);
		STaskEntry* entry = new STaskEntry();
		entry->taskcopy = task;
		entry->taskid = entry->taskcopy->mId;
		entry->startProgress = entry->taskcopy->mProgress;
		entry->stopProgress = entry->taskcopy->mCheckpoints;
		sched->addEntry(entry, mrResources[min_mix], -1);

		CLogger::mainlog->debug("ScheduleAlgorithmReMinMin: add task %d to resource %d", entry->taskcopy->mId, min_mix);

		// update ETC and E for running tasks
		// for slot 0 running tasks ignore init for ETC and E
		// this changes once slot 0 is gone
		if (sched->resourceTasks(min_mix) == 1 && (*runningTasks)[min_mix] != 0) {
			CTaskCopy* task = (*runningTasks)[min_mix];
			// find tix
			int tix = -1;
			for (unsigned int i=0; i<pTasks->size(); i++) {
				if ((*pTasks)[i].mId == task->mId) {
					tix = i;
					break;
				}
			}
			if (tix != -1) {

				CResource* res = mrResources[min_mix];

				// compute execution time
				double init = mpEstimation->taskTimeInit(task, res);
				double compute = mpEstimation->taskTimeCompute(task, res, task->mProgress, task->mCheckpoints);
				double fini = mpEstimation->taskTimeFini(task, res);

				// compute energy
				double initEnergy = mpEstimation->taskEnergyInit(task, res);
				double computeEnergy = mpEstimation->taskEnergyCompute(task, res, task->mProgress, task->mCheckpoints);
				double finiEnergy = mpEstimation->taskEnergyFini(task, res);

				double dur = init + compute + fini;
				ETC[tasks*min_mix + tix] = dur;

				double energy = initEnergy + computeEnergy + finiEnergy;
				E[tasks*min_mix + tix] = energy;

			}
		}

	}

	sched->computeTimes();

	// clean up
	delete[] E;
	delete[] ETC;

	return sched;
}
