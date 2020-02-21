// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#include "CTaskWrapper.h"
#include "CLogger.h"
#include "CResource.h"
#include "CComSchedClient.h"
#include "CTaskDatabase.h"
using namespace sched::task;


const char* CTaskWrapper::stateStrings[] = {
		"PRE",
		"STARTING",
		"RUNNING",
		"STOPPING",
		"SUSPENDED",
		"POST",
		"ABORTED"
	};


CTaskWrapper::CTaskWrapper(std::string* pName,
			long size,
			long checkpoints,
			std::vector<CResource*>* pResources,
			int* pDeplist,
			int depNum,
			CComSchedClient* pClient,
			CTaskDatabase& rTaskDatabase) :
	CTask(pName,
			size,
			checkpoints,
			pResources,
			pDeplist,
			depNum),
	mrTaskDatabase(rTaskDatabase)
{

	mpClient = pClient;

}


CTaskWrapper::~CTaskWrapper(){

}


void CTaskWrapper::printEndTask(){
	long long addedl = mTimes.Added.time_since_epoch().count();
	long long startedl = mTimes.Started.time_since_epoch().count();
	long long finishedl = mTimes.Finished.time_since_epoch().count();
	long long abortedl = mTimes.Aborted.time_since_epoch().count();
	CLogger::eventlog->info("\"event\":\"ENDTASK\",\"id\":%d,\"times\":{\"added\":%ld,\"started\":%ld,\"finished\":%ld,\"aborted\":%ld},\"state\":\"%s\"",
		mId,
		addedl,
		startedl,
		finishedl,
		abortedl,
		stateStrings[mState]
		);
}


int CTaskWrapper::start(CResource& resource, int targetProgress, ETaskOnEnd onEnd) {

	CLogger::mainlog->debug("Task %d %s: start", mId, stateStrings[mState]);

	if (mState != ETaskState::PRE &&
		mState != ETaskState::SUSPENDED &&
		mState != ETaskState::RUNNING &&
		mState != ETaskState::STARTING) {
		return -1;
	}

	

	mpResource = &resource;
	this->mTimes.Started = std::chrono::steady_clock::now();
	CLogger::eventlog->info("\"event\":\"TASK_START\",\"id\":%d,\"res\":\"%s\",\"target_progress\":%d,\"on_end\":\"%s\"", mId, mpResource->mName.c_str(), targetProgress, ETaskOnEndString[onEnd]);

	if (mState != ETaskState::RUNNING) {
		mState = ETaskState::STARTING;
	}

	if (mpClient != 0) {
		int ret = mpClient->start(resource, targetProgress, onEnd, *this);
		if (ret < 0) {
			mrTaskDatabase.abortTask(this);
			return -1;
		}
	} else {
		// abort task to not block schedule
		mrTaskDatabase.abortTask(this);
		return -1;
	}
	return 0;
}


int CTaskWrapper::suspend() {

	CLogger::mainlog->debug("Task %d %s: suspend", mId, stateStrings[mState]);

	if (mState != ETaskState::RUNNING) {
		return -1;
	}

	CLogger::eventlog->info("\"event\":\"TASK_SUSPEND\",\"id\":%d", mId);
	if (mpClient != 0) {
		mState = ETaskState::STOPPING;
		int ret = mpClient->suspend(*this);
		if (ret < 0) {
			mrTaskDatabase.abortTask(this);
			return -1;
		}
	} else {
		// abort task to not block schedule
		mrTaskDatabase.abortTask(this);
		return -1;
	}

	return 0;
}


// -1->error, 0->ok
int CTaskWrapper::getProgress() {

	CLogger::mainlog->debug("Task %d %s: getProgress", mId, stateStrings[mState]);
	if (mState != ETaskState::RUNNING) {
	CLogger::mainlog->debug("Task %d: progress not running", mId);
		return -1;
	}

	if (mpClient != 0) {
		CLogger::eventlog->info("\"event\":\"TASK_GETPROGRESS\",\"id\":%d", mId);
		int ret = mpClient->progress(*this);
		CLogger::mainlog->debug("Task %d: client progress", mId);
		if (ret < 0) {
			CLogger::mainlog->debug("Task %d: progress failed", mId);
			mrTaskDatabase.abortTask(this);
			return -1;
		}
	} else {
		CLogger::mainlog->debug("Task %d: no client", mId);
		return -1;
	}
	return 0;
}


void CTaskWrapper::gotProgress(int progress) {

	CLogger::mainlog->debug("Task %d %s: gotProgress %d", mId, stateStrings[mState], progress);

	if (mState != ETaskState::RUNNING) {
		return;
	}

	mProgress = progress;
	CLogger::eventlog->info("\"event\":\"TASK_GOTPROGRESS\",\"id\":%d,\"progress\":%d", mId, progress);

}


void CTaskWrapper::finished() {

	CLogger::mainlog->debug("Task %d %s: finished", mId, stateStrings[mState]);

	if (mState != ETaskState::RUNNING &&
		mState != ETaskState::STOPPING) {
		return;
	}

	mState = ETaskState::POST;
	mTimes.Finished = std::chrono::steady_clock::now();
	CLogger::eventlog->info("\"event\":\"TASK_FINISHED\",\"id\":%d", mId);
	
}


void CTaskWrapper::started(){

	CLogger::mainlog->debug("Task %d %s: started", mId, stateStrings[mState]);

	if (mState != ETaskState::STARTING) {
		return;
	}
	mState = ETaskState::RUNNING;
	CLogger::eventlog->info("\"event\":\"TASK_STARTED\",\"id\":%d,\"res\":\"%s\"", mId, (mpResource != 0?mpResource->mName.c_str():""));

}

void CTaskWrapper::suspended(int progress){

	CLogger::mainlog->debug("Task %d %s: suspended", mId, stateStrings[mState]);

	if (mState != ETaskState::STOPPING && mState != ETaskState::RUNNING) {
		return;
	}

	mState = ETaskState::SUSPENDED;
	mProgress = progress;
	CLogger::eventlog->info("\"event\":\"TASK_SUSPENDED\",\"id\":%d,\"progress\":%d", mId, progress);
}


void CTaskWrapper::abort(){

	CLogger::mainlog->debug("Task %d %s: abort", mId, stateStrings[mState]);

	if (mState != ETaskState::POST && mState != ETaskState::ABORTED) {
		mState = ETaskState::ABORTED;
		mTimes.Aborted = std::chrono::steady_clock::now();
		CLogger::eventlog->info("\"event\":\"TASK_ABORT\",\"id\":%d", mId);
		if (mpClient != 0) {
			mpClient->abort(*this);
			mpClient = 0;
		}
	}
}


void CTaskWrapper::aborted(){

	CLogger::mainlog->debug("Task %d %s: aborted", mId, stateStrings[mState]);

	mpClient = 0;
	if (mState != ETaskState::POST && mState != ETaskState::ABORTED) {
		mState = ETaskState::ABORTED;
		mTimes.Aborted = std::chrono::steady_clock::now();
		CLogger::eventlog->info("\"event\":\"TASK_ABORTED\",\"id\":%d", mId);
	}
}


void CTaskWrapper::clientDisconnected(){

	CLogger::mainlog->debug("Task %d %s: client disconnected", mId, stateStrings[mState]);
	mpClient = 0;
	if (mState != ETaskState::POST && mState != ETaskState::ABORTED) {
		mState = ETaskState::ABORTED;
		mTimes.Aborted = std::chrono::steady_clock::now();

	}

}


// 1:ready, 0:not yet, -1:invalid (aborted dependencies)
int CTaskWrapper::dependenciesReady() {
	int ok = 1;
	for (unsigned int i = 0; i<mpDependencies.size(); i++) {
		CTask* task = mpDependencies[i];
		if (task == 0) {
			CLogger::mainlog->error("Task %d: dependency #%d is unknown", mId, i);
			continue;
		}
		if (task->mState != ETaskState::POST) {
			ok = 0;
			break;
		}
	}
	return ok;
}


CComSchedClient* CTaskWrapper::getClient(){
	return mpClient;
}


std::vector<CTaskWrapper*>& CTaskWrapper::getDependencies(){

	return mpDependencies;

}
