// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#include <limits>
#include <list>
#include <algorithm>
#include "CScheduleAlgorithmMCTMig.h"
#include "CTask.h"
#include "CTaskCopy.h"
#include "CEstimation.h"
#include "CSchedule.h"
#include "CLogger.h"
using namespace sched::algorithm;
using sched::task::CTask;
using sched::schedule::STaskEntry;
using sched::schedule::CSchedule;
using sched::task::ETaskState;


CScheduleAlgorithmMCTMig::CScheduleAlgorithmMCTMig(std::vector<CResource*>& rResources)
	: CScheduleAlgorithm(rResources)
{
	mpEstimation = CEstimation::getEstimation();
}

CScheduleAlgorithmMCTMig::~CScheduleAlgorithmMCTMig(){
	delete mpEstimation;
	mpEstimation = 0;
}

int CScheduleAlgorithmMCTMig::init() {
	return 0;
}

void CScheduleAlgorithmMCTMig::fini() {
}

CSchedule* CScheduleAlgorithmMCTMig::compute(std::vector<CTaskCopy>* pTasks, std::vector<CTaskCopy*>* runningTasks, volatile int* interrupt, int updated) {

	int tasks = pTasks->size();
	int machines = mrResources.size();

	CSchedule* sched = new CSchedule(tasks, machines, mrResources);
	sched->mpOTasks = pTasks;

	// generate map for global id to local id conversion
	std::unordered_map<int,int> taskmap;
	for (unsigned int i=0; i<pTasks->size(); i++) {
		taskmap[(*pTasks)[i].mId] = i;
	}


	std::chrono::steady_clock::time_point currentTime = std::chrono::steady_clock::now();

	double* pResourceReady = new double[machines]();
	// copy previous schedule
	if (mpPreviousSchedule != 0) {
		for (int mix=0; mix<machines; mix++) {
			std::vector<STaskEntry*>* mList = (*mpPreviousSchedule->mpTasks)[mix];
			CResource* res = mrResources[mix];

			for (unsigned int tix=0; tix<mList->size(); tix++) {
				STaskEntry* entry = (*mList)[tix];
				std::unordered_map<int,int>::const_iterator ix_it = taskmap.find(entry->taskid);
				if (taskmap.end() == ix_it) {
					// task already done
					continue;
				}
				int taskix = ix_it->second;

				CTaskCopy* task = &((*pTasks)[taskix]);

				STaskEntry* newentry = new STaskEntry();
				newentry->taskcopy = task;
				newentry->taskid = newentry->taskcopy->mId;
				newentry->stopProgress = newentry->taskcopy->mCheckpoints;

				if (task->mState == ETaskState::RUNNING) {

					// task is running
					sched->updateEntryProgress(newentry, task, res, currentTime, updated);

				} else {

					// task has some other state
					newentry->startProgress = task->mProgress;

				}

				(*(sched->mpTasks))[mix]->push_back(newentry);
			}
		}
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

	// update sched times
	sched->computeTimes();

	// update machine ready times
	for (int mix=0; mix<machines; mix++) {
		int mix_num = (*(sched->mpTasks))[mix]->size();
		if (mix_num == 0) {
			pResourceReady[mix] = 0.0;
		} else {
			STaskEntry* lastentry = (*(*(sched->mpTasks))[mix])[mix_num-1];
			pResourceReady[mix] = lastentry->timeFinish.count() / (double)1000000000.0;
		}
	}

	std::vector<STaskEntry*> newEntries;

	struct MCTOpt* options = new struct MCTOpt[machines];

	// map tasks
	for (unsigned int tix = 0; tix<todoTasks.size(); tix++) {
		CTaskCopy* task = todoTasks[tix];

		int depNum = task->mPredecessorNum;
		double depReady = 0.0;
		if (depNum > 0) {
			// find latest finish time for predecessors
			// assumption: all predecessor tasks are in the todoTasks/newEntries list
			for (int six=0; six<depNum; six++) {
				int sid = task->mpPredecessorList[six];
				CLogger::mainlog->debug("MCT: check dep for taskid %d sid %d", task->mId, sid);
				int sentryIx = -1;
				for (unsigned int todoIx=0; todoIx<tix; todoIx++) {
					CTaskCopy* stask = todoTasks[todoIx];
					if (stask->mId == sid) {
						sentryIx = todoIx;
						break;
					}
				}
				if (sentryIx == -1) {
					CLogger::mainlog->debug("MCT: check dep for taskid %d sid %d not found in todoTasks", task->mId, sid);
					continue;
				}
				STaskEntry* sentry = newEntries[sentryIx];
				double sfinish = sentry->timeFinish.count() / (double)1000000000.0;
				if (sfinish > depReady) {
					depReady = sfinish;
				}
			}
		}

		CLogger::mainlog->debug("MCT task id %d depReady %lf", task->mId, depReady);


		// test migration options
		MigOpt min_mig = {};
		min_mig.parts[0].mix = -1;
		
		for (int mixA = 0; mixA<machines; mixA++) {
			CResource* resA = mrResources[mixA];

			if (task->validResource(resA) == false) {
				continue;
			}

			double resReadyA = pResourceReady[mixA];
			if (depReady > resReadyA) {
				resReadyA = depReady;
			}
			for (int mixB = 0; mixB<machines; mixB++) {
				CResource* resB = mrResources[mixB];

				if (task->validResource(resB) == false) {
					continue;
				}
				
				double resReadyB = pResourceReady[mixB];
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

			double resourceReady = pResourceReady[mix];
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
			pResourceReady[options[0].mix] = options[0].complete;

			STaskEntry* entry = new STaskEntry();
			entry->taskcopy = task;
			entry->taskid = entry->taskcopy->mId;
			entry->stopProgress = entry->taskcopy->mCheckpoints;
			entry->timeFinish = std::chrono::duration<long long,std::nano>( (long long) (options[0].complete*1000000000.0));
			(*(sched->mpTasks))[options[0].mix]->push_back(entry);
			newEntries.push_back(entry);


			CLogger::mainlog->debug("MCT apply task %d mix %d start %d stop %d", task->mId, options[0].mix,
			options[0].startProgress,
			options[0].stopProgress);

		} else {

			// add migration option
			pResourceReady[min_mig.parts[0].mix] = min_mig.parts[0].complete;
			pResourceReady[min_mig.parts[1].mix] = min_mig.parts[1].complete;

			STaskEntry* entry = new STaskEntry();
			entry->taskcopy = task;
			entry->taskid = entry->taskcopy->mId;
			entry->startProgress = min_mig.parts[0].startProgress;
			entry->stopProgress = min_mig.parts[0].stopProgress;
			entry->timeFinish = std::chrono::duration<long long,std::nano>( (long long) (min_mig.parts[0].complete*1000000000.0));
			(*(sched->mpTasks))[min_mig.parts[0].mix]->push_back(entry);

			CLogger::mainlog->debug("MCT apply task %d mix %d start %d stop %d", task->mId, min_mig.parts[0].mix,
			min_mig.parts[0].startProgress,
			min_mig.parts[0].stopProgress);

			entry = new STaskEntry();
			entry->taskcopy = task;
			entry->taskid = entry->taskcopy->mId;
			entry->startProgress = min_mig.parts[1].startProgress;
			entry->stopProgress = min_mig.parts[1].stopProgress;
			entry->timeFinish = std::chrono::duration<long long,std::nano>( (long long) (min_mig.parts[1].complete*1000000000.0));
			(*(sched->mpTasks))[min_mig.parts[1].mix]->push_back(entry);

			CLogger::mainlog->debug("MCT apply task %d mix %d start %d stop %d", task->mId, min_mig.parts[1].mix,
			min_mig.parts[1].startProgress,
			min_mig.parts[1].stopProgress);

			// add final part to newEntries for dependency computation
			newEntries.push_back(entry);
			
		}

		mTaskMap[task->mId] = 1;
	}

	sched->mActiveTasks = tasks;
	mpPreviousSchedule = sched;
	delete[] pResourceReady;
	delete[] options;

	sched->computeTimes();

	return sched;
}
