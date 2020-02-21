// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#include "CResource.h"
#include "CConfig.h"
#include "CLogger.h"
#include "CTaskDatabase.h"
#include "CTaskWrapper.h"
#include "CTaskCopy.h"
#include "CScheduleExecutor.h"
#include "CFeedback.h"
#include "CSchedule.h"
#include "ETaskOnEnd.h"
#include "CEstimation.h"
#include "CMeasure.h"
using namespace sched::schedule;
using sched::task::ETaskState;
using sched::task::ETaskOnEnd;
using sched::measure::CMeasure;

CResource::ETaskRunUntil CResource::sTaskRunUntil = ETaskRunUntil::PROGRESS_SUSPEND;

CResource::CResource(CTaskDatabase& rTaskDatabase):
	mrTaskDatabase(rTaskDatabase)
{

	mProgressTimedOutCall = std::bind(&CResource::progressTimedOut, this);

	// load task end hook

	CConfig* config = CConfig::getConfig();
	std::string* endhook_str = 0;
	const char* endhook = 0;
	int res = 0;
	res = config->conf->getString((char*)"resource_taskendhook", &endhook_str);

	if (-1 == res) {
		CLogger::mainlog->info("Resource: config key \"resource_taskendhook\" not found");
	} else {
		endhook = endhook_str->c_str();
		mpTaskEndHook = new CExternalHook((char*)endhook);
	}

}


CResource::~CResource(){

	if (mpTaskEndHook != 0) {
		delete mpTaskEndHook;
		mpTaskEndHook = 0;
	}

}

void CResource::setScheduleExecutor(CScheduleExecutor* pScheduleExecutor){

	mpScheduleExecutor = pScheduleExecutor;

}

void CResource::setFeedback(CFeedback* pFeedback){

	mpFeedback = pFeedback;

}

void CResource::getStatus(int* expectProgress, STaskEntry** taskentry, CTaskWrapper** task, ETaskState* state){

	{
		std::lock_guard<std::mutex> lg(mResourceMutex);

		if (expectProgress != 0) {
			*expectProgress = mExpectProgress;
		}
		if (taskentry != 0) {
			*taskentry = mpTaskEntry;
		}
		if (task != 0) {
			*task = mpTask;
		}
		if (state != 0) {
			if (mpTask != 0) {
				*state = mpTask->mState;
			}
		}
	}

}


void CResource::progressTimedOut(){

	{
		std::lock_guard<std::mutex> lg(mResourceMutex);

		if (mpTaskEntry != 0 && 
			mpTask->mState == ETaskState::RUNNING) {

			execSuspendTask();

		}

	}
}

void CResource::startTask(STaskEntry* taskentry){

	{
		std::lock_guard<std::mutex> lg(mResourceMutex);

		if (mpTaskEntry == 0) {

			CTaskWrapper* task = mrTaskDatabase.getTaskWrapper(taskentry->taskid);
			int started;

			switch (sTaskRunUntil) {
				case ETaskRunUntil::PROGRESS_SUSPEND:
					started = task->start(*this, taskentry->stopProgress, ETaskOnEnd::TASK_ONEND_SUSPENDS);
				break;
				case ETaskRunUntil::ESTIMATION_TIMER:
					started = task->start(*this, -1, ETaskOnEnd::TASK_ONEND_SUSPENDS);
				break;
			}

			CMeasure* measure = CMeasure::getMeasure();
			if (measure != 0) {
				measure->print();
			}

			if (started == 0) {
				// task start sent
				mpTaskEntry = taskentry;
				mpTask = task;
				if (sTaskRunUntil == ETaskRunUntil::ESTIMATION_TIMER) {
					if (taskentry->durTotal != std::chrono::steady_clock::duration::zero()) {
						mProgressTimer.set(taskentry->durTotal, mProgressTimedOutCall);
					}
				}
			} else {
				// task aborted
				mpTaskEntry->state = ETaskEntryState::ABORTED;
				mpScheduleExecutor->operationDone();
			}

		}
		else
		{
			CLogger::mainlog->error("Resource %s: got startTask while task is running, current task: %d, state: %d", mName.c_str(), (mpTaskEntry == 0?-1:mpTaskEntry->taskid), (mpTask!=0?mpTask->mState:-1));
		}

	}

}

void CResource::updateTask(STaskEntry* taskentry, CSchedule* schedule){
	CLogger::mainlog->debug("Resource %s: got update", mName.c_str());
	{
		std::lock_guard<std::mutex> lg(mResourceMutex);

		if (mpTaskEntry != 0) {


			if (mpTaskEntry->taskid == taskentry->taskid) {

				STaskEntry* oldentry = mpTaskEntry;
				// update task entry
				mpTaskEntry = taskentry;
				CTaskWrapper* task = mrTaskDatabase.getTaskWrapper(taskentry->taskid);
				mpTask = task;

				if (oldentry->stopProgress != taskentry->stopProgress) {
					// resend start message with different target checkpoint

					int started;

					switch (sTaskRunUntil) {
						case ETaskRunUntil::PROGRESS_SUSPEND:
							started = task->start(*this, taskentry->stopProgress, ETaskOnEnd::TASK_ONEND_SUSPENDS);
						break;
						case ETaskRunUntil::ESTIMATION_TIMER:
							started = task->start(*this, -1, ETaskOnEnd::TASK_ONEND_SUSPENDS);
						break;
					}
					if (started == 0) {
						// task start sent
						if (sTaskRunUntil == ETaskRunUntil::ESTIMATION_TIMER) {

							// update timer
							int oldTarget = oldentry->stopProgress;
							int newTarget = taskentry->stopProgress;
							if (oldTarget != newTarget) {
								int minTarget = oldTarget < newTarget ? oldTarget : newTarget;
								int maxTarget = oldTarget < newTarget ? newTarget : oldTarget;
								double diff = schedule->mpEst->taskTimeCompute(task, this, minTarget, maxTarget);

								if (newTarget == minTarget) {
									diff = -diff;
								}

								std::chrono::duration<long long,std::nano> ntime((long long)(diff*1000000000.0));

								mProgressTimer.updateRelative(ntime);

							}
						}
					} else {
						// task aborted
						mpTaskEntry->state = ETaskEntryState::ABORTED;
						mpScheduleExecutor->operationDone();
					}
				}

			} else {
				CLogger::mainlog->error("Resource %s: got updateTask, but update task differs from current task, current task %d, update task: %d", mName.c_str(), (mpTaskEntry == 0?-1:mpTaskEntry->taskid), (taskentry == 0?-1:taskentry->taskid));
			}
		}
		else
		{
			CLogger::mainlog->error("Resource %s: got updateTask while no task is running, updated task: %d", mName.c_str(), (taskentry == 0?-1:taskentry->taskid));
		}

	}

}

void CResource::suspendTask(){

	{
		std::lock_guard<std::mutex> lg(mResourceMutex);

		if (mpTaskEntry != 0) {

			if (mpTask->mState == ETaskState::RUNNING) {

				execSuspendTask();

			} else
			if (mpTask-> mState == ETaskState::STARTING) {

				mSuspendOnceRunning = 1;

			}
		} else {

			CLogger::mainlog->error("Resource %s: got suspendTask while no task is running", mName.c_str());
		}

	}

}

// 0-> wait, 1->done
int CResource::requestProgress(){

	{
		std::lock_guard<std::mutex> lg(mResourceMutex);

		CLogger::mainlog->debug("Resource %s: requestProgress", mName.c_str());
		if (mpTaskEntry != 0) {

			if (mpTask->mState == ETaskState::RUNNING) {
				CLogger::mainlog->debug("Resource %s: task->getProgress", mName.c_str());
				int ret = mpTask->getProgress();
				if (ret == 0) {
					CLogger::mainlog->debug("Resource %s: getProgress ok", mName.c_str());
					mExpectProgress = 1;
					// wait for progress response
					return 0;
				}
			} else
			if (mpTask->mState == ETaskState::STOPPING) {
				CLogger::mainlog->debug("Resource %s: expect progress later", mName.c_str());
				mExpectProgress = 1;
				// wait for progress in suspension/finish repsonse
				return 0;
			}
		}
	}
	// task not existing or in a state where no progress update is necessary
	return 1;
}

void CResource::taskAborted(CTaskWrapper& task){

	{
		std::lock_guard<std::mutex> lg(mResourceMutex);

		if (mpTaskEntry != 0 &&
			mpTask == &task) {

			mpTaskEntry->state = ETaskEntryState::ABORTED;
			mpTask->aborted();
			mpTaskEntry = 0;
			mpTask = 0;
			mProgressTimer.unset();
			mpScheduleExecutor->operationDone();
			mSuspendOnceRunning = 0;
			if (mExpectProgress == 1) {
				mExpectProgress = 0;
				mpFeedback->gotProgress(*this);
			}
		} else {

			CLogger::mainlog->error("Resource %s: got taskAborted for wrong task, current Task: %d, aborted Task: %d", mName.c_str(), (mpTaskEntry==0?-1:mpTaskEntry->taskid), task.mId);
			// task is not the current task of this resource
			// simply set it as aborted
			task.aborted();
		}

	}
}

void CResource::taskStarted(CTaskWrapper& task){

	{
		std::lock_guard<std::mutex> lg(mResourceMutex);

		CLogger::mainlog->debug("Resource %s: got taskStarted", mName.c_str());
		if (mpTaskEntry != 0 &&
			mpTask->mState == ETaskState::STARTING &&
			mpTask == &task) {

			mpTask->started();

			CMeasure* measure = CMeasure::getMeasure();
			if (measure != 0) {
				measure->print();
			}

			if (mSuspendOnceRunning == 1) {

				execSuspendTask();
			}
		} else {
			CLogger::mainlog->error("Resource %s: got taskStarted for wrong task, current task: %d, task that got started: %d", mName.c_str(), (mpTaskEntry == 0?-1:mpTaskEntry->taskid), task.mId);
		}
	}
}

void CResource::taskSuspended(CTaskWrapper& task, int progress){

	{
		std::lock_guard<std::mutex> lg(mResourceMutex);

		if (mpTaskEntry != 0 &&
			(mpTask->mState == ETaskState::STOPPING || mpTask->mState == ETaskState::RUNNING) &&
			mpTask == &task) {

			// task end hook
			execTaskEndHook();

			CMeasure* measure = CMeasure::getMeasure();
			if (measure != 0) {
				measure->print();
			}


			mpTask->suspended(progress);
			if ((long) mpTaskEntry->stopProgress <= progress) {
				mpTaskEntry->state = ETaskEntryState::DONE;
			}
			mpTaskEntry = 0;
			mpTask = 0;
			mpScheduleExecutor->operationDone();
			mSuspendOnceRunning = 0;
			mProgressTimer.unset();
			if (mExpectProgress == 1) {
				mExpectProgress = 0;
				mpFeedback->gotProgress(*this);
			}
		} else {
			//CLogger::mainlog->debug("Resource %s: mpTask->mState %d mpTask %x task %x", mName.c_str(), mpTask->mState, mpTask, &task);
			CLogger::mainlog->error("Resource %s: got taskSuspended for wrong task, current task: %d, task that got suspended: %d", mName.c_str(), (mpTaskEntry == 0?-1:mpTaskEntry->taskid), task.mId);
		}
	}
}

void CResource::taskProgress(CTaskWrapper& task, int progress){

	{
		std::lock_guard<std::mutex> lg(mResourceMutex);

		CLogger::mainlog->debug("Resource %s: taskProgress", mName.c_str());
		if (mpTaskEntry != 0) {
			mpTask->gotProgress(progress);
			if (mExpectProgress == 1) {
				mExpectProgress = 0;
				mpFeedback->gotProgress(*this);
			}
		} else {
			CLogger::mainlog->error("Resource %s: got taskProgress for wrong task, current task: %d, task that got progress: %d", mName.c_str(), (mpTaskEntry == 0?-1:mpTaskEntry->taskid), task.mId);
		}

	}

}

void CResource::taskFinished(CTaskWrapper& task){

	{
		std::lock_guard<std::mutex> lg(mResourceMutex);

		CLogger::mainlog->debug("Resource %s: got task finished", mName.c_str());
		if (mpTaskEntry != 0 &&
			(mpTask->mState == ETaskState::RUNNING ||
			mpTask->mState == ETaskState::STOPPING ||
			mpTask->mState == ETaskState::SUSPENDED ||
			mpTask->mState == ETaskState::STARTING) &&
			mpTask == &task) {

			// task end hook
			execTaskEndHook();

			mpTaskEntry->state = ETaskEntryState::DONE;
			mpTask->finished();
			mpTaskEntry = 0;
			mpTask = 0;
			mpScheduleExecutor->operationDone();
			mProgressTimer.unset();
			mSuspendOnceRunning = 0;
			if (mExpectProgress == 1) {
				mExpectProgress = 0;
				mpFeedback->gotProgress(*this);
			}
		} else {

			CLogger::mainlog->error("Resource %s: got taskFinished for wrong task, current task: %d, task that finished: %d", mName.c_str(), (mpTaskEntry == 0?-1:mpTaskEntry->taskid), task.mId);
		}
	}
}

void CResource::clientDisconnected(CTaskWrapper& task){

	{
		std::lock_guard<std::mutex> lg(mResourceMutex);

		if (mpTaskEntry != 0 &&
			mpTask == &task) {

			mpTask->clientDisconnected();
			mpTaskEntry->state = ETaskEntryState::ABORTED;
			mpTaskEntry = 0;
			mpTask = 0;
			mpScheduleExecutor->operationDone();
			mProgressTimer.unset();
			mSuspendOnceRunning = 0;
			if (mExpectProgress == 1) {
				mExpectProgress = 0;
				mpFeedback->gotProgress(*this);
			}
		} else {

			// this is a common case
			//CLogger::mainlog->error("Resource %s: got clientDisconnected for wrong task, current task: %d, task whose client disconnected: %d", mName.c_str(), (mpTaskEntry == 0?-1:mpTaskEntry->taskid), task.mId);
		}
	}
}

void CResource::execSuspendTask() {

	int suspended = mpTask->suspend();
	if (suspended == 0) {
		// task going to suspend
		mProgressTimer.unset();
	} else {
		// task aborted
		mpTaskEntry = 0;
		mpTask = 0;
		mpScheduleExecutor->operationDone();
	}
	mSuspendOnceRunning = 0;

}

void CResource::execTaskEndHook() {

	if (mpTaskEndHook == 0) {
		return;
	}

	if (mpTask == 0) {

		// current resource
		char* res = (char*) mName.c_str();

		// no current task
		char* arguments[] = {
			res,
			0
		};

		// execute hook
		mTaskEndHookLastStatus = mpTaskEndHook->execute(arguments);
		return;
	}


	// prepare arguments
	// get next task
	CSchedule* schedule = mpScheduleExecutor->getCurrentSchedule();
	STaskEntry* nexttask = schedule->getNextTaskEntry(this, 1);
//	// check current task, is it from this schedule?
//	STaskEntry* currentTaskEntry = schedule->getNextTaskEntry(this, 0);
//	bool nextTaskReady = false;
//	if (currentTaskEntry != 0 && mpTaskEntry->taskid != currentTaskEntry->taskid) {
//		// this task is not the correct one
//		CTaskWrapper* currentTaskWrapper = mrTaskDatabase.getTaskWrapper(currentTaskEntry->taskid);
//		if (currentTaskWrapper != 0 && currentTaskWrapper->dependenciesReady() == 1) {
//			nextTaskReady = true;
//		}
//	}
	// current resource
	char* res = (char*) mName.c_str();
	// current task name
	char* taskname = (char*) mpTask->mpName->c_str();
	// current task size
	std::ostringstream tasksize_ss;
	tasksize_ss << mpTask->mSize;
	std::string tasksize_str = tasksize_ss.str();
	char* tasksize = (char*) (tasksize_str.c_str());

	if (nexttask != 0) {

		// found next task
		// next task name
		char* nexttaskname = (char*) nexttask->taskcopy->mpName->c_str();
		// next task size
		std::ostringstream nexttasksize_ss;
		nexttasksize_ss << nexttask->taskcopy->mSize;
		std::string nexttasksize_str = nexttasksize_ss.str();
		char* nexttasksize = (char*) (nexttasksize_str.c_str());
		
		// estimated time until next task
		uint64_t nexttaskns_break = (uint64_t) mpTaskEntry->durBreak.count();

		//std::chrono::steady_clock::duration dur = time.time_since_epoch();
		//double sec = dur.count() / 1000000000.0;

		std::ostringstream break_ss;
		break_ss << nexttaskns_break;
		std::string breakstr_str = break_ss.str();
		char* breakstr = (char*) (breakstr_str.c_str());

		char* arguments[] = {
			res,
			taskname,
			tasksize,
			nexttaskname,
			nexttasksize,
			breakstr,
			0
		};

		// execute hook
		mTaskEndHookLastStatus = mpTaskEndHook->execute(arguments);

	} else {

		// no next task
		char* arguments[] = {
			res,
			taskname,
			tasksize,
			0
		};

		// execute hook
		mTaskEndHookLastStatus = mpTaskEndHook->execute(arguments);
	}

}

int CResource::loadResources(std::vector<CResource*> *list, CTaskDatabase& rTaskDatabase){

	sTaskRunUntil = ETaskRunUntil::PROGRESS_SUSPEND;

	CConfig* config = CConfig::getConfig();

	// load run until
	std::string* rununtil_str = 0;
	const char* rununtil = 0;
	int res = 0;
	res = config->conf->getString((char*)"task_rununtil", &rununtil_str);
	if (-1 == res) {
		CLogger::mainlog->warn("Resource: config key \"task_rununtil\" not found, using default: progress_suspend");
	} else {
		rununtil = rununtil_str->c_str();
		if (strcmp(rununtil,"estimation_timer") == 0) {
			sTaskRunUntil = ETaskRunUntil::ESTIMATION_TIMER;
		} else
		if (strcmp(rununtil,"progress_suspend") == 0) {
			sTaskRunUntil = ETaskRunUntil::PROGRESS_SUSPEND;
		}
	}

	switch (sTaskRunUntil) {
		case ETaskRunUntil::PROGRESS_SUSPEND:
			CLogger::mainlog->info("Resource: task runs until target progress and suspends");
		break;
		case ETaskRunUntil::ESTIMATION_TIMER:
			CLogger::mainlog->info("Resource: task runs until estimation timer");
		break;
	}

	// load resources

	int error = -1;

	std::vector<CConf*>* resource_list = 0;
	res = config->conf->getList((char*)"resources", &resource_list);
	if (-1 == res) {
		CLogger::mainlog->error("CResource: config key \"resources\" not found, no resources loaded");
		return -1;
	}
	for (unsigned int i = 0; i<resource_list->size(); i++) {
		CConf* json_res = (*resource_list)[i];
		if (json_res->mType != EConfType::Map) {
			// entry is not an object
			CLogger::mainlog->error("CResource: entry in resource list in config not an object");
			error = 0;
			break;
		}
		std::string* name_str = 0;
		res = json_res->getString((char*)"name", &name_str);
		if (-1 == res) {
			CLogger::mainlog->error("CResource: entry in resource list in config has no name");
			error = 0;
			break;
		}
		CResource* res = new CResource(rTaskDatabase);
		res->mName = *(name_str);
		res->mId = list->size();
		list->push_back(res);
	}

	if (error == 0) {
		// clean up list
		while (list->size() > 0) {
			delete list->back();
			list->pop_back();
		}
		return -1;
	}

	return 0;

}

void CResource::postManage() {

}


void CResource::idle() {
	
	if (mName.compare("MaxelerVectis") == 0) {
		// FPGA resource

		if (mTaskEndHookLastStatus != 0) {
			// last hook was not successful

			{
				std::lock_guard<std::mutex> lg(mResourceMutex);

				execTaskEndHook();
			}
			
		}
	}
}
