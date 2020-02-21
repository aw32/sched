// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#include <signal.h>
#include <unistd.h>
#include <sys/syscall.h>
#include "CScheduleComputerMain.h"
#include "CConfig.h"
#include "CLogger.h"
#include "CScheduleAlgorithmLinear.h"
#include "CScheduleAlgorithmTestMigration.h"
#include "CScheduleAlgorithmMinMin.h"
#include "CScheduleAlgorithmMinMin2.h"
#include "CScheduleAlgorithmMinMin2Dyn.h"
#include "CScheduleAlgorithmMinMinMig.h"
#include "CScheduleAlgorithmMinMinMig2.h"
#include "CScheduleAlgorithmMinMinMig2Dyn.h"
#include "CScheduleAlgorithmMaxMin.h"
#include "CScheduleAlgorithmMaxMin2.h"
#include "CScheduleAlgorithmMaxMin2Dyn.h"
#include "CScheduleAlgorithmMaxMinMig2.h"
#include "CScheduleAlgorithmMaxMinMig2Dyn.h"
#include "CScheduleAlgorithmSufferage.h"
#include "CScheduleAlgorithmSufferage2.h"
#include "CScheduleAlgorithmSufferage2Dyn.h"
#include "CScheduleAlgorithmSufferageMig2.h"
#include "CScheduleAlgorithmSufferageMig2Dyn.h"
#include "CScheduleAlgorithmHEFT.h"
#include "CScheduleAlgorithmHEFT2.h"
#include "CScheduleAlgorithmHEFT2Dyn.h"
#include "CScheduleAlgorithmHEFTMig2.h"
#include "CScheduleAlgorithmHEFTMig2Dyn.h"
#include "CScheduleAlgorithmGenetic.h"
#include "CScheduleAlgorithmGeneticDyn.h"
#include "CScheduleAlgorithmGeneticMig.h"
#include "CScheduleAlgorithmGeneticMigDyn.h"
#include "CScheduleAlgorithmMCT.h"
#include "CScheduleAlgorithmMCT2.h"
#include "CScheduleAlgorithmMCTMig.h"
#include "CScheduleAlgorithmMCTMig2.h"
#include "CScheduleAlgorithmMET.h"
#include "CScheduleAlgorithmMET2.h"
#include "CScheduleAlgorithmMETMig2.h"
#include "CScheduleAlgorithmOLB.h"
#include "CScheduleAlgorithmOLB2.h"
#include "CScheduleAlgorithmOLBMig2.h"
#include "CScheduleAlgorithmSA.h"
#include "CScheduleAlgorithmSA2.h"
#include "CScheduleAlgorithmSAMig2.h"
#include "CScheduleAlgorithmKPB.h"
#include "CScheduleAlgorithmKPB2.h"
#include "CScheduleAlgorithmKPBMig2.h"
#include "CScheduleAlgorithmReMinMin.h"
#include "CScheduleAlgorithmReMinMin2.h"
#include "CScheduleAlgorithmReMinMin2Dyn.h"
#include "CScheduleAlgorithmReMinMinMig2.h"
#include "CScheduleAlgorithmReMinMinMig2Dyn.h"
#include "CScheduleAlgorithmSimulatedAnnealing.h"
#include "CScheduleAlgorithmSimulatedAnnealingDyn.h"
#include "CScheduleAlgorithmSimulatedAnnealingMig.h"
#include "CScheduleAlgorithmSimulatedAnnealingMigDyn.h"
#include "CTaskDatabase.h"
#include "CFeedback.h"
#include "CSchedule.h"
#include "CScheduleExecutor.h"
#include "CTaskWrapper.h"
#include "CTaskCopy.h"
#include "CResource.h"
using namespace sched::schedule;
using sched::task::CTaskWrapper;
using sched::task::CTaskCopy;
using sched::task::CResource;

using sched::algorithm::CScheduleAlgorithmLinear;
using sched::algorithm::CScheduleAlgorithmTestMigration;
using sched::algorithm::CScheduleAlgorithmMinMin;
using sched::algorithm::CScheduleAlgorithmMinMin2;
using sched::algorithm::CScheduleAlgorithmMinMin2Dyn;
using sched::algorithm::CScheduleAlgorithmMinMinMig;
using sched::algorithm::CScheduleAlgorithmMinMinMig2;
using sched::algorithm::CScheduleAlgorithmMinMinMig2Dyn;
using sched::algorithm::CScheduleAlgorithmMaxMin;
using sched::algorithm::CScheduleAlgorithmMaxMin2;
using sched::algorithm::CScheduleAlgorithmMaxMin2Dyn;
using sched::algorithm::CScheduleAlgorithmMaxMinMig2;
using sched::algorithm::CScheduleAlgorithmMaxMinMig2Dyn;
using sched::algorithm::CScheduleAlgorithmSufferage;
using sched::algorithm::CScheduleAlgorithmSufferage2;
using sched::algorithm::CScheduleAlgorithmSufferage2Dyn;
using sched::algorithm::CScheduleAlgorithmSufferageMig2;
using sched::algorithm::CScheduleAlgorithmSufferageMig2Dyn;
using sched::algorithm::CScheduleAlgorithmHEFT;
using sched::algorithm::CScheduleAlgorithmHEFT2;
using sched::algorithm::CScheduleAlgorithmHEFT2Dyn;
using sched::algorithm::CScheduleAlgorithmHEFTMig2;
using sched::algorithm::CScheduleAlgorithmHEFTMig2Dyn;
using sched::algorithm::CScheduleAlgorithmGenetic;
using sched::algorithm::CScheduleAlgorithmGeneticDyn;
using sched::algorithm::CScheduleAlgorithmGeneticMig;
using sched::algorithm::CScheduleAlgorithmGeneticMigDyn;
using sched::algorithm::CScheduleAlgorithmMCT;
using sched::algorithm::CScheduleAlgorithmMCT2;
using sched::algorithm::CScheduleAlgorithmMCTMig;
using sched::algorithm::CScheduleAlgorithmMCTMig2;
using sched::algorithm::CScheduleAlgorithmMET;
using sched::algorithm::CScheduleAlgorithmMET2;
using sched::algorithm::CScheduleAlgorithmMETMig2;
using sched::algorithm::CScheduleAlgorithmOLB;
using sched::algorithm::CScheduleAlgorithmOLB2;
using sched::algorithm::CScheduleAlgorithmOLBMig2;
using sched::algorithm::CScheduleAlgorithmSA;
using sched::algorithm::CScheduleAlgorithmSA2;
using sched::algorithm::CScheduleAlgorithmSAMig2;
using sched::algorithm::CScheduleAlgorithmKPB;
using sched::algorithm::CScheduleAlgorithmKPB2;
using sched::algorithm::CScheduleAlgorithmKPBMig2;
using sched::algorithm::CScheduleAlgorithmReMinMin;
using sched::algorithm::CScheduleAlgorithmReMinMin2;
using sched::algorithm::CScheduleAlgorithmReMinMin2Dyn;
using sched::algorithm::CScheduleAlgorithmReMinMinMig2;
using sched::algorithm::CScheduleAlgorithmReMinMinMig2Dyn;
using sched::algorithm::CScheduleAlgorithmSimulatedAnnealing;
using sched::algorithm::CScheduleAlgorithmSimulatedAnnealingDyn;
using sched::algorithm::CScheduleAlgorithmSimulatedAnnealingMig;
using sched::algorithm::CScheduleAlgorithmSimulatedAnnealingMigDyn;

CScheduleComputerMain::CScheduleComputerMain(std::vector<CResource*>& rResources, CFeedback& rFeedback, CTaskDatabase& rTaskDatabase):
	mrResources(rResources),
	mrFeedback(rFeedback),
	mrTaskDatabase(rTaskDatabase)
{

	// Register at task database
	mrTaskDatabase.setScheduleComputer(this);


	// check config
	// read attribute "computer_interrupt"
	// possible values:
	//		"no_interrupt" (default)
	//		"get_progress"
	//		"suspend_executor"
	CConfig* config = CConfig::getConfig();
	std::string* interrupt_str = 0;
	const char* interrupt = 0;
	int res = 0;
	res = config->conf->getString((char*)"computer_interrupt", &interrupt_str);
	if (-1 == res){
		CLogger::mainlog->error("ScheduleComputer: config key \"computer_interrupt\" not found, using default: no_interrupt");
	} else {
		interrupt = interrupt_str->c_str();
		if (strcmp(interrupt,"get_progress") == 0) {
			mExecutorInterrupt = EExecutorInterrupt::GETPROGRESS;
		} else
		if (strcmp(interrupt,"suspend_executor") == 0) {
			mExecutorInterrupt = EExecutorInterrupt::SUSPENDEXECUTOR;
		}
	}
	switch (mExecutorInterrupt) {
		case EExecutorInterrupt::NOINTERRUPT:
			CLogger::mainlog->info("ScheduleComputer: no executor interrupt");
			mTaskUpdate = 0;
		break;
		case EExecutorInterrupt::GETPROGRESS:
			CLogger::mainlog->info("ScheduleComputer: get progress");
			mTaskUpdate = 1;
		break;
		break;
		case EExecutorInterrupt::SUSPENDEXECUTOR:
			CLogger::mainlog->info("ScheduleComputer: suspend executor");
			mTaskUpdate = 1;
		break;
	}

	uint64_t appCount = 0;
	res = config->conf->getUint64((char*)"computer_required_applications", &appCount);
	if (-1 == res || appCount <= 0) {
		CLogger::mainlog->info("ScheduleComputer: config key \"computer_required_applications\" not found, using default: 0");
		CLogger::mainlog->info("ScheduleComputer: schedule computations starts immediately");
		mRequiredApplicationCount = 0;
	} else {
		CLogger::mainlog->info("ScheduleComputer: wait for %ld applications before computing schedule", appCount);
		mRequiredApplicationCount = appCount;
	}
}

int CScheduleComputerMain::loadAlgorithm(){

	CConfig* config = CConfig::getConfig();
	std::string* scheduler_str = 0;
	const char* scheduler = 0;
	int res = 0;
	res = config->conf->getString((char*)"scheduler", &scheduler_str);
	if (-1 == res) {
		CLogger::mainlog->error("ScheduleComputer: config key \"scheduler\" not found, using default: Linear");
		scheduler = (char*)"Linear";
	} else {
		scheduler = scheduler_str->c_str();
	}
	
	// Load scheduler
	CScheduleAlgorithm* alg = 0;
	if (strcmp("Linear",scheduler) == 0) {
		alg = new CScheduleAlgorithmLinear(mrResources);
	} else
	if (strcmp("TestMigration",scheduler) == 0) {
		alg = new CScheduleAlgorithmTestMigration(mrResources);
	} else
	if (strcmp("MinMin",scheduler) == 0) {
		alg = new CScheduleAlgorithmMinMin(mrResources);
	} else
	if (strcmp("MinMin2",scheduler) == 0) {
		alg = new CScheduleAlgorithmMinMin2(mrResources);
	} else
	if (strcmp("MinMin2Dyn",scheduler) == 0) {
		alg = new CScheduleAlgorithmMinMin2Dyn(mrResources);
	} else
	if (strcmp("MinMinMig",scheduler) == 0) {
		alg = new CScheduleAlgorithmMinMinMig(mrResources);
	} else
	if (strcmp("MinMinMig2",scheduler) == 0) {
		alg = new CScheduleAlgorithmMinMinMig2(mrResources);
	} else
	if (strcmp("MinMinMig2Dyn",scheduler) == 0) {
		alg = new CScheduleAlgorithmMinMinMig2Dyn(mrResources);
	} else
	if (strcmp("MaxMin",scheduler) == 0) {
		alg = new CScheduleAlgorithmMaxMin(mrResources);
	} else
	if (strcmp("MaxMin2",scheduler) == 0) {
		alg = new CScheduleAlgorithmMaxMin2(mrResources);
	} else
	if (strcmp("MaxMin2Dyn",scheduler) == 0) {
		alg = new CScheduleAlgorithmMaxMin2Dyn(mrResources);
	} else
	if (strcmp("MaxMinMig2",scheduler) == 0) {
		alg = new CScheduleAlgorithmMaxMinMig2(mrResources);
	} else
	if (strcmp("MaxMinMig2Dyn",scheduler) == 0) {
		alg = new CScheduleAlgorithmMaxMinMig2Dyn(mrResources);
	} else
	if (strcmp("Sufferage",scheduler) == 0) {
		alg = new CScheduleAlgorithmSufferage(mrResources);
	} else
	if (strcmp("Sufferage2",scheduler) == 0) {
		alg = new CScheduleAlgorithmSufferage2(mrResources);
	} else
	if (strcmp("Sufferage2Dyn",scheduler) == 0) {
		alg = new CScheduleAlgorithmSufferage2Dyn(mrResources);
	} else
	if (strcmp("SufferageMig2",scheduler) == 0) {
		alg = new CScheduleAlgorithmSufferageMig2(mrResources);
	} else
	if (strcmp("SufferageMig2Dyn",scheduler) == 0) {
		alg = new CScheduleAlgorithmSufferageMig2Dyn(mrResources);
	} else
	if (strcmp("HEFT",scheduler) == 0) {
		alg = new CScheduleAlgorithmHEFT(mrResources);
	} else
	if (strcmp("HEFT2",scheduler) == 0) {
		alg = new CScheduleAlgorithmHEFT2(mrResources);
	} else
	if (strcmp("HEFT2Dyn",scheduler) == 0) {
		alg = new CScheduleAlgorithmHEFT2Dyn(mrResources);
	} else
	if (strcmp("HEFTMig2",scheduler) == 0) {
		alg = new CScheduleAlgorithmHEFTMig2(mrResources);
	} else
	if (strcmp("HEFTMig2Dyn",scheduler) == 0) {
		alg = new CScheduleAlgorithmHEFTMig2Dyn(mrResources);
	} else
	if (strcmp("Genetic",scheduler) == 0) {
		alg = new CScheduleAlgorithmGenetic(mrResources, CScheduleAlgorithmGenetic::EGeneticFitnessType::GENETIC_FITNESS_MAKESPAN);
	} else
	if (strcmp("GeneticDyn",scheduler) == 0) {
		alg = new CScheduleAlgorithmGeneticDyn(mrResources, CScheduleAlgorithmGeneticDyn::EGeneticFitnessType::GENETIC_FITNESS_MAKESPAN);
	} else
	if (strcmp("GeneticEnergy",scheduler) == 0) {
		alg = new CScheduleAlgorithmGenetic(mrResources, CScheduleAlgorithmGenetic::EGeneticFitnessType::GENETIC_FITNESS_ENERGY);
	} else
	if (strcmp("GeneticEnergyDyn",scheduler) == 0) {
		alg = new CScheduleAlgorithmGeneticDyn(mrResources, CScheduleAlgorithmGeneticDyn::EGeneticFitnessType::GENETIC_FITNESS_ENERGY);
	} else
	if (strcmp("GeneticMig",scheduler) == 0) {
		alg = new CScheduleAlgorithmGeneticMig(mrResources, CScheduleAlgorithmGeneticMig::EGeneticFitnessType::GENETIC_FITNESS_MAKESPAN);
	} else
	if (strcmp("GeneticMigDyn",scheduler) == 0) {
		alg = new CScheduleAlgorithmGeneticMigDyn(mrResources, CScheduleAlgorithmGeneticMigDyn::EGeneticFitnessType::GENETIC_FITNESS_MAKESPAN);
	} else
	if (strcmp("GeneticEnergyMig",scheduler) == 0) {
		alg = new CScheduleAlgorithmGeneticMig(mrResources, CScheduleAlgorithmGeneticMig::EGeneticFitnessType::GENETIC_FITNESS_ENERGY);
	} else
	if (strcmp("GeneticEnergyMigDyn",scheduler) == 0) {
		alg = new CScheduleAlgorithmGeneticMigDyn(mrResources, CScheduleAlgorithmGeneticMigDyn::EGeneticFitnessType::GENETIC_FITNESS_ENERGY);
	} else
	if (strcmp("MCT",scheduler) == 0) {
		alg = new CScheduleAlgorithmMCT(mrResources);
	} else
	if (strcmp("MCT2",scheduler) == 0) {
		alg = new CScheduleAlgorithmMCT2(mrResources);
	} else
	if (strcmp("MCTMig",scheduler) == 0) {
		alg = new CScheduleAlgorithmMCTMig(mrResources);
	} else
	if (strcmp("MCTMig2",scheduler) == 0) {
		alg = new CScheduleAlgorithmMCTMig2(mrResources);
	} else
	if (strcmp("MET",scheduler) == 0) {
		alg = new CScheduleAlgorithmMET(mrResources);
	} else
	if (strcmp("MET2",scheduler) == 0) {
		alg = new CScheduleAlgorithmMET2(mrResources);
	} else
	if (strcmp("METMig2",scheduler) == 0) {
		alg = new CScheduleAlgorithmMETMig2(mrResources);
	} else
	if (strcmp("OLB",scheduler) == 0) {
		alg = new CScheduleAlgorithmOLB(mrResources);
	} else
	if (strcmp("OLB2",scheduler) == 0) {
		alg = new CScheduleAlgorithmOLB2(mrResources);
	} else
	if (strcmp("OLBMig2",scheduler) == 0) {
		alg = new CScheduleAlgorithmOLBMig2(mrResources);
	} else
	if (strcmp("SA",scheduler) == 0) {
		alg = new CScheduleAlgorithmSA(mrResources);
	} else
	if (strcmp("SA2",scheduler) == 0) {
		alg = new CScheduleAlgorithmSA2(mrResources);
	} else
	if (strcmp("SAMig2",scheduler) == 0) {
		alg = new CScheduleAlgorithmSAMig2(mrResources);
	} else
	if (strcmp("KPB",scheduler) == 0) {
		alg = new CScheduleAlgorithmKPB(mrResources);
	} else
	if (strcmp("KPB2",scheduler) == 0) {
		alg = new CScheduleAlgorithmKPB2(mrResources);
	} else
	if (strcmp("KPBMig2",scheduler) == 0) {
		alg = new CScheduleAlgorithmKPBMig2(mrResources);
	} else
	if (strcmp("ReMinMin",scheduler) == 0) {
		alg = new CScheduleAlgorithmReMinMin(mrResources);
	} else
	if (strcmp("ReMinMin2",scheduler) == 0) {
		alg = new CScheduleAlgorithmReMinMin2(mrResources);
	} else
	if (strcmp("ReMinMin2Dyn",scheduler) == 0) {
		alg = new CScheduleAlgorithmReMinMin2Dyn(mrResources);
	} else
	if (strcmp("ReMinMinMig2",scheduler) == 0) {
		alg = new CScheduleAlgorithmReMinMinMig2(mrResources);
	} else
	if (strcmp("ReMinMinMig2Dyn",scheduler) == 0) {
		alg = new CScheduleAlgorithmReMinMinMig2Dyn(mrResources);
	} else
	if (strcmp("SimulatedAnnealing",scheduler) == 0) {
		alg = new CScheduleAlgorithmSimulatedAnnealing(mrResources, CScheduleAlgorithmGenetic::EGeneticFitnessType::GENETIC_FITNESS_MAKESPAN);
	} else
	if (strcmp("SimulatedAnnealingDyn",scheduler) == 0) {
		alg = new CScheduleAlgorithmSimulatedAnnealingDyn(mrResources, CScheduleAlgorithmGeneticDyn::EGeneticFitnessType::GENETIC_FITNESS_MAKESPAN);
	} else
	if (strcmp("SimulatedAnnealingMig",scheduler) == 0) {
		alg = new CScheduleAlgorithmSimulatedAnnealingMig(mrResources, CScheduleAlgorithmGeneticMig::EGeneticFitnessType::GENETIC_FITNESS_MAKESPAN);
	} else
	if (strcmp("SimulatedAnnealingMigDyn",scheduler) == 0) {
		alg = new CScheduleAlgorithmSimulatedAnnealingMigDyn(mrResources, CScheduleAlgorithmGeneticMigDyn::EGeneticFitnessType::GENETIC_FITNESS_MAKESPAN);
	} else
	if (strcmp("SimulatedAnnealingEnergy",scheduler) == 0) {
		alg = new CScheduleAlgorithmSimulatedAnnealing(mrResources, CScheduleAlgorithmGenetic::EGeneticFitnessType::GENETIC_FITNESS_ENERGY);
	} else
	if (strcmp("SimulatedAnnealingEnergyDyn",scheduler) == 0) {
		alg = new CScheduleAlgorithmSimulatedAnnealingDyn(mrResources, CScheduleAlgorithmGeneticDyn::EGeneticFitnessType::GENETIC_FITNESS_ENERGY);
	} else
	if (strcmp("SimulatedAnnealingEnergyMig",scheduler) == 0) {
		alg = new CScheduleAlgorithmSimulatedAnnealingMig(mrResources, CScheduleAlgorithmGeneticMig::EGeneticFitnessType::GENETIC_FITNESS_ENERGY);
	} else
	if (strcmp("SimulatedAnnealingEnergyMigDyn",scheduler) == 0) {
		alg = new CScheduleAlgorithmSimulatedAnnealingMigDyn(mrResources, CScheduleAlgorithmGeneticMigDyn::EGeneticFitnessType::GENETIC_FITNESS_ENERGY);
	}

	if (0 != alg) {
		CLogger::mainlog->info("ScheduleComputer: found algorithm: %s", scheduler);
		CLogger::eventlog->info("\"event\":\"ALGORITHM\",\"algorithm\":\"%s\"", scheduler);
		int initok = alg->init();
		if (initok == -1) {
			delete alg;
			return -1;
		}
		mpAlgorithm = alg;
		return 0;
	}
	
	CLogger::mainlog->error("ScheduleComputer: no valid algorithm found in config");
	return -1;
}

CScheduleComputerMain::~CScheduleComputerMain(){

	if (mpAlgorithm != 0) {
		mpAlgorithm->fini();
		delete mpAlgorithm;
		mpAlgorithm = 0;
	}

}

void CScheduleComputerMain::computeSchedule(){

	{
		std::lock_guard<std::mutex> lg(mMessageMutex);
		mMessage = mMessage | ComputerMessage::UPDATE;
		mAlgorithmInterrupt = 1;
		CLogger::eventlog->info("\"event\":\"COMPUTER_UPDATE\"");

		// do required number of applications check
		// do check here instead in the message loop to avoid race condition that leads to double schedule computation
		if (mRequiredApplicationCount > 0) {
			mRegisteredApplications = mrTaskDatabase.getApplicationCount();
		}
	}
	mMessageCondVar.notify_one();

}

void CScheduleComputerMain::setScheduleExecutor(CScheduleExecutor* pScheduleExecutor){

	mpScheduleExecutor = pScheduleExecutor;

}

int CScheduleComputerMain::start(){

	this->mThread = std::thread(&CScheduleComputerMain::compute, this);

	return 0;

}

void CScheduleComputerMain::stop(){
	CLogger::mainlog->debug("ScheduleComputer: going to stop threads");
	mStopThread = 1;
	{
		std::lock_guard<std::mutex> lg(mMessageMutex);
		mMessage = mMessage | ComputerMessage::EXIT;
		mAlgorithmInterrupt = 1;
		CLogger::mainlog->debug("ScheduleComputer: set interrupt flag");
	}
	mMessageCondVar.notify_one();
	if (mThread.joinable() == true) {
		mThread.join();
	}
}

void CScheduleComputerMain::compute(){

	CLogger::mainlog->debug("ScheduleComputer: thread start");

	pid_t pid = syscall(SYS_gettid);
	CLogger::mainlog->debug("ScheduleComputer: thread %d", pid);

	// mask signals for this thread
	int ret = 0;
	sigset_t sigset = {};
	ret = sigemptyset(&sigset);
	ret = sigaddset(&sigset, SIGINT);
	ret = sigaddset(&sigset, SIGTERM);
	//ret = sigprocmask(SIG_BLOCK, &sigset, 0);
	ret = pthread_sigmask(SIG_BLOCK, &sigset, 0);
	if (ret != 0) {
		CLogger::mainlog->error("ScheduleComputer: thread signal error: %s", strerror(errno));
	}



	unsigned int message = 0;
	
	while (mStopThread != 1){

		// wait for next message
		{
			std::unique_lock<std::mutex> ul(mMessageMutex);
			while (mMessage == 0) {
				mMessageCondVar.wait(ul);
			}
			// copy message
				message = mMessage;
				mMessage = 0;

		}

		CLogger::mainlog->debug("ScheduleComputer: got message %d", message);

		// exit
		if ((message & ComputerMessage::EXIT) != 0) {
			break;
		}

		// update schedule
		if ((message & ComputerMessage::UPDATE) != 0) {

			// check if required number of applications registered
			if (mRequiredApplicationCount > 0) {
				if (mRegisteredApplications < mRequiredApplicationCount) {
					CLogger::mainlog->debug("ScheduleComputer: not enough registered applications: %d of %d", this->mRegisteredApplications, mRequiredApplicationCount);
					continue;
				}
			}

			// until a new schedule is created
			int ret = 0;
			while (ret == 0) {

				if (mStopThread == 1) {
					// shutdown
					break;
				}

				// mode
				if (mExecutorInterrupt == EExecutorInterrupt::GETPROGRESS) {
					// getprogress: get progress of current tasks
					ret = mrFeedback.getProgress();
				} else
				if (mExecutorInterrupt == EExecutorInterrupt::SUSPENDEXECUTOR) {
					// suspendexecutor: stop current tasks
					CLogger::mainlog->debug("ScheduleComputer: susexec entry");
					ret = suspendExecutor();
					CLogger::mainlog->debug("ScheduleComputer: susexec outry");
				} else {
					// nointerrupt: just continue with the schedule creation
				}

				// another check for shutdown
				if (mStopThread == 1) {
					break;
				}

				// run schedule algorithm
				CLogger::eventlog->info("\"event\":\"COMPUTER_ALGOSTART\"");
				ret = computeAlgorithm();

			}

		}

	}

	CLogger::mainlog->debug("ScheduleComputer: thread stop");
}

int CScheduleComputerMain::suspendExecutor(){

	// wait for suspended executor
	{
		std::unique_lock<std::mutex> ul(mExecutorSuspensionMutex);
		mpScheduleExecutor->suspendSchedule();
		mExecutorSuspensionCondVar.wait(ul);
	}

	return 0;
}

void CScheduleComputerMain::executorSuspended(){

	CLogger::mainlog->debug("ScheduleComputer: executorSuspended entry");
	{
		std::lock_guard<std::mutex> lg(mExecutorSuspensionMutex);
		CLogger::mainlog->debug("ScheduleComputer: executorSuspended");
	}
	mExecutorSuspensionCondVar.notify_one();

}

// 0->repeat, 1->ok, -1->error
int CScheduleComputerMain::computeAlgorithm(){

	CSchedule* newSchedule = 0;

	CLogger::mainlog->info("ScheduleAlgorithm: start schedule algorithm");

	// reset time
	mAlgorithmStop = {};
	mAlgorithmDuration = {};
	mAlgorithmStart = std::chrono::steady_clock::now();

	// compute new schedule
	mAlgorithmInterrupt = 0;
	// copy tasks
	std::vector<CTaskCopy>* unfinishedTasks = mrTaskDatabase.copyUnfinishedTasks();

	// find out running tasks
	std::vector<CTaskCopy*> runningTasks;
	for (unsigned int mix=0; mix<mrResources.size(); mix++) {
		CTaskWrapper* task = 0;
		mrResources[mix]->getStatus(0,0,&task,0);
		if (task == 0) {
			// no task, add 0 pointer
			runningTasks.push_back(0);
		} else {
			// find local copy of task
			int taskid = task->mId;
			CTaskCopy* realtask = 0;
			for (unsigned int tix=0; tix<unfinishedTasks->size(); tix++) {
				if ((*unfinishedTasks)[tix].mId == taskid) {
					realtask = &((*unfinishedTasks)[tix]);
					break;
				}
			}
			// add pointer to local copy to list of running tasks (or 0 pointer if not found)
			runningTasks.push_back(realtask);
		}
	}

	// execute algorithm
	newSchedule = mpAlgorithm->compute(unfinishedTasks, &runningTasks, &mAlgorithmInterrupt, mTaskUpdate);

	if (newSchedule == 0) {
		CLogger::mainlog->debug("ScheduleComputer: algorithm interrupted");
		return 0;
	}
	
	// log schedule computation duration
	mAlgorithmStop = std::chrono::steady_clock::now();
	mAlgorithmDuration = mAlgorithmStop - mAlgorithmStart;
	double nseconds = double(mAlgorithmDuration.count()) 
		* std::chrono::steady_clock::period::num 
		/ std::chrono::steady_clock::period::den;
	newSchedule->mComputeStart = mAlgorithmStart;
	newSchedule->mComputeStop = mAlgorithmStop;
	newSchedule->mComputeDuration = mAlgorithmDuration;


	CLogger::mainlog->info("ScheduleAlgorithm: duration %f s, %d tasks", nseconds, newSchedule->mActiveTasks);


	// message schedule executor
	newSchedule->mId = mScheduleNum++;
	CLogger::eventlog->info("\"event\":\"COMPUTER_ALGOSTOP\",\"duration\":%f", nseconds);
	std::ostringstream scheduleJson;
	newSchedule->printJson(scheduleJson);
	CLogger::eventlog->infoStream() << "\"event\":\"SCHEDULE\",\"schedule\":" << scheduleJson.str();
	mpScheduleExecutor->updateSchedule(newSchedule);
	newSchedule = 0;
	return 1;
}


int CScheduleComputerMain::getRequiredApplicationCount() {
	return this->mRequiredApplicationCount;
}


int CScheduleComputerMain::getRegisteredApplications() {
	return this->mRegisteredApplications;
}
