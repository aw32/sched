// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#include "CTaskDatabase.h"
#include "CLogger.h"
#include "CTaskLoader.h"
#include "CTaskWrapper.h"
#include "CTask.h"
#include "CTaskCopy.h"
#include "CResource.h"
#include "CScheduleComputer.h"
using namespace sched::task;


CTaskDatabase::CTaskDatabase(){

	mpTaskLoader = CTaskLoader::loadTaskLoader();

}

int CTaskDatabase::initialize(){

	if (mpTaskLoader == 0) {
		return -1;
	}
	int ret = mpTaskLoader->loadInfo();
	return ret;

}

CTaskDatabase::~CTaskDatabase(){

	for (unsigned int i=0; i<mTasks.size(); i++) {
		delete mTasks[i];
	}
	mTasks.clear();

	if (mpTaskLoader != 0) {
		mpTaskLoader->clearInfo();
		delete mpTaskLoader;
		mpTaskLoader = 0;
	}

}

void CTaskDatabase::setScheduleComputer(CScheduleComputer* pScheduleComputer){

	mpScheduleComputer = pScheduleComputer;

}

CTaskWrapper* CTaskDatabase::getTaskWrapper(int taskId){

	return mTaskMap[taskId];

}

int CTaskDatabase::registerTask(CTaskWrapper* task){

	// load additional information
	mpTaskLoader->getInfo(task);
	
	this->mTaskMutex.lock();

	// count application
	mAppNum += 1;

	// get task id
	task->mId = mTaskNum++;

	// add task to task list
	mTasks.push_back(task);
	mTaskMap[task->mId] = task;

	this->mTaskMutex.unlock();

	task->mTimes.Added = std::chrono::steady_clock::now();
	std::ostringstream ss;
	for (unsigned int i=0; i<task->mpResources->size(); i++) {
		ss << "\"" << (*(task->mpResources))[i]->mName.c_str() << "\"";
		if (i<task->mpResources->size()-1) {
			ss << ",";
		}
	}
	CLogger::eventlog->info("\"event\":\"NEWTASK\",\"id\":%d,\"res\":[%s],\"name\":\"%s\",\"size\":%d,\"checkpoints\":%d", task->mId, ss.str().c_str(), task->mpName->c_str(), task->mSize, task->mCheckpoints);

	mpScheduleComputer->computeSchedule();

	return 0;

}

int CTaskDatabase::registerTasklist(std::vector<CTaskWrapper*>* tasks){

	CLogger::mainlog->debug("TaskDatabase: registerTasklist");

	// load additional information
	for (unsigned int i=0; i<tasks->size(); i++) {
		mpTaskLoader->getInfo((*tasks)[i]);
	}
	
	this->mTaskMutex.lock();

	// count application
	mAppNum += 1;

	// get task ids
	for (unsigned int i=0; i<tasks->size(); i++) {
		CLogger::mainlog->debug("TaskDatabase: task %d/%d id %d", i, tasks->size(), mTaskNum);
		(*tasks)[i]->mTimes.Added = std::chrono::steady_clock::now();
		(*tasks)[i]->mId = mTaskNum++;
	}

	// adjust dependencies
	int oldId = 0;
	for (unsigned int i=0; i<tasks->size(); i++) {
		std::ostringstream ss;
		ss << "\"res\":[";
		// resources
		for (unsigned int j=0; j<(*tasks)[i]->mpResources->size(); j++) {
			ss << "\"" << (*((*tasks)[i]->mpResources))[j]->mName.c_str() << "\"";
			if (j<(*tasks)[i]->mpResources->size()-1) {
				ss << ",";
			}
		}
		ss << "],\"dep\":[";
	

		for (int j=0; j<(*tasks)[i]->mPredecessorNum; j++) {
			oldId = (*tasks)[i]->mpPredecessorList[j];
			ss << (*tasks)[oldId]->mId;
			if (j<(*tasks)[i]->mPredecessorNum -1) {
				ss << ",";
			}
			// oldId should be < tasks.size()
			(*tasks)[i]->getDependencies().push_back((*tasks)[oldId]);
			(*tasks)[i]->mpPredecessorList[j] = (*tasks)[oldId]->mId;
			(*tasks)[oldId]->mSuccessorNum++;
		}
		ss << "]";
		CLogger::eventlog->info("\"event\":\"NEWTASK\",\"id\":%d,%s,\"name\":\"%s\",\"size\":%d, \"checkpoints\":%d", (*tasks)[i]->mId, ss.str().c_str(), (*tasks)[i]->mpName->c_str(), (*tasks)[i]->mSize, (*tasks)[i]->mCheckpoints);
	}

	// fill successor list
	for (unsigned int i=0; i<tasks->size(); i++) {
		int mId = (*tasks)[i]->mId;
		int succNum = (*tasks)[i]->mSuccessorNum;
		(*tasks)[i]->mpSuccessorList = new int[succNum]();
		int nextIx = 0;
		for (unsigned int j=i+1; j<tasks->size(); j++) {
			unsigned int predNum = (*tasks)[j]->mPredecessorNum;
			int sId = (*tasks)[j]->mId;
			for (unsigned int k=0; k<predNum; k++) {
				if ((*tasks)[j]->mpPredecessorList[k] == mId) {
					(*tasks)[i]->mpSuccessorList[nextIx++] = sId;
					break;
				}
			}
			if (nextIx == succNum) {
				break;
			}
		}
	}

	// add tasks to task list
	for (unsigned int i=0; i<tasks->size(); i++) {
		mTasks.push_back((*tasks)[i]);
		CLogger::mainlog->debug("CTaskDatabase: registerTasklist: %d %p", (*tasks)[i]->mId, (*tasks)[i]);
		mTaskMap[(*tasks)[i]->mId] = (*tasks)[i];
	}

	this->mTaskMutex.unlock();

	// print
//	for (int i=0; i<mTaskNum; i++) {
//		CLogger::mainlog->info("task %d dep %d", i, mTasks[i]->mDependencies.size());
//	}

	//scheduleComputer->computeSchedule();

	return 0;
}

int CTaskDatabase::abortTask(CTaskWrapper* task){

	std::vector<CTaskWrapper*> list(1);
	list[0] = task;
	// create list of dependent tasks
	unsigned int curix = 0;
	while(curix < list.size()){
		CTaskWrapper* task = list[curix];
		
		for (unsigned int i=0; i<task->getDependencies().size(); i++){
			CTaskWrapper* newtask = task->getDependencies()[i];
			for (unsigned int j=0; j<list.size(); j++) {
				if (list[j] == newtask) {
					// already in list
					newtask = 0;
					break;
				}
			}
			if (newtask == 0) {
				list.push_back(newtask);
			}
		}
		curix++;
	}
	// abort all found tasks
	for (unsigned int i=0; i<list.size(); i++) {
		CTaskWrapper* task = list[i];
		task->abort();
	}
	// update schedule
	//scheduleComputer->computeSchedule();
	return 0;
}


void CTaskDatabase::printEndTasks(){
	for (unsigned int i=0; i<mTasks.size(); i++) {
		mTasks[i]->printEndTask();
	}
}


std::vector<CTaskCopy>* CTaskDatabase::copyUnfinishedTasks(){

	unsigned int size = mTasks.size();
	std::vector<CTaskCopy>* list = new std::vector<CTaskCopy>(size);

	unsigned int current = 0;
	for (unsigned int i=0; i<size; i++) {
		CTask* task = mTasks[i];
		if (task->mState != ETaskState::POST && task->mState != ETaskState::ABORTED) {
			CLogger::mainlog->debug("TaskDatabase: copyTasks add task %d state %d", task->mId, task->mState);
			CTaskCopy* copy = &((*list)[current]);
			CLogger::mainlog->debug("list copy CTaskCopy %p", copy);
			(*list)[current] = task;
			current++;
		}
	}

	CLogger::mainlog->debug("CTaskDatabase: copyUnfinishedTasks resize");
	list->resize(current);

	return list;

}

bool CTaskDatabase::tasksDone(){
	bool done = true;
	int num = mTasks.size();
	for (int index = 0; index < num; index++) {
		if (mTasks[index]->mState != ETaskState::POST) {
			done = false;
			break;
		}
	}
	return done;
}

int CTaskDatabase::getApplicationCount() {

	int count = 0;

	this->mTaskMutex.lock();

	count = mAppNum;

	this->mTaskMutex.unlock();

	return count;
}
