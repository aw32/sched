// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#include "CComUnixSchedClientMain.h"
#include "CTaskWrapper.h"
#include "CResource.h"
#include "CComUnixServer.h"
#include "CTaskDatabase.h"
#include "CScheduleComputer.h"
#include "CLogger.h"
using namespace sched::com;
using sched::com::CComUnixServer;
using sched::task::CTaskWrapper;
using sched::schedule::CResource;
using sched::task::CTaskDatabase;
using sched::schedule::CScheduleComputer;

CComUnixSchedClientMain::CComUnixSchedClientMain(
	CComUnixServer& rServer,
	std::vector<CResource*>& rResources,
	CTaskDatabase& rTaskDatabase,
	CScheduleComputer& rScheduleComputer,
	int socket
	) :
	CComUnixSchedClient(rServer, rResources, rTaskDatabase, socket),
	mrScheduleComputer(rScheduleComputer)
{
}

CComUnixSchedClientMain::~CComUnixSchedClientMain() {

	CLogger::mainlog->debug("UnixSchedClientMain: deconstruct %x", this);

	for (int i=pTasks.size()-1; i>=0; i--) {
		if (mClientQuit == 1) {
			CResource* resource = pTasks[i]->mpResource;
			if (resource != 0) {
				resource->clientDisconnected(*pTasks[i]);
			} else {
				pTasks[i]->clientDisconnected();
			}
		} else {
			CResource* resource = pTasks[i]->mpResource;
			if (resource != 0) {
				resource->taskAborted(*pTasks[i]);
			} else {
				pTasks[i]->aborted();
			}
		}
	}

}

int CComUnixSchedClientMain::start(CResource& resource, int targetProgress, ETaskOnEnd onEnd, CTaskWrapper& task) {

	CComSchedMessage* message = new CComSchedMessage();
	message->type = EComSchedMessageType::START;
	message->writer = this;
	message->resource = &resource;
	message->task = &task;
	message->targetProgress = targetProgress;
	message->onEnd = onEnd;

	CComUnixWriteMessage* umsg = dynamic_cast<CComUnixWriteMessage*> (message);
	mrServer.getWriter().addMessage(umsg);
	return 0;
}

int CComUnixSchedClientMain::abort(CTaskWrapper& task) {

	CComSchedMessage* message = new CComSchedMessage();
	message->type = EComSchedMessageType::ABORT;
	message->writer = this;
	message->task = &task;

	CComUnixWriteMessage* umsg = dynamic_cast<CComUnixWriteMessage*> (message);
	mrServer.getWriter().addMessage(umsg);
	return 0;
}

int CComUnixSchedClientMain::progress(CTaskWrapper& task) {

	CComSchedMessage* message = new CComSchedMessage();
	message->type = EComSchedMessageType::PROGRESS;
	message->writer = this;
	message->task = &task;

	CComUnixWriteMessage* umsg = dynamic_cast<CComUnixWriteMessage*> (message);
	mrServer.getWriter().addMessage(umsg);
	return 0;
}

int CComUnixSchedClientMain::suspend(CTaskWrapper& task) {

	CComSchedMessage* message = new CComSchedMessage();
	message->type = EComSchedMessageType::SUSPEND;
	message->writer = this;
	message->task = &task;

	CComUnixWriteMessage* umsg = dynamic_cast<CComUnixWriteMessage*> (message);
	mrServer.getWriter().addMessage(umsg);
	return 0;
}

int CComUnixSchedClientMain::taskids(std::vector<CTaskWrapper*>* tasks) {

	int* ids = new int[tasks->size()+1]();
	ids[tasks->size()] = -1;
	for (unsigned int i=0; i<tasks->size(); i++) {
		ids[i] = (*tasks)[i]->mId;
	}

	CComSchedMessage* message = new CComSchedMessage();
	message->type = EComSchedMessageType::TASKIDS;
	message->writer = this;
	message->taskids = ids;

	CComUnixWriteMessage* umsg = dynamic_cast<CComUnixWriteMessage*> (message);
	mrServer.getWriter().addMessage(umsg);
	return 0;
}

void CComUnixSchedClientMain::closeClient(){

	CComSchedMessage* message = new CComSchedMessage();
	message->type = EComSchedMessageType::QUIT;
	message->writer = this;

	CComUnixWriteMessage* umsg = dynamic_cast<CComUnixWriteMessage*> (message);
	mrServer.getWriter().addMessage(umsg);
}


void CComUnixSchedClientMain::onTasklist(std::vector<CTaskWrapper*>* list) {

	// register new tasklist
	pTasks.insert(pTasks.end(), list->begin(), list->end());
	int ret = mrTaskDatabase.registerTasklist(list);
	if (ret == 0) {
		taskids(list);
		mrScheduleComputer.computeSchedule();
	}

}

void CComUnixSchedClientMain::onStarted(int taskid) {

	CTaskWrapper* task = mrTaskDatabase.getTaskWrapper(taskid);
	if (task != 0) {
		CResource* resource = task->mpResource;
		if (resource != 0) {
			resource->taskStarted(*task);
		}
	}

}

void CComUnixSchedClientMain::onSuspended(int taskid, int progress) {

	CTaskWrapper* task = mrTaskDatabase.getTaskWrapper(taskid);
	if (task != 0) {
		CResource* resource = task->mpResource;
		if (resource != 0) {
			resource->taskSuspended(*task, progress);
		}
	}

}

void CComUnixSchedClientMain::onFinished(int taskid) {

	CTaskWrapper* task = mrTaskDatabase.getTaskWrapper(taskid);
	if (task != 0) {
		CResource* resource = task->mpResource;
		if (resource != 0) {
			resource->taskFinished(*task);
		}
	}

}

void CComUnixSchedClientMain::onProgress(int taskid, int progress) {

	CTaskWrapper* task = mrTaskDatabase.getTaskWrapper(taskid);
	if (task != 0) {
		CResource* resource = task->mpResource;
		if (resource != 0) {
			resource->taskProgress(*task, progress);
		}
	}

}

void CComUnixSchedClientMain::onQuit(){
}
