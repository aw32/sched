// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#include <limits>
#include <list>
#include <algorithm>
#include "CConfig.h"
#include "CScheduleAlgorithmKPBMig2.h"
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


CScheduleAlgorithmKPBMig2::CScheduleAlgorithmKPBMig2(std::vector<CResource*>& rResources)
	: CScheduleAlgorithm(rResources)
{
	mpEstimation = CEstimation::getEstimation();
}

CScheduleAlgorithmKPBMig2::~CScheduleAlgorithmKPBMig2() {
	delete mpEstimation;
	mpEstimation = 0;
}

int CScheduleAlgorithmKPBMig2::init() {

    CConfig* config = CConfig::getConfig();                               
    int ret = config->conf->getDouble((char*)"kpb_percentage", &mPercentage);
    if (-1 == ret) {
        CLogger::mainlog->error("ScheduleAlgorithmSA: kpb_percentage not found in config"); 
        return -1;
    }
    CLogger::mainlog->info("ScheduleAlgorithmSA: kpb_percentage: %f", mPercentage);
	CLogger::eventlog->info("\"event\":\"ALGORITHM_PARAM\",\"name\":\"kpb_percentage\",\"value\":%f", mPercentage);


	return 0;
}

void CScheduleAlgorithmKPBMig2::fini() {
}

int CScheduleAlgorithmKPBMig2::nextMETOption(struct MigOpt* options, int num) {
	int index = 0;
	double exec = 0.0;
	for (int i=0; i<num; i++) {
		if (options[i].parts[0].mix == -1) {
			// found empty spot
			return i;
		}
		if (i==0 || options[i].exec > exec) {
			// select first spot or one with worse execution time
			index = i;
			exec = options[i].exec;
		}
	}
	return index;
}

CSchedule* CScheduleAlgorithmKPBMig2::compute(std::vector<CTaskCopy>* pTasks, std::vector<CTaskCopy*>* runningTasks, volatile int* interrupt, int updated) {

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

	int best_num = (mPercentage/1.0)*machines;
	CLogger::mainlog->debug("KPB mperc %lf  machines %d best_num %d", mPercentage, machines, best_num);
	struct MigOpt* options = new struct MigOpt[best_num]();
	for (int i=0; i<best_num; i++) {
		options[i].parts[0].mix = -1;
	}

	// map tasks
	for (unsigned int tix = 0; tix<todoTasks.size(); tix++) {
		CTaskCopy* task = todoTasks[tix];

		double depReady = sched->taskReadyTime(task);

		// reset list
		for (int i=0; i<best_num; i++) {
			options[i].parts[0].mix = -1;
		}

		// Check migration options
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
				int nextOption = nextMETOption(options, best_num);
				// Add to options array if empty spot or old option is worse
				if (options[nextOption].parts[0].mix == -1 || 
					options[nextOption].exec > timeA + timeB) {
					struct MigOpt* min_mig = &(options[nextOption]);
					min_mig->complete = completeB;
					min_mig->exec = timeA + timeB;
					min_mig->parts[0].mix = mixA;
					min_mig->parts[0].startProgress = task->mProgress;
					min_mig->parts[0].stopProgress = task->mProgress+pointsA;
					min_mig->parts[0].complete = completeA;
					min_mig->parts[0].exec = timeA;
					min_mig->parts[1].mix = mixB;
					min_mig->parts[1].startProgress = task->mProgress+pointsA;
					min_mig->parts[1].stopProgress = task->mCheckpoints;
					min_mig->parts[1].complete = completeB;
					min_mig->parts[1].exec = timeB;
				}
			}
		}



		// fill arrays
		for (int mix = 0; mix<machines; mix++) {
			CResource* res = mrResources[mix];

			if (task->validResource(res) == false) {
				continue;
			}

			double resourceReady = sched->resourceReadyTime(mix);
			double ready = (depReady > resourceReady ? depReady : resourceReady);
			double exec = 
				mpEstimation->taskTimeInit(task, res) +
				mpEstimation->taskTimeCompute(task, res, task->mProgress, task->mCheckpoints) + 
				mpEstimation->taskTimeFini(task, res);
			double complete = ready + exec;
			
			int nextOption = nextMETOption(options, best_num);
			// Add to options array if empty spot or old option is worse
			CLogger::mainlog->debug("KPB option %d", nextOption);
			if (options[nextOption].parts[0].mix == -1 || 
				options[nextOption].exec > exec) {
				struct MigOpt* min_mig = &(options[nextOption]);
				min_mig->complete = complete;
				min_mig->exec = exec;
				min_mig->parts[0].mix = mix;
				min_mig->parts[0].startProgress = task->mProgress;
				min_mig->parts[0].stopProgress = task->mCheckpoints;
				min_mig->parts[0].complete = complete;
				min_mig->parts[0].exec = exec;
				min_mig->parts[1].mix = -1;
			}

		}

		// choose option with best completion time
		int index = 0;
		double complete = 0.0;
		for (int i=0; i<best_num; i++) {
			if (i==0 || (options[i].parts[0].mix != -1 && options[i].complete < complete)) {
				index = i;
				complete = options[i].complete;
			}
		}

		// add to schedule
		struct MigOpt* min_mig = &(options[index]);
		STaskEntry* entry = new STaskEntry();
		entry->taskcopy = task;
		entry->taskid = entry->taskcopy->mId;
		entry->startProgress = min_mig->parts[0].startProgress;
		entry->stopProgress = min_mig->parts[0].stopProgress;
		sched->addEntry(entry, mrResources[min_mig->parts[0].mix], -1);

		CLogger::mainlog->debug("KPB apply task %d mix %d start %d stop %d", task->mId, min_mig->parts[0].mix,
		min_mig->parts[0].startProgress,
		min_mig->parts[0].stopProgress);

		if (min_mig->parts[1].mix != -1) {
			// migration option
			entry = new STaskEntry();
			entry->taskcopy = task;
			entry->taskid = entry->taskcopy->mId;
			entry->startProgress = min_mig->parts[1].startProgress;
			entry->stopProgress = min_mig->parts[1].stopProgress;
			sched->addEntry(entry, mrResources[min_mig->parts[1].mix], -1);

			CLogger::mainlog->debug("KPB apply task %d mix %d start %d stop %d", task->mId, min_mig->parts[1].mix,
			min_mig->parts[1].startProgress,
			min_mig->parts[1].stopProgress);

		}

		mTaskMap[task->mId] = 1;
	}

	mpPreviousSchedule = sched;
	delete[] options;

	sched->computeTimes();

	return sched;
}
