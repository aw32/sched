// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#include <limits>
#include <list>
#include <algorithm>
#include "CScheduleAlgorithmReMinMin.h"
#include "CTask.h"
#include "CTaskCopy.h"
#include "CEstimation.h"
#include "CSchedule.h"
#include "CLogger.h"
using namespace sched::algorithm;
using sched::task::CTask;
using sched::schedule::STaskEntry;
using sched::schedule::CSchedule;



CScheduleAlgorithmReMinMin::CScheduleAlgorithmReMinMin(std::vector<CResource*>& rResources)
	: CScheduleAlgorithm(rResources)
{
	mpEstimation = CEstimation::getEstimation();
}

CScheduleAlgorithmReMinMin::~CScheduleAlgorithmReMinMin() {
	delete mpEstimation;
	mpEstimation = 0;
}


int CScheduleAlgorithmReMinMin::init() {
	return 0;
}

void CScheduleAlgorithmReMinMin::fini() {
}

CSchedule* CScheduleAlgorithmReMinMin::compute(std::vector<CTaskCopy>* pTasks, std::vector<CTaskCopy*>* runningTasks, volatile int* interrupt, int updated) {

	int tasks = pTasks->size();
	int machines = mrResources.size();

	CSchedule* sched = new CSchedule(tasks, machines, mrResources);
	sched->mpOTasks = pTasks;

	// task resource energy
	double* E = new double[tasks*machines]();
	// task resource execution time
	double* ETC = new double[tasks*machines]();
	// list of unmapped tasks
	// 0 -> unmapped
	// 1 -> mapped
	int* active = new int[tasks]();

	int dep[tasks];
	double depReady[tasks];
	for (int tix = 0; tix<tasks; tix++) {
		CTaskCopy* task = &((*pTasks)[tix]);
		CLogger::mainlog->debug("ReMinMin: new tix %d taskid %d", tix, task->mId);
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
					}
				}
			}
			dep[tix] = found;
		}
	}


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

	// remaining tasks
	// 0 -> remaining, 1 -> done
	int* rtasks = new int[tasks]();
	int rtasks_num = tasks;
	// machine ready times
	double* ready = new double[machines]();
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
		double min_delayed_task_time = 0.0;
		double min_makespan = 0.0;
		double min_task_energy = 0.0;
		int first = 0;
		CTaskCopy* min_task = 0;

		// find task/resource combination with smallest total energy
		for (int mix=0; mix < machines; mix++) {
			CResource* res = mrResources[mix];
			double mix_ready = ready[mix];
			for (int tix=0; tix < tasks; tix++) {

				if (rtasks[tix] == 1) {
					// skip done task
					continue;
				}
				// skip tasks whose predecessors are not ready
				if (dep[tix] > 0) {
					continue;
				}

				CTaskCopy* task = &((*pTasks)[tix]);

				if (task->validResource(res) == false) {
					continue;
				}

				double real_ready = (depReady[tix] > mix_ready ? depReady[tix] : mix_ready);

				// task time including delay because of dependencies
				double delayed_task_time = (depReady[tix] > mix_ready ? depReady[tix] - mix_ready + ETC[tasks*mix + tix] : ETC[tasks*mix + tix]);
				double task_time = ETC[tasks*mix + tix];
				double new_makespan = real_ready + task_time;
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
					min_delayed_task_time = delayed_task_time;
					min_makespan = new_makespan;
					min_task_energy = E_task;
CLogger::mainlog->debug("ScheduleAlgorithmReMinMin: pick task %d", tix);
					first = 1;
					min_task = task;
				}
			}
		}

		// adopt selected task/resource combination
		makespan = min_makespan;
		ready[min_mix] += min_delayed_task_time;
		E_dynamic += min_task_energy;
		rtasks[min_tix] = 1;
		rtasks_num--;

		// add entry to schedule
		CTaskCopy* task = &((*pTasks)[min_tix]);
		STaskEntry* entry = new STaskEntry();
		entry->taskcopy = task;
		entry->taskid = entry->taskcopy->mId;
		entry->stopProgress = entry->taskcopy->mCheckpoints;
		(*(sched->mpTasks))[min_mix]->push_back(entry);

		active[min_tix] = 1;

		CLogger::mainlog->debug("ScheduleAlgorithmReMinMin: add task %d to resource %d", entry->taskcopy->mId, min_mix);

		if (rtasks_num == 0) {
			break;
		}

		// update completion time matrix
		for (int tix = 0; tix<tasks; tix++) {
			if (active[tix] == 1) {
				continue;
			}
			CTaskCopy* task = &((*pTasks)[tix]);
			CLogger::mainlog->debug("ReMinMin: Check task %d", tix);
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
				if (ready[min_mix] > depReady[tix]) {
					depReady[tix] = ready[min_mix];
				}
			}
		}



	}

	sched->mActiveTasks = tasks;
	sched->computeTimes();

	// clean up
	delete[] E;
	delete[] ETC;
	delete[] ready;
	delete[] rtasks;
	delete[] active;

	return sched;
}
