// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#include <cstring>
#include "CScheduleAlgorithmLinear.h"
#include "CLogger.h"
#include "CResource.h"
#include "CTask.h"
#include "CTaskCopy.h"
#include "CSchedule.h"
using namespace sched::algorithm;
using sched::task::CTask;
using sched::task::ETaskState;
using sched::schedule::STaskEntry;


CScheduleAlgorithmLinear::CScheduleAlgorithmLinear(std::vector<CResource*>& rResources):
	CScheduleAlgorithm(rResources)
{
}

int CScheduleAlgorithmLinear::init(){
	return 0;
}

CSchedule* CScheduleAlgorithmLinear::compute(std::vector<CTaskCopy>* pTasks, std::vector<CTaskCopy*>* runningTasks, volatile int* interrupt, int updated){

	if (pTasks == 0) {
		return 0;
	}

	int resNum = mrResources.size();
	CSchedule* sched = new CSchedule(pTasks->size(), resNum, mrResources);
	sched->mpOTasks = pTasks;

	int resId[resNum];
	memset(resId, 0, resNum*sizeof(int));
	int count = 0;

	for (unsigned int i = 0; i<pTasks->size(); i++) {
		if ((*pTasks)[i].mState == ETaskState::POST ||
			(*pTasks)[i].mState == ETaskState::ABORTED){
			continue;
		}
		int id = count++;
		int res = id % resNum;
		resId[res]++;
		STaskEntry* entry = new STaskEntry();
		entry->taskcopy = &((*pTasks)[i]);
		entry->taskid = (*pTasks)[i].mId;
		entry->stopProgress = (*pTasks)[i].mCheckpoints;
		(*(sched->mpTasks))[res]->push_back(entry);
		CLogger::mainlog->debug("ScheduleAlgorithmLinear: schedule task %d to resource %d", entry->taskid, res);
	}
	CLogger::mainlog->debug("ScheduleAlgorithmLinear: scheduled %d tasks", count);

	sched->mActiveTasks = count;

	return sched;
}

void CScheduleAlgorithmLinear::fini(){
}

CScheduleAlgorithmLinear::~CScheduleAlgorithmLinear(){
}
