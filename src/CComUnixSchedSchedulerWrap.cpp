// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#include "CComUnixSchedSchedulerWrap.h"
#include "CComUnixServer.h"
#include "CUnixWrapClient.h"
#include "CComUnixWriteBuffer.h"
using namespace sched::wrap;
using sched::com::CComUnixSchedScheduler;
using sched::com::CComUnixWriteMessage;

CComUnixSchedSchedulerWrap::CComUnixSchedSchedulerWrap(
	CComUnixServer& rServer,
	std::vector<CResource*>& rResources,
	CTaskDatabase& rTaskDatabase,
	CUnixWrapClient& rWrap,
	int socket
	) :
	CComUnixSchedScheduler(rServer, rResources, rTaskDatabase, socket),
	mrWrap(rWrap)
{
}

CComUnixSchedSchedulerWrap::~CComUnixSchedSchedulerWrap() {
}

int CComUnixSchedSchedulerWrap::started(int taskid) {

	CComSchedMessage* message = new CComSchedMessage();
	message->type = CComUnixSchedScheduler::EComSchedMessageType::STARTED;
	message->writer = this;
	message->taskid = taskid;

	CComUnixWriteMessage* umsg = dynamic_cast<CComUnixWriteMessage*> (message);
	mrServer.getWriter().addMessage(umsg);
	return 0;
}

int CComUnixSchedSchedulerWrap::suspended(int taskid, int progress) {

	CComSchedMessage* message = new CComSchedMessage();
	message->type = CComUnixSchedScheduler::EComSchedMessageType::SUSPENDED;
	message->writer = this;
	message->taskid = taskid;
	message->progress = progress;

	CComUnixWriteMessage* umsg = dynamic_cast<CComUnixWriteMessage*> (message);
	mrServer.getWriter().addMessage(umsg);
	return 0;
}

int CComUnixSchedSchedulerWrap::finished(int taskid) {

	CComSchedMessage* message = new CComSchedMessage();
	message->type = CComUnixSchedScheduler::EComSchedMessageType::FINISHED;
	message->writer = this;
	message->taskid = taskid;

	CComUnixWriteMessage* umsg = dynamic_cast<CComUnixWriteMessage*> (message);
	mrServer.getWriter().addMessage(umsg);
	return 0;
}

int CComUnixSchedSchedulerWrap::progress(int taskid, int progress) {

	CComSchedMessage* message = new CComSchedMessage();
	message->type = CComUnixSchedScheduler::EComSchedMessageType::PROGRESS;
	message->writer = this;
	message->taskid = taskid;
	message->progress = progress;

	CComUnixWriteMessage* umsg = dynamic_cast<CComUnixWriteMessage*> (message);
	mrServer.getWriter().addMessage(umsg);
	return 0;
}

int CComUnixSchedSchedulerWrap::tasklist(std::vector<CTaskWrapper*>* list) {

	CComSchedMessage* message = new CComSchedMessage();
	message->type = CComUnixSchedScheduler::EComSchedMessageType::TASKLIST;
	message->writer = this;
	message->tasks = list;

	CComUnixWriteMessage* umsg = dynamic_cast<CComUnixWriteMessage*> (message);
	mrServer.getWriter().addMessage(umsg);
	return 0;
}

int CComUnixSchedSchedulerWrap::closeClient() {

	CComSchedMessage* message = new CComSchedMessage();
	message->type = CComUnixSchedScheduler::EComSchedMessageType::QUIT;
	message->writer = this;

	CComUnixWriteMessage* umsg = dynamic_cast<CComUnixWriteMessage*> (message);
	mrServer.getWriter().addMessage(umsg);
	return 0;
}





void CComUnixSchedSchedulerWrap::onTaskids(int* taskids) {
	mrWrap.serverTaskIds(*this, taskids);
}

void CComUnixSchedSchedulerWrap::onStart(int taskid, CResource& resource, int targetProgress, ETaskOnEnd onEnd) {
	mrWrap.serverTaskStart(*this, taskid, resource, targetProgress, onEnd);
}
void CComUnixSchedSchedulerWrap::onSuspend(int taskid) {
	mrWrap.serverTaskSuspend(*this, taskid);
}

void CComUnixSchedSchedulerWrap::onAbort(int taskid) {
	mrWrap.serverTaskAbort(*this, taskid);
}

void CComUnixSchedSchedulerWrap::onProgress(int taskid) {
	mrWrap.serverTaskProgress(*this, taskid);
}

void CComUnixSchedSchedulerWrap::onQuit() {
	mrWrap.serverQuit(*this);
}

void CComUnixSchedSchedulerWrap::onFail() {
	mrWrap.serverFail(*this);}
