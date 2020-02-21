// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#include "CResourceLoader.h"
#include "CResourceLoaderMS.h"
#include "CConfig.h"
#include "CLogger.h"
using namespace sched::schedule;


CResourceLoader::~CResourceLoader() {
}

int CResourceLoader::loadInfo(){

	CLogger::mainlog->debug("TaskLoader: information loaded (done nothing)");

	return 0;
}

void CResourceLoader::clearInfo(){
}

void CResourceLoader::getInfo(CResource* task){
}

CResourceLoader* CResourceLoader::loadResourceLoader(){

	// check config for loader name
	CConfig* config = CConfig::getConfig();
	std::string* loader_str = 0;
	const char* loader = 0;
	int res = 0;
	res = config->conf->getString((char*)"resourceloader", &loader_str);
	if (-1 == res) {
		CLogger::mainlog->error("ResourceLoader: config key \"resourceloader\" not found, using default: doing nothing");
		loader = "default";
	} else {
		loader = loader_str->c_str();
	}
	if (strcmp(loader, "resourceloaderms") == 0) {
		CLogger::mainlog->info("ResourceLoader: loaded ResourceLoaderMS");
		return new CResourceLoaderMS();
	} else
	if (strcmp(loader, "default") == 0) {
		CLogger::mainlog->info("ResourceLoader: use default (doing nothing)");
		return new CResourceLoader();
	} else {
		CLogger::mainlog->info("ResourceLoader: unknown resourceloader, using default (doing nothing)");
		return new CResourceLoader();
	}
}
