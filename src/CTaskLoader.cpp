// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#include "CTaskLoader.h"
#include "CTaskLoaderMS.h"
#include "CConfig.h"
#include "CLogger.h"
using namespace sched::task;


CTaskLoader::~CTaskLoader() {
}

int CTaskLoader::loadInfo(){

	CLogger::mainlog->debug("TaskLoader: information loaded (done nothing)");

	return 0;
}

void CTaskLoader::clearInfo(){
}

void CTaskLoader::getInfo(CTask* task){
}

CTaskLoader* CTaskLoader::loadTaskLoader(){

	// check config for loader name
	CConfig* config = CConfig::getConfig();
	std::string* loader_str = 0;
	const char* loader = 0;
	int res = 0;
	res = config->conf->getString((char*)"taskloader", &loader_str);
	if (-1 == res) {
		CLogger::mainlog->warn("TaskLoader: config key \"taskloader\" not found, using default: doing nothing");
		loader = "default";
	} else {
		loader = loader_str->c_str();
	}
	if (strcmp(loader, "taskloaderms") == 0) {
		CLogger::mainlog->info("TaskLoader: loaded TaskLoaderMS");
		return new CTaskLoaderMS();
	} else
	if (strcmp(loader, "default") == 0) {
		CLogger::mainlog->info("TaskLoader: use default (doing nothing)");
		return new CTaskLoader();
	} else {
		CLogger::mainlog->info("TaskLoader: unknown taskloader, using default (doing nothing)");
		return new CTaskLoader();
	}
}
