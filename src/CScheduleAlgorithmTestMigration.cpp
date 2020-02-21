// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#include <limits>
#include <list>
#include <algorithm>
#include "CScheduleAlgorithmTestMigration.h"
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


CScheduleAlgorithmTestMigration::CScheduleAlgorithmTestMigration(std::vector<CResource*>& rResources)
	: CScheduleAlgorithm(rResources)
{
	mpEstimation = CEstimation::getEstimation();
}

CScheduleAlgorithmTestMigration::~CScheduleAlgorithmTestMigration(){
	delete mpEstimation;
	mpEstimation = 0;
}

int CScheduleAlgorithmTestMigration::init() {
	return 0;
}

void CScheduleAlgorithmTestMigration::fini() {
}

CSchedule* CScheduleAlgorithmTestMigration::compute(std::vector<CTaskCopy>* pTasks, std::vector<CTaskCopy*>* runningTasks, volatile int* interrupt, int updated) {

	int tasks = pTasks->size();
	int machines = mrResources.size();

	CSchedule* sched = new CSchedule(tasks, machines, mrResources);
	sched->mpOTasks = pTasks;

	sched->mActiveTasks = tasks;

	if (pTasks->size() == 0) {
		sched->computeTimes();
		return sched;
	}

	CTaskCopy* task = &((*pTasks)[0]);

	// add first part
	STaskEntry* entry = new STaskEntry();
	entry->taskcopy = task;
	entry->taskid = entry->taskcopy->mId;
	entry->stopProgress = entry->taskcopy->mCheckpoints/2;
	(*(sched->mpTasks))[0]->push_back(entry);

	// add second part
	entry = new STaskEntry();
	entry->taskcopy = task;
	entry->taskid = entry->taskcopy->mId;
	entry->startProgress = entry->taskcopy->mCheckpoints/2;
	entry->stopProgress = entry->taskcopy->mCheckpoints;
	(*(sched->mpTasks))[1]->push_back(entry);

	sched->computeTimes();

	return sched;
}
