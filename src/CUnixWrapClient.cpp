// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include "CUnixWrapClient.h"
#include "CWrapTaskGroup.h"
#include "CTaskWrapper.h"
#include "CComUnixClient.h"
#include "CComUnixServer.h"
#include "CComUnixSchedSchedulerWrap.h"
#include "CComSchedClient.h"
#include "CLogger.h"
#include "CWrapMain.h"
using namespace sched::wrap;
using sched::com::CComSchedClient;
using sched::com::CComUnixClient;


CUnixWrapClient::CUnixWrapClient(CTaskDefinitions& definitions, CWrapTaskGroup& group, std::vector<CResource*>& rResources, CTaskDatabase& rTaskDatabase) :
	mrResources(rResources),
	mrDefinitions(definitions),
	mrTaskDatabase(rTaskDatabase),
	mrGroup(group)
{
	mTotalApps = mrGroup.mTaskApps.size();

}

CUnixWrapClient::~CUnixWrapClient() {

}

void CUnixWrapClient::setServer(CComUnixServer* pServer) {

	mpServer = pServer;

}

int CUnixWrapClient::initialize() {

	char* schedSocket = std::getenv("WRAP_SOCKET");
	if (schedSocket == 0) {
		CLogger::mainlog->error("WrapClient: WRAP_SOCKET undefined");
		return -1;
	}
	mpSocketPath = schedSocket;

	char* logPrefix = std::getenv("WRAP_LOGPREFIX");
	if (logPrefix == 0) {
		CLogger::mainlog->error("WrapClient: WRAP_LOGPREFIX undefined");
		return -1;
	}
	mpLogPrefix = logPrefix;


	CLogger::mainlog->debug("WrapClient: initiliaze %d apps", mTotalApps);

	while (mErrorApps == 0 && mRegisteredApps < mTotalApps) {

		int nextAppIndex = mRegisteredApps;

		CWrapTaskApp* app = mrGroup.mTaskApps[nextAppIndex];

		CLogger::mainlog->debug("WrapClient: start app %s", app->name);

		{
			std::unique_lock<std::mutex> ul(mWrapMutex);

			if (app->exec == true) {
				int ret = app->start(nextAppIndex, mpLogPrefix, mpSocketPath);
				if (ret == -1) {
					CLogger::mainlog->debug("WrapClient: app failed");
					mErrorApps++;
					break;
				}
			}

			int newApps = mRegisteredApps+1;
			// wait for app response or error
			while(mErrorApps == 0 && mRegisteredApps < newApps) {
				std::cv_status to_status = std::cv_status::no_timeout;
				to_status = mWrapCondVar.wait_for(ul, std::chrono::seconds(5));
				if (to_status == std::cv_status::timeout) {
					// timeout after 5 seconds
					// check if app died
					int joined = app->joinIfDead();
					if (joined == 0) {
						// app exited on startup
						mErrorApps += 1;
						CLogger::mainlog->error("WrapClient: app failed after startup");
					}
				}
			}
		}
	}

	if (mErrorApps > 0) {
		CLogger::mainlog->debug("WrapClient: error apps %d", mErrorApps);
		return -1;
	}
	return 0;
}

int CUnixWrapClient::registerGroup() {

	{
		std::unique_lock<std::mutex> ul(mWrapMutex);

		// send tasklist
		CComUnixSchedSchedulerWrap* client = connectToScheduler();
		if (client == 0) {
			return -1;
		}
		schedulerClient = client;
		std::vector<CTaskWrapper*>* tasks = new std::vector<CTaskWrapper*>();
		tasks->insert(tasks->end(), allTasks.begin(), allTasks.end());
		client->tasklist(tasks);
		// wait for response
		while (registered != true) {
			mWrapCondVar.wait(ul);	
		}
	}
	return 0;
}

CComUnixSchedSchedulerWrap* CUnixWrapClient::connectToScheduler() {

	char* schedSocket = std::getenv("SCHED_SOCKET");
	if (schedSocket == 0) {
		CLogger::mainlog->error("WrapClient: SCHED_SOCKET undefined");
		return 0;
	}

	CLogger::mainlog->info("WrapClient: connect to scheduler at %s", schedSocket);	

	struct sockaddr_un addr = {};
	addr.sun_family = AF_UNIX;

	strcpy(addr.sun_path, schedSocket);

	int socketid = socket(AF_UNIX, SOCK_STREAM, 0);
	if (socketid == -1) {
		int error = errno;
		errno = 0;
		CLogger::mainlog->error("WrapClient: Failed to connect to scheduler socket %s %d %s", schedSocket, error, strerror(error));
		return 0;
	}

	int res = connect(socketid, (struct sockaddr *)&addr, sizeof(struct sockaddr_un));

	if (res == -1) {
		int error = errno;
		errno = 0;
		CLogger::mainlog->error("WrapClient: Failed to connect to scheduler socket %s %d %s", schedSocket, error, strerror(error));
		close(socketid);
		return 0;
	}

	CComUnixSchedSchedulerWrap* sched = new CComUnixSchedSchedulerWrap(*mpServer, mrResources, mrTaskDatabase, *this, socketid);
	res = sched->initClient();
	if (res != 0) {
		delete sched;
		return 0;
	}
	CComUnixClient* uclient = dynamic_cast <CComUnixClient*> (sched);
	mpServer->addNewClient(uclient, socketid);
	return sched;
}

void CUnixWrapClient::clientTasklist(CComUnixSchedClientWrap& client, std::vector<CTaskWrapper*>* newtasks) {
	CLogger::mainlog->debug("WrapClient: clientTasklist");
	{
		std::unique_lock<std::mutex> ul(mWrapMutex);
		CWrapTaskApp* app = mrGroup.mTaskApps[mRegisteredApps];
		app->globalTasks.insert(app->globalTasks.end(), newtasks->begin(), newtasks->end());
		allTasks.insert(allTasks.end(), newtasks->begin(), newtasks->end());
		// add tasks to mapping
		for (int ix=0; app->tasks[ix]!=-1; ix++) {
			CTaskWrapper* task = (*newtasks)[ix];
			// add inter-app dependencies
			int* dep = app->dependencies[ix];
			if (dep == 0) {
				continue;
			}
			for (int dix=0; dep[dix]!=-1; dix++) {
				int pid = dep[dix];
				// predecessor belongs to same app, dependency exists already
				bool sameapp = app->containsTask(pid);
				if (sameapp == true) {
					continue;
				}
				// find app containing id
				CWrapTaskApp* dapp = 0;
				for (int appix=0; appix<mRegisteredApps; appix++) {
					if (mrGroup.mTaskApps[appix]->containsTask(pid) == true) {
						dapp = mrGroup.mTaskApps[appix];
						break;
					}
				}
				if (dapp == 0) {
					CLogger::mainlog->error("WrapClient: invalid dependency");
					mErrorApps++;
					return;
				}
				CTaskWrapper* ptask = dapp->getTaskWithId(pid);
				if (ptask == 0) {
					CLogger::mainlog->error("WrapClient: invalid dependency");
					mErrorApps++;
					return;
				}

				// replace predecessor list
				int* newplist = new int[task->mPredecessorNum+1]();
				for (int i=0; i<task->mPredecessorNum; i++) {
					newplist[i] = task->mpPredecessorList[i];
				}
				newplist[task->mPredecessorNum] = pid;
				
				delete[] task->mpPredecessorList;
				task->mpPredecessorList = newplist;
				task->mPredecessorNum++;

				// replace successor list
				int* newslist = new int[ptask->mSuccessorNum+1]();
				for (int i=0; i<ptask->mSuccessorNum; i++) {
					newslist[i] = ptask->mpSuccessorList[i];
				}
				newslist[ptask->mSuccessorNum] = task->mId;

				delete[] ptask->mpSuccessorList;
				ptask->mpSuccessorList = newslist;
				ptask->mSuccessorNum++;

			}
		}


		mRegisteredApps++;
	}
	
	mWrapCondVar.notify_one();
}

void CUnixWrapClient::clientAborted(CComUnixSchedClientWrap& client) {
	CLogger::mainlog->debug("WrapClient: clientAbort");
	{
		std::unique_lock<std::mutex> ul(mWrapMutex);
		mErrorApps++;
	}
	mWrapCondVar.notify_one();
}

void CUnixWrapClient::clientFail(CComUnixSchedClientWrap& client) {
	CLogger::mainlog->debug("WrapClient: clientFail");
	{
		std::unique_lock<std::mutex> ul(mWrapMutex);
		mErrorApps++;
	}
	mWrapCondVar.notify_one();

	CWrapMain::shutdown();
}

void CUnixWrapClient::clientStarted(CComUnixSchedClientWrap& client, int taskid) {

	CTaskWrapper* task = mrTaskDatabase.getTaskWrapper(taskid);
	task->started();

	CLogger::mainlog->debug("WrapClient: clientStarted");
	int schedid = taskGlobalToSched[taskid];
	schedulerClient->started(schedid);
}

void CUnixWrapClient::clientFinished(CComUnixSchedClientWrap& client, int taskid) {

	CTaskWrapper* task = mrTaskDatabase.getTaskWrapper(taskid);
	task->finished();

	CLogger::mainlog->debug("WrapClient: clientFinished");
	int schedid = taskGlobalToSched[taskid];
	schedulerClient->finished(schedid);

	if (mrTaskDatabase.tasksDone() == true) {
		CLogger::mainlog->debug("WrapClient: all tasks done");
		CWrapMain::shutdown();
	}
}

void CUnixWrapClient::clientSuspended(CComUnixSchedClientWrap& client, int taskid, int progress) {

	CTaskWrapper* task = mrTaskDatabase.getTaskWrapper(taskid);
	task->suspended(progress);

	CLogger::mainlog->debug("WrapClient: clientSuspended");
	int schedid = taskGlobalToSched[taskid];
	schedulerClient->suspended(schedid, progress);
}

void CUnixWrapClient::clientProgress(CComUnixSchedClientWrap& client, int taskid, int progress) {


	CTaskWrapper* task = mrTaskDatabase.getTaskWrapper(taskid);
	task->gotProgress(progress);

	CLogger::mainlog->debug("WrapClient: clientProgress");
	int schedid = taskGlobalToSched[taskid];
	schedulerClient->progress(schedid, progress);
}

void CUnixWrapClient::clientQuit(CComUnixSchedClientWrap& client) {
	CLogger::mainlog->debug("WrapClient: clientQuit");
}





void CUnixWrapClient::serverTaskStart(CComUnixSchedSchedulerWrap& scheduler, int taskid, CResource& resource, int targetProgress, ETaskOnEnd onEnd) {

	CLogger::mainlog->debug("WrapClient: serverTaskStart %d", taskid);
	int globalid = taskSchedToGlobal[taskid];
	CTaskWrapper* task = mrTaskDatabase.getTaskWrapper(globalid);
	CLogger::mainlog->debug("WrapClient: found task %x", task);

	task->start(resource, targetProgress, onEnd);

	//CComSchedClient* client = task->getClient();
	//client->start(resource, targetProgress, onEnd, *task);
}

void CUnixWrapClient::serverTaskSuspend(CComUnixSchedSchedulerWrap& scheduler, int taskid) {

	CLogger::mainlog->debug("WrapClient: serverTaskSuspend");
	int globalid = taskSchedToGlobal[taskid];
	CTaskWrapper* task = mrTaskDatabase.getTaskWrapper(globalid);

	task->suspend();

	//CComSchedClient* client = task->getClient();
	//client->suspend(*task);
}

void CUnixWrapClient::serverTaskAbort(CComUnixSchedSchedulerWrap& scheduler, int taskid) {

	// TODO:
	CLogger::mainlog->debug("WrapClient: serverTaskAbort");
}

void CUnixWrapClient::serverTaskProgress(CComUnixSchedSchedulerWrap& scheduler, int taskid) {

	CLogger::mainlog->debug("WrapClient: serverTaskProgress");
	int globalid = taskSchedToGlobal[taskid];
	CTaskWrapper* task = mrTaskDatabase.getTaskWrapper(globalid);

	task->getProgress();

	//CComSchedClient* client = task->getClient();
	//client->progress(*task);
}

void CUnixWrapClient::serverTaskIds(CComUnixSchedSchedulerWrap& scheduler, int* taskids) {

	CLogger::mainlog->debug("WrapClient: serverTaskIds");
	{
		std::unique_lock<std::mutex> ul(mWrapMutex);

		for (unsigned int i=0; i<allTasks.size(); i++) {
			int localid = i;
			int schedid = taskids[i];
			taskSchedToGlobal[schedid] = localid;
			taskGlobalToSched[localid] = schedid;
		}

		registered = true;
	}
	mWrapCondVar.notify_one();

}

void CUnixWrapClient::serverQuit(CComUnixSchedSchedulerWrap& scheduler) {

	CLogger::mainlog->debug("WrapClient: serverQuit");
}

void CUnixWrapClient::serverFail(CComUnixSchedSchedulerWrap& scheduler) {

	CLogger::mainlog->debug("WrapClient: serverFail");
	CWrapMain::shutdown();
}
