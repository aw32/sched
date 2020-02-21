// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#include <map>
#include "CEstimationLinear.h"
#include "CTask.h"
#include "CResource.h"
using namespace sched::algorithm;

double CEstimationLinear::taskTimeInit(CTask* task, CResource* res) {
	std::map<std::string, double*>* resmap = 0;
	resmap = (std::map<std::string, double*>*) (*task->mpAttributes)[std::string("msresults")];

	if (resmap == 0) {
		return 0.0;
	}

	double* results = (*resmap)[res->mName];
	if (results == 0) {
		return 0.0;
	}

	return results[4];
}

double CEstimationLinear::taskTimeCompute(CTask* task, CResource* res, int startCheckpoint, int stopCheckpoint) {

	std::map<std::string, double*>* resmap = 0;
	resmap = (std::map<std::string, double*>*) (*task->mpAttributes)[std::string("msresults")];

	if (resmap == 0) {
		return 0.0;
	}

	double* results = (*resmap)[res->mName];
	if (results == 0) {
		return 0.0;
	}

	return (results[5] / task->mCheckpoints) * (stopCheckpoint-startCheckpoint);
}

double CEstimationLinear::taskTimeFini(CTask* task, CResource* res) {

	std::map<std::string, double*>* resmap = 0;
	resmap = (std::map<std::string, double*>*) (*task->mpAttributes)[std::string("msresults")];

	if (resmap == 0) {
		return 0.0;
	}

	double* results = (*resmap)[res->mName];
	if (results == 0) {
		return 0.0;
	}

	return results[6];
}


int CEstimationLinear::taskTimeComputeCheckpoint(CTask* task, CResource* res, int startCheckpoint, double sec) {

	std::map<std::string, double*>* resmap = 0;
	resmap = (std::map<std::string, double*>*) (*task->mpAttributes)[std::string("msresults")];

	if (resmap == 0) {
		return 0;
	}

	double* results = (*resmap)[res->mName];
	if (results == 0) {
		return 0;
	}

	// elapsed time in seconds / time for one compute checkpoint
	// return number of reached checkpoints
	return (int)(sec / (results[5] / task->mCheckpoints));
}

CEstimationLinear::~CEstimationLinear(){
}


double CEstimationLinear::taskEnergyInit(CTask* task, CResource* res) {
	return 0.0;
}

double CEstimationLinear::taskEnergyCompute(CTask* task, CResource* res, int startCheckpoint, int stopCheckpoint) {

	std::map<std::string, double*>* resmap = 0;
	resmap = (std::map<std::string, double*>*) (*task->mpAttributes)[std::string("msresults")];

	if (resmap == 0) {
		return 0.0;
	}

	double* results = (*resmap)[res->mName];
	if (results == 0) {
		return 0.0;
	}

	// dynamic energy per checkpoint / number of checkpoints
	return (results[2] / task->mCheckpoints) * (stopCheckpoint-startCheckpoint);
}

double CEstimationLinear::taskEnergyFini(CTask* task, CResource* res) {
	return 0.0;
}


int CEstimationLinear::taskEnergyComputeCheckpoint(CTask* task, CResource* res, int startCheckpoint, double energy) {

	std::map<std::string, double*>* resmap = 0;
	resmap = (std::map<std::string, double*>*) (*task->mpAttributes)[std::string("msresults")];

	if (resmap == 0) {
		return 0;
	}

	double* results = (*resmap)[res->mName];
	if (results == 0) {
		return 0;
	}

	// elapsed time in seconds / time for one compute checkpoint
	// return number of reached checkpoints
	return (int)(energy / (results[2] / task->mCheckpoints));
}


double CEstimationLinear::resourceIdleEnergy(CResource* res, double seconds) {
	
	// get power info loaded by resource loader
	double* power = (double*) res->mAttributes[std::string("idle_power")];
	if (0 == power) {
		return 0.0;
	}

	// energy = time * power
	return seconds * (*power);

}


double CEstimationLinear::resourceIdlePower(CResource* res) {

	// get power info loaded by resource loader
	double* power = (double*) res->mAttributes[std::string("idle_power")];
	if (0 == power) {
		return 0.0;
	}

	return *power;

}
