// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#include "CComUnixSchedClientWrap.h"
#include "CComUnixServer.h"
#include "CTaskWrapper.h"
#include "CComUnixWriteBuffer.h"
#include "CTaskDatabase.h"
#include "CUnixWrapClient.h"
using namespace sched::wrap;
using sched::com::CComUnixServer;
using sched::com::CComUnixSchedClient;
using sched::com::CComUnixWriteMessage;

CComUnixSchedClientWrap::CComUnixSchedClientWrap(
	CComUnixServer& rServer,
	std::vector<CResource*>& rResources,
	CTaskDatabase& rTaskDatabase,
	CUnixWrapClient& rWrap,
	int socket
	) :
	CComUnixSchedClient(rServer, rResources, rTaskDatabase, socket),
	mrWrap(rWrap)
{
}

CComUnixSchedClientWrap::~CComUnixSchedClientWrap() {
}

int CComUnixSchedClientWrap::start(CResource& resource, int targetProgress, ETaskOnEnd onEnd, CTaskWrapper& task) {

	CComSchedMessage* message = new CComSchedMessage();
	message->type = CComUnixSchedClient::EComSchedMessageType::START;
	message->writer = this;
	message->resource = &resource;
	message->task = &task;
	message->targetProgress = targetProgress;
	message->onEnd = onEnd;

	CComUnixWriteMessage* umsg = dynamic_cast<CComUnixWriteMessage*> (message);
	mrServer.getWriter().addMessage(umsg);
	return 0;
}

int CComUnixSchedClientWrap::suspend(CTaskWrapper& task) {

	CComSchedMessage* message = new CComSchedMessage();
	message->type = CComUnixSchedClient::EComSchedMessageType::SUSPEND;
	message->writer = this;
	message->task = &task;

	CComUnixWriteMessage* umsg = dynamic_cast<CComUnixWriteMessage*> (message);
	mrServer.getWriter().addMessage(umsg);
	return 0;
}

int CComUnixSchedClientWrap::abort(CTaskWrapper& task) {

	CComSchedMessage* message = new CComSchedMessage();
	message->type = CComUnixSchedClient::EComSchedMessageType::ABORT;
	message->writer = this;
	message->task = &task;

	CComUnixWriteMessage* umsg = dynamic_cast<CComUnixWriteMessage*> (message);
	mrServer.getWriter().addMessage(umsg);
	return 0;
}

int CComUnixSchedClientWrap::progress(CTaskWrapper& task) {
	CComSchedMessage* message = new CComSchedMessage();

	message->type = CComUnixSchedClient::EComSchedMessageType::PROGRESS;
	message->writer = this;
	message->task = &task;

	CComUnixWriteMessage* umsg = dynamic_cast<CComUnixWriteMessage*> (message);
	mrServer.getWriter().addMessage(umsg);
	return 0;
}

int CComUnixSchedClientWrap::taskids(std::vector<CTaskWrapper*>* tasks) {

	int* ids = new int[tasks->size()+1]();
	ids[tasks->size()] = -1;
	for (unsigned int i=0; i<tasks->size(); i++) {
		ids[i] = (*tasks)[i]->mId;
	}

	CComSchedMessage* message = new CComSchedMessage();
	message->type = CComUnixSchedClient::EComSchedMessageType::TASKIDS;
	message->writer = this;
	message->taskids = ids;

	CComUnixWriteMessage* umsg = dynamic_cast<CComUnixWriteMessage*> (message);
	mrServer.getWriter().addMessage(umsg);
	return 0;
}

void CComUnixSchedClientWrap::closeClient() {

	CComSchedMessage* message = new CComSchedMessage();
	message->type = CComUnixSchedClient::EComSchedMessageType::QUIT;
	message->writer = this;

	CComUnixWriteMessage* umsg = dynamic_cast<CComUnixWriteMessage*> (message);
	mrServer.getWriter().addMessage(umsg);
}

void CComUnixSchedClientWrap::onTasklist(std::vector<CTaskWrapper*>* list) {

	// register new tasklist
	int ret = mrTaskDatabase.registerTasklist(list);
	if (ret == 0) {
		taskids(list);
		mrWrap.clientTasklist(*this, list);
	} else {
		mrWrap.clientFail(*this);
	}
}

void CComUnixSchedClientWrap::onStarted(int taskid) {
	mrWrap.clientStarted(*this, taskid);
}
void CComUnixSchedClientWrap::onSuspended(int taskid, int progress) {
	mrWrap.clientSuspended(*this, taskid, progress);
}

void CComUnixSchedClientWrap::onFinished(int taskid) {
	mrWrap.clientFinished(*this, taskid);
}

void CComUnixSchedClientWrap::onProgress(int taskid, int progress) {
	mrWrap.clientProgress(*this, taskid, progress);
}

void CComUnixSchedClientWrap::onQuit() {
	mrWrap.clientQuit(*this);
}

