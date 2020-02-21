// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#include <iostream>
#include <cstring>
#include <signal.h>
//#include <sys/types.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <vector>
#include "CSimMain.h"
#include "CLogger.h"
#include "CConfig.h"
#include "CComUnixServer.h"
#include "CTaskLoader.h"
#include "CResourceLoader.h"
#include "CResource.h"
#include "CTaskDatabase.h"
#include "CScheduleExecutorMain.h"
#include "CScheduleComputerMain.h"
#include "CFeedback.h"
#include "CSimQueue.h"
#ifdef SCHED_MEASURE_AMPEHRE
#include "CMeasureAmpehre.h"
#else
#include "CMeasureNull.h"
#endif

using namespace sched::sim;

#ifdef SCHED_MEASURE_AMPEHRE
using sched::measure::CMeasureAmpehre;
#else
using sched::measure::CMeasureNull;
#endif

pid_t CSimMain::mPid = 0;
pthread_t CSimMain::mPthread = 0;

int CSimMain::loadConfiguration(){

	char const* defaultFile = "config.yml";
	char* configFile = std::getenv("SCHED_CONFIG");

	if (configFile == 0) {
		configFile = (char*) defaultFile;
	}

	int ret = CConfig::loadConfig(configFile);
	if (ret == -1) {
		CLogger::mainlog->infoStream() << "Main: Config parsing failed, shutting down";
	}
	return ret;

}

void CSimMain::unloadConfiguration(){

	CConfig::cleanConfig();
}

int CSimMain::loadTaskDatabase(){

	CLogger::mainlog->info("Main: Start TaskDatabase");
	taskDatabase = new CTaskDatabase();
	int ret = taskDatabase->initialize();
	return ret;

}

void CSimMain::unloadTaskDatabase(){

	if (taskDatabase != 0) {
		// Print tasks
		taskDatabase->printEndTasks();
		delete taskDatabase;
		taskDatabase = 0;
	}

}

int CSimMain::loadResourceLoader(){

	resourceLoader = CResourceLoader::loadResourceLoader();
	if (0 != resourceLoader->loadInfo()) {
		CLogger::mainlog->error("Main: Error in resource loader");
		return -1;
	}
	return 0;

}

void CSimMain::unloadResourceLoader(){

	if (0 != resourceLoader) {
		resourceLoader->clearInfo();
		delete resourceLoader;
		resourceLoader = 0;
	}

}

int CSimMain::loadResources(){

	int resok = CResource::loadResources(&resources, *taskDatabase);
	if (resok == -1) {
		CLogger::mainlog->error("Main: Error in resource config");
		return -1;
	}
	if (resources.size() == 0) {
		CLogger::mainlog->error("Main: No resources found, shutting down");
		return -1;
	}
	CLogger::mainlog->info("Main: Found %d resources:", resources.size());
	std::ostringstream ss;
	for (unsigned int i = 0; i<resources.size(); i++) {
		CLogger::mainlog->info("Main:  - %s", resources[i]->mName.c_str());
		ss << "\"" << resources[i]->mName.c_str() << "\"";
		if (i < resources.size()-1) {
			ss << ",";
		}
	}
	CLogger::eventlog->info("\"event\":\"RESOURCES\",\"resources\":[%s]", ss.str().c_str());

	// load resource info
	for (unsigned int i = 0; i<resources.size(); i++) {
		resourceLoader->getInfo(resources[i]);
	}

	return 0;

}

void CSimMain::unloadResources(){

	for (unsigned int i=0; i<resources.size(); i++) {
		delete resources[i];
	}

}

int CSimMain::loadSimulation(){
	simulation = new CSimQueue(resources, *taskDatabase);
	int ret = simulation->init();
	if (-1 == ret) {
		return -1;
	}
	return simulation->loadTaskEvents();
}

void CSimMain::unloadSimulation(){
	if (simulation != 0) {
		simulation->stopSimulation();
		delete simulation;
	}
}

int CSimMain::main(){

	mPid = syscall(SYS_gettid);
	mPthread = pthread_self();

	int ret = 0;

	// Start logging
	CLogger::startLogging();
	CLogger::mainlog->info("Main: Starting scheduler");
	CLogger::mainlog->debug("Main: thread %d", mPid);
	uint64_t sec = 0;
	uint64_t nsec = 0;
	CLogger::realtime(&sec, &nsec);
	CLogger::eventlog->info("\"event\":\"SCHEDULER_START\",\"realtime\":\"%ld.%09ld\"", sec, nsec);

	// Reading configuration file
	ret = loadConfiguration();
	if (ret == 0) {

		// Init task database
		ret = loadTaskDatabase();
		if (ret == 0) {

			ret = loadResourceLoader();
			if (ret == 0) {

				// Load resources
				ret = loadResources();
				if (ret == 0) {
					
					// Load simulation
					ret = loadSimulation();
					if (ret == 0) {

						simulation->startSimulation();

						// Wait for shutdown
						CLogger::mainlog->info("Main: Wait");
						waitForSignal();

						CLogger::mainlog->info("Main: Shutting down");

					}
					// Remove simulation
					unloadSimulation();
				}
				// Remove resources
				unloadResources();
			}
			// Clear Resource Loader
			unloadResourceLoader();

		}
		// Remove task database
		unloadTaskDatabase();

	}
	// Delete config
	unloadConfiguration();

	// Stop logging
	CLogger::eventlog->info("\"event\":\"SCHEDULER_STOP\"");
	CLogger::stopLogging();

	if (CLogger::error == 1) {
		return 1;
	}
	return 0;
}

void CSimMain::serveSigaction(int sig) {

    pid_t pid = syscall(SYS_gettid);
    CLogger::mainlog->info("serveSigaction %d", pid);
}

void CSimMain::shutdown(){
	int ret = 0;
/*	sigset_t sigset = {};
	struct sigaction siga = {};
	siga.sa_handler = (void(*)(int)) (&CSimMain::serveSigaction);
	ret = sigemptyset(&sigset);
	ret = sigaddset(&sigset, SIGINT);
	ret = sigaction(SIGINT, &siga, 0);*/

	//ret = kill(mPid, SIGINT);
	ret = pthread_kill(mPthread, SIGINT);
	//int ret = raise(SIGINT);
	//int ret = kill(getpid(), SIGINT);
	if (ret != 0) {
		int error = errno;
		errno = 0;
		CLogger::mainlog->info("Main: Shutdown, sending SIGINT to self failed with errno %d", error);
	}
}

void CSimMain::waitForSignal(){

	int ret = 0;
/*	sigset_t sigset = {};
	struct sigaction siga = {};
	siga.sa_handler = (void(*)(int)) (&CSimMain::serveSigaction);
	ret = sigemptyset(&sigset);
	ret = sigaddset(&sigset, SIGINT);
	ret = sigaction(SIGINT, &siga, 0);*/


	sigset_t set = {};
	siginfo_t signal = {};
	int error = 0;
	//sigfillset(&set);
	sigemptyset(&set);
	sigaddset(&set, SIGINT);
	sigaddset(&set, SIGTERM);
	// block signal, or else the default signal handler will be executed
	// the blocked signal will be set to "pending" and catched by sigwait
	ret = pthread_sigmask(SIG_BLOCK, &set, NULL);
	ret = sigwaitinfo(&set, &signal);
	if (ret == -1) {
		error = errno;
		CLogger::mainlog->error("Signal error %d", error);
	} else {
		CLogger::mainlog->info("Received signal to shutdown %d", ret);
		CLogger::eventlog->info("\"event\":\"SCHEDULER_SIGNAL\"");
	}
}
