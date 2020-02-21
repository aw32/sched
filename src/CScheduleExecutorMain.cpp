// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#include <signal.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <cstring>
#include "CScheduleExecutorMain.h"
#include "CLogger.h"
#include "CSchedule.h"
#include "CResource.h"
#include "CTaskDatabase.h"
#include "CTask.h"
#include "CTaskWrapper.h"
#include "CScheduleComputer.h"
#include "CConfig.h"
using namespace sched::schedule;

using sched::task::ETaskState;


CScheduleExecutorMain::CScheduleExecutorMain(std::vector<CResource*>& rResources, CTaskDatabase& rTaskDatabase) :
	mrResources(rResources),
	mrTaskDatabase(rTaskDatabase)
{

	// Register at resources
	for(unsigned int i=0; i<mrResources.size(); i++) {
		mrResources[i]->setScheduleExecutor(this);
	}

	CConfig* config = CConfig::getConfig();

	int res = config->conf->getBool((char*)"executor_idle_reschedule", &mReschedule);
	if (res == -1) {
		CLogger::mainlog->warn("ScheduleExecutor: config key \"executor_idle_reschedule\" not found or not a valid boolean value");
	}
	if (mReschedule == true) {
		CLogger::mainlog->info("ScheduleExecutor: rescheduling in case of idling active");
	} else {
		CLogger::mainlog->info("ScheduleExecutor: rescheduling in case of idling not active");
	}

}

CScheduleExecutorMain::~CScheduleExecutorMain(){

	for (unsigned int i=0; i<mLastSchedules.size(); i++) {
		delete mLastSchedules[i];
	}
}

void CScheduleExecutorMain::updateSchedule(CSchedule* pSchedule){

	{
		std::lock_guard<std::mutex> lg(mMessageMutex);
		mState = EScheduleState::ACTIVE;
		if (pSchedule != 0) {
			mMessage = mMessage | ExecutorMessage::SCHEDULE;
			if (mpNewSchedule != 0) {
				delete mpNewSchedule;
			}
			mpNewSchedule = pSchedule;
			CLogger::eventlog->info("\"event\":\"EXECUTOR_NEWSCHEDULE\",\"id\":%d", pSchedule->mId);
		} else {
			mMessage = mMessage | ExecutorMessage::RESOURCE;
			CLogger::eventlog->info("\"event\":\"EXECUTOR_RESUME\"");
		}
	}
	mMessageCondVar.notify_one();

}

void CScheduleExecutorMain::suspendSchedule(){

	{
		std::lock_guard<std::mutex> lg(mMessageMutex);
		mMessage = mMessage | ExecutorMessage::RESOURCE;
		mState = EScheduleState::INACTIVE;
		CLogger::eventlog->info("\"event\":\"EXECUTOR_SUSPEND\"");
	}
	mMessageCondVar.notify_one();

}

void CScheduleExecutorMain::operationDone(){

	CLogger::mainlog->debug("operationDone");
	{
		std::lock_guard<std::mutex> lg(mMessageMutex);
		mMessage = mMessage | ExecutorMessage::RESOURCE;
	}
	mMessageCondVar.notify_one();

}

CSchedule* CScheduleExecutorMain::getCurrentSchedule() {
	return mpCurrentSchedule;
}

void CScheduleExecutorMain::setScheduleComputer(CScheduleComputer* pScheduleComputer){

	mpScheduleComputer = pScheduleComputer;

}

int CScheduleExecutorMain::start(){

	this->mThread = std::thread(&CScheduleExecutorMain::execute, this);

	return 0;

}

void CScheduleExecutorMain::stop(){
	CLogger::mainlog->debug("ScheduleExecutor going to stop thread");
	mStopThread = 1;
	{
		std::lock_guard<std::mutex> lg(mMessageMutex);
		mMessage = mMessage | ExecutorMessage::EXIT;
	}
	mMessageCondVar.notify_one();
	mLoopCondVar.notify_all();
	if (this->mThread.joinable() == true) {
		this->mThread.join();
	}
}

void CScheduleExecutorMain::execute(){

	CLogger::mainlog->debug("ScheduleExecutor: thread start");

	pid_t pid = syscall(SYS_gettid);
	CLogger::mainlog->debug("ScheduleExecutor: thread %d", pid);

	// mask signals for this thread
	int ret = 0;
	sigset_t sigset = {};
	ret = sigemptyset(&sigset);
	ret = sigaddset(&sigset, SIGINT);
	ret = sigaddset(&sigset, SIGTERM);
	//ret = sigprocmask(SIG_BLOCK, &sigset, 0);
	ret = pthread_sigmask(SIG_BLOCK, &sigset, 0);
	if (ret != 0) {
		CLogger::mainlog->error("ScheduleExecutor: thread signal error: %s", strerror(errno));
	}

	unsigned int message = 0;
	CSchedule* newSchedule = 0;

	while (mStopThread == 0) {

		// copy message
		{
			std::unique_lock<std::mutex> ul(mMessageMutex);
			while (mMessage == 0) {
				mMessageCondVar.wait(ul);
			}
			message = mMessage;
			mMessage = 0;
			newSchedule = mpNewSchedule;
			mpNewSchedule = 0;
			CLogger::mainlog->debug("ScheduleExecutor: got message %d", message);
		}
		{
			std::lock_guard<std::mutex> ul(mLoopMutex);
			// exit
			if ((message & ExecutorMessage::EXIT) != 0) {
				break;
			}

			int manageResources = 0;

			// schedule update
			if ((message & ExecutorMessage::SCHEDULE) != 0) {
				setupSchedule(newSchedule);
				manageResources = 1;
			}

			// resource update
			if ((message & ExecutorMessage::RESOURCE) != 0) {
				manageResources = 1;
			}

			if (manageResources == 1) {
				int activeResources = 0;
				activeResources = this->manageResources();
				if (mState == EScheduleState::INACTIVE && activeResources == 0) {
					mpScheduleComputer->executorSuspended();
					CLogger::eventlog->info("\"event\":\"EXECUTOR_SUSPENDED\"");
				} else {
					// all tasks done?
					bool alldone = mrTaskDatabase.tasksDone();
					if (alldone != true && activeResources == 0 && mReschedule == true) {
						// reschedule
						CLogger::eventlog->info("\"event\":\"EXECUTOR_IDLE_RESCHEDULE\"");
						mpScheduleComputer->computeSchedule();
					}
				}
			}
			mLoopCounter++;
		}
		mLoopCondVar.notify_all();

	}


	CLogger::mainlog->debug("ScheduleExecutor: thread stop");
}

void CScheduleExecutorMain::setupSchedule(CSchedule* pSchedule){
	
	mLastSchedules.push_back(pSchedule);
	mpCurrentSchedule = pSchedule;

	CLogger::mainlog->debug("ScheduleExecutor: setup new schedule");
	
}

int CScheduleExecutorMain::manageResources(){


	CLogger::mainlog->debug("ScheduleExecutor: start manage");

	int activeResources = 0;

	// iterate over all resources
	for (unsigned int resId = 0; resId<mrResources.size(); resId++) {

		CResource* resource = mrResources[resId];

		// get resource state
		STaskEntry* curTaskEntry = 0;
		int curTaskId = -1;
		CTaskWrapper* curTask = 0;
		ETaskState curTaskState = ETaskState::PRE;

		resource->getStatus(0, &curTaskEntry, &curTask, &curTaskState);
		// get task if resource is not idle, else the taskentry is 0
		if (curTaskEntry != 0) {
			curTaskId = curTaskEntry->taskid;
		}

		// get next task as planned by the schedule
		STaskEntry* nextTaskEntry = 0;
		CTaskWrapper* nextTask = 0;
		int nextTaskId = -1; // -1 -> no task planned, else task id

		if (mState == EScheduleState::ACTIVE) {
			// ACTIVE EXECUTOR
			if (mpCurrentSchedule == 0) {
				CLogger::mainlog->error("ScheduleExecutor: no current schedule");
				return 0;
			}
			// get next task
			nextTaskEntry = mpCurrentSchedule->getNextTaskEntry(resource, 0);
			if (nextTaskEntry != 0) {
				nextTaskId = nextTaskEntry->taskid;
				nextTask = mrTaskDatabase.getTaskWrapper(nextTaskId);
			}
		} else {
			// INACTIVE EXECUTOR
			// pretend there is no planned task
			// suspend current task
		}


		// has the resource the correct task?
		if (curTaskId == nextTaskId) {
			if (curTaskId != -1) {
				activeResources++;
			}
			if (curTaskEntry != nextTaskEntry) {
				// same task but differing task entries, update task entry

				CLogger::mainlog->debug("ScheduleExecutor: - %15s (%3d %4.4s) == (%3d %4.4s) (update)", resource->mName.c_str(),
					(curTaskEntry == 0?-1:curTaskId),
					(curTaskEntry == 0?"-":curTask->stateStrings[curTask->mState]),
					(nextTaskEntry == 0?-1:nextTaskId),
					(nextTaskEntry == 0?"-":nextTask->stateStrings[nextTask->mState])
				);

				resource->updateTask(nextTaskEntry, mpCurrentSchedule);

			} else {
				// do nothing

				CLogger::mainlog->debug("ScheduleExecutor: - %15s (%3d %4.4s) == (%3d %4.4s)", resource->mName.c_str(),
					(curTaskEntry == 0?-1:curTaskId),
					(curTaskEntry == 0?"-":curTask->stateStrings[curTask->mState]),
					(nextTaskEntry == 0?-1:nextTaskId),
					(nextTaskEntry == 0?"-":nextTask->stateStrings[nextTask->mState])
				);
			}
			// resource is doing the right thing, continue with next resource
			if (curTaskId == -1) {
				// resource keeps idling
				resource->idle();
			}
			continue;
		}


		if (curTaskEntry == 0) { // resource is idling

			// check if task is ready
			ETaskState nextTaskState = nextTask->mState;
			switch (nextTaskState) {
				// task is running on a different resource or is dead
				case ETaskState::RUNNING:
				case ETaskState::POST:
				case ETaskState::ABORTED:
					// do nothing
					CLogger::mainlog->debug("ScheduleExecutor: - %15s (%3d %4.4s) -- (%3d %4.4s) (!)", resource->mName.c_str(),
						(curTaskEntry == 0?-1:curTaskId),
						(curTaskEntry == 0?"-":curTask->stateStrings[curTask->mState]),
						(nextTaskEntry == 0?-1:nextTaskId),
						(nextTaskEntry == 0?"-":nextTask->stateStrings[nextTask->mState])
					);

				break;

				// task is fresh or suspended
				case ETaskState::PRE:
				case ETaskState::SUSPENDED:
					// check task dependencies
					if (nextTask->dependenciesReady() == 1 && nextTask->mProgress >= nextTaskEntry->startProgress) {
						// start task
						CLogger::mainlog->debug("ScheduleExecutor: - %15s (%3d %4.4s) -- (%3d %4.4s) (start)", resource->mName.c_str(),
							(curTaskEntry == 0?-1:curTaskId),
							(curTaskEntry == 0?"-":curTask->stateStrings[curTask->mState]),
							(nextTaskEntry == 0?-1:nextTaskId),
							(nextTaskEntry == 0?"-":nextTask->stateStrings[nextTask->mState])
						);
						activeResources++;

						CLogger::mainlog->debug("ScheduleExecutor: start next task id %d up to progress %d", nextTaskId, nextTaskEntry->stopProgress);
						resource->startTask(nextTaskEntry);
					} else {
						// not ready, wait
						CLogger::mainlog->debug("ScheduleExecutor: - %15s (%3d %4.4s) -- (%3d %4.4s) (depwait)", resource->mName.c_str(),
							(curTaskEntry == 0?-1:curTaskId),
							(curTaskEntry == 0?"-":curTask->stateStrings[curTask->mState]),
							(nextTaskEntry == 0?-1:nextTaskId),
							(nextTaskEntry == 0?"-":nextTask->stateStrings[nextTask->mState])
						);
						activeResources++;
						resource->idle();
					}
				break;
				case ETaskState::STARTING:
				case ETaskState::STOPPING:
				// nothing to do
					CLogger::mainlog->debug("ScheduleExecutor: - %15s (%3d %4.4s) -- (%3d %4.4s)", resource->mName.c_str(),
						(curTaskEntry == 0?-1:curTaskId),
						(curTaskEntry == 0?"-":curTask->stateStrings[curTask->mState]),
						(nextTaskEntry == 0?-1:nextTaskId),
						(nextTaskEntry == 0?"-":nextTask->stateStrings[nextTask->mState])
					);

				break;
			}

		} else {

			// what is the current task doing?
			switch (curTaskState) {
				// resource is starting/running the wrong task
				case ETaskState::STARTING:
				case ETaskState::RUNNING:
					// suspend the wrong task
					CLogger::mainlog->debug("ScheduleExecutor: - %15s (%3d %4.4s) -- (%3d %4.4s) (suspend)", resource->mName.c_str(),
						(curTaskEntry == 0?-1:curTaskId),
						(curTaskEntry == 0?"-":curTask->stateStrings[curTask->mState]),
						(nextTaskEntry == 0?-1:nextTaskId),
						(nextTaskEntry == 0?"-":nextTask->stateStrings[nextTask->mState])
					);
					resource->suspendTask();
					activeResources++;
				break;
				// resource is stopping the wrong task
				case ETaskState::STOPPING:
					// waiting for IDLE state 
					CLogger::mainlog->debug("ScheduleExecutor: - %15s (%3d %4.4s) -- (%3d %4.4s) (waitforidle)", resource->mName.c_str(),
						(curTaskEntry == 0?-1:curTaskId),
						(curTaskEntry == 0?"-":curTask->stateStrings[curTask->mState]),
						(nextTaskEntry == 0?-1:nextTaskId),
						(nextTaskEntry == 0?"-":nextTask->stateStrings[nextTask->mState])
					);

					activeResources++;
				break;

				case ETaskState::PRE:
				case ETaskState::SUSPENDED:
				case ETaskState::POST:
				case ETaskState::ABORTED:
				// curTaskEntry should be 0 in these cases
					CLogger::mainlog->debug("ScheduleExecutor: - %15s (%3d %4.4s) -- (%3d %4.4s) (oops)", resource->mName.c_str(),
						(curTaskEntry == 0?-1:curTaskId),
						(curTaskEntry == 0?"-":curTask->stateStrings[curTask->mState]),
						(nextTaskEntry == 0?-1:nextTaskId),
						(nextTaskEntry == 0?"-":nextTask->stateStrings[nextTask->mState])
					);
				break;
			}
		}

		// call post manage function
		resource->postManage();

	}
	CLogger::mainlog->debug("ScheduleExecutor: stop manage");

	return activeResources;
}

int CScheduleExecutorMain::getCurrentLoop(){
	{
		std::unique_lock<std::mutex> ul(mLoopMutex);
		return mLoopCounter;
	}
}

int CScheduleExecutorMain::getNextLoop(int current){

	int counter = 0;
	{
		std::unique_lock<std::mutex> ul(mLoopMutex);
		while (current == mLoopCounter && mStopThread != 1) {
			mLoopCondVar.wait(ul);
		}
		counter = mLoopCounter;
	}
	return counter;
}
