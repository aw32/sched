// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#include <iostream>
#include <cstring>
#include <signal.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <vector>
#include "CWrapMain.h"
#include "CLogger.h"
#include "CConfig.h"
#include "CComUnixSchedServerWrap.h"
#include "CWrapTaskGroup.h"
#include "CUnixWrapClient.h"
#include "CTaskLoader.h"
#include "CResourceLoader.h"
#include "CResource.h"
#include "CTaskDatabase.h"

using namespace sched::wrap;

pid_t CWrapMain::mPid = 0;
pthread_t CWrapMain::mPthread = 0;

int CWrapMain::loadTaskDefinitions(){

	char* defFile = std::getenv("SCHED_TASKDEF");

	if (defFile == 0) {
		CLogger::mainlog->error("Main: SCHED_TASKDEF not defined");
		return 1;
	}

	taskDefinitions = CTaskDefinitions::loadTaskDefinitions(defFile);

	if (taskDefinitions == 0) {
		CLogger::mainlog->error("Main: failed to load task definitions");
		return 1;
	}

	return 0;
}

void CWrapMain::unloadTaskDefinitions(){

	if (taskDefinitions != 0) {

		delete taskDefinitions;
		taskDefinitions = 0;
	}

}

int CWrapMain::loadWrap() {

	char* schedSocket = std::getenv("SCHED_SOCKET");
	if (schedSocket == 0) {
		CLogger::mainlog->error("WrapClient: SCHED_SOCKET undefined");
		return -1;
	}

	wrap = new CUnixWrapClient(*taskDefinitions, *group, resources, *taskDatabase);
	return 0;
}

void CWrapMain::unloadWrap() {
	if (wrap != 0) {
		delete wrap;
		wrap = 0;
	}
}

int CWrapMain::loadGroup() {

	char* groupFile = std::getenv("WRAP_FILE");

	if (groupFile == 0) {
		return -1;
	}

	group = CWrapTaskGroup::loadTaskGroups(groupFile, *taskDefinitions);
	if (group == 0) {
		return -1;
	}
	return 0;
}

void CWrapMain::unloadGroup() {
	if (group != 0) {
		delete group;
		group = 0;
	}
}

int CWrapMain::loadConfiguration(){

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

void CWrapMain::unloadConfiguration(){

	CConfig::cleanConfig();
}

int CWrapMain::loadTaskDatabase(){

	CLogger::mainlog->info("Main: Start TaskDatabase");
	taskDatabase = new CTaskDatabase();
	int ret = taskDatabase->initialize();
	return ret;

}

void CWrapMain::unloadTaskDatabase(){

	if (taskDatabase != 0) {
		// Print tasks
		taskDatabase->printEndTasks();
		delete taskDatabase;
		taskDatabase = 0;
	}

}

int CWrapMain::loadResourceLoader(){

	resourceLoader = CResourceLoader::loadResourceLoader();
	if (0 != resourceLoader->loadInfo()) {
		CLogger::mainlog->error("Main: Error in resource loader");
		return -1;
	}
	return 0;

}

void CWrapMain::unloadResourceLoader(){

	if (0 != resourceLoader) {
		resourceLoader->clearInfo();
		delete resourceLoader;
		resourceLoader = 0;
	}

}

int CWrapMain::loadResources(){

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

void CWrapMain::unloadResources(){

	for (unsigned int i=0; i<resources.size(); i++) {
		delete resources[i];
	}

}

/*
int CMain::loadScheduleExecutor(){

	CLogger::mainlog->info("Main: Start ScheduleExecutor");
	scheduleExecutor = new CScheduleExecutorMain(resources, *taskDatabase);
	scheduleExecutor->start();
	return 0;

}

void CMain::unloadScheduleExecutor(){

	if (scheduleExecutor != 0) {
		scheduleExecutor->stop();
		delete scheduleExecutor;
		scheduleExecutor = 0;
	}

}

int CMain::loadFeedback(){

	feedback = new CFeedbackMain(resources);
	return 0;

}

void CMain::unloadFeedback(){

	if (feedback != 0) {
		delete feedback;
		feedback = 0;
	}

}

int CMain::loadScheduleComputer(){

	CLogger::mainlog->info("Main: Start ScheduleComputer");
	scheduleComputer = new CScheduleComputerMain(resources, *feedback, *taskDatabase);
	int algok = scheduleComputer->loadAlgorithm();
	if (algok == -1) {
		CLogger::mainlog->error("Main: Loading schedule algorithm failed");
		delete scheduleComputer;
		scheduleComputer = 0;
		return -1;
	}
	scheduleExecutor->setScheduleComputer(scheduleComputer);
	scheduleComputer->setScheduleExecutor(scheduleExecutor);
	scheduleComputer->start();
	return 0;

}

void CMain::unloadScheduleComputer(){

	if (scheduleComputer != 0) {
		scheduleComputer->stop();
		delete scheduleComputer;
		scheduleComputer = 0;
	}

}

int CMain::loadMeasurement(){

	CLogger::mainlog->info("Main: Start Measurement");
#ifdef SCHED_MEASURE_AMPEHRE
	measure = new CMeasureAmpehre();
#else
	measure = new CMeasureNull();
#endif
	measure->start();
	return 0;

}

void CMain::unloadMeasurement(){

	CLogger::mainlog->info("Main: Stop Measurement");
	if (measure != 0) {
		measure->print();
		measure->stop();
		measure->print();
		delete measure;
		measure = 0;
	}

}
*/

int CWrapMain::loadComUnixServer(){

	char* wrapSocket = std::getenv("WRAP_SOCKET");

	if (wrapSocket == 0) {
		CLogger::mainlog->error("UnixServer: invalid WRAP_SOCKET");
		return -1;
	}

	char* pPath = wrapSocket;

	// Check config for path
/*	CConfig* config = CConfig::getConfig();
	std::string* path_str = 0;
	int res = 0;
	res = config->conf->getString((char*)"unixsocketpath", &path_str);
	if (-1 == res) {
		CLogger::mainlog->error("UnixServer: config key \"unixsocketpath\" not found, using default: /tmp/sched.socket");
		return -1;
	} else {
		pPath = (char*) path_str->c_str();
	}
*/

	server = new CComUnixSchedServerWrap(resources, *taskDatabase, *wrap, pPath);
	int ret = server->start();
	if (ret != 0) {
		CLogger::mainlog->error("Main: Starting UnixServer failed, shutting down");
		delete server;
		server = 0;
		return -1;
	}
	wrap->setServer(server);
	return 0;

}

void CWrapMain::unloadComUnixServer(){

	if (server != 0) {
		server->stop();
		delete server;
		server = 0;
	}

}

int CWrapMain::main(){

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

	// Reading task definitions
	ret = loadTaskDefinitions();
	if (ret == 0) {

		// Reading group file
		ret = loadGroup();
		if (ret == 0) {


			// Reading configuration file
			ret = loadConfiguration();
			if (ret == 0) {

				// Init task database
				ret = loadTaskDatabase();
				if (ret == 0) {

					// Load Resource Loader
					ret = loadResourceLoader();
					if (ret == 0) {

						// Load resources
						ret = loadResources();
						if (ret == 0) {

							// Load wrap
							ret = loadWrap();
							if (ret == 0) {

											// Start Unix Socket Server
											ret = loadComUnixServer();
											if (ret == 0) {

												sleep(1);
												CLogger::mainlog->info("WrapMain: initialize");
												ret = wrap->initialize();
												if (ret == 0) {

													CLogger::mainlog->info("WrapMain: register group");
													ret = wrap->registerGroup();

													if (ret == 0) {
														// Wait for shutdown
														waitForSignal();
	
														CLogger::mainlog->info("Main: Shutting down");
													}
												}

											}
											// Stop Unix Socket Server
											unloadComUnixServer();

							}
							// delete wrap
							unloadWrap();

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

		}
		// Delete group
		unloadGroup();

	}
	unloadTaskDefinitions();


/*
	// Reading configuration file
	ret = loadConfiguration();
	if (ret == 0) {

		// Init task database
		ret = loadTaskDatabase();
		if (ret == 0) {

			// Load Resource Loader
			ret = loadResourceLoader();
			if (ret == 0) {

				// Load resources
				ret = loadResources();
				if (ret == 0) {

					// Load feedback
					ret = loadFeedback();
					if (ret == 0) {

						// Init schedule executor
						ret = loadScheduleExecutor();
						if (ret == 0) {

							// Init schedule computer
							ret = loadScheduleComputer();
							if (ret == 0) {

								// Start measurement
								ret = loadMeasurement();
								if (ret == 0) {

									// Start Unix Socket Server
									ret = loadComUnixServer();
									if (ret == 0) {

										// Wait for shutdown
										waitForSignal();

										CLogger::mainlog->info("Main: Shutting down");

									}
									// Stop Unix Socket Server
									unloadComUnixServer();

								}
								// Stop measurement
								unloadMeasurement();
							}
							// Remove schedule computer
							unloadScheduleComputer();

						}
						// Remove schedule executor
						// also removes resources
						unloadScheduleExecutor();

					}
					// Remove feedback
					unloadFeedback();
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
*/

	// Stop logging
	CLogger::eventlog->info("\"event\":\"SCHEDULER_STOP\"");
	CLogger::stopLogging();

	if (CLogger::error == 1) {
		return 1;
	}
	return 0;
}

void CWrapMain::serveSigaction(int sig) {

    pid_t pid = syscall(SYS_gettid);
    CLogger::mainlog->info("serveSigaction %d", pid);
}

void CWrapMain::shutdown(){
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

void CWrapMain::waitForSignal(){
	sigset_t set = {};
	siginfo_t signal = {};
	int ret = 0;
	int error = 0;
	sigemptyset(&set);
	sigaddset(&set, SIGINT);
	sigaddset(&set, SIGTERM);
	// block signal, or else the default signal handler will be executed
	// the blocked signal will be set to "pending" and catched by sigwait
	//sigprocmask(SIG_BLOCK, &set, NULL);
	pthread_sigmask(SIG_BLOCK, &set, NULL);
	ret = sigwaitinfo(&set, &signal);
	if (ret == -1) {
		error = errno;
		CLogger::mainlog->error("Signal error %d", error);
	} else {
		CLogger::mainlog->info("Received signal to shutdown %d", ret);
		CLogger::eventlog->info("\"event\":\"SCHEDULER_SIGNAL\"");
	}
}
