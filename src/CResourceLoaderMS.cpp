// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#include <fstream>
#include "cjson/cJSON.h"
#include "CResourceLoaderMS.h"
#include "CResource.h"
#include "CConfig.h"
#include "CLogger.h"
using namespace sched::schedule;


CResourceLoaderMS::CResourceLoaderMS(){

	// load path from config
	CConfig* config = CConfig::getConfig();
	std::string* path_str = 0;
	int res = 0;
	res = config->conf->getString((char*)"resourceloaderms_idle", &path_str);
	if (-1 == res) {
		CLogger::mainlog->error("ResourceLoaderMS: \"resourceloaderms_idle\" not found in config");
	} else {
		mpIdleFile = path_str->c_str();
		CLogger::mainlog->info("ResourceLoaderMS: idle file %s", mpIdleFile);
	}
}

CResourceLoaderMS::~CResourceLoaderMS(){
}

int CResourceLoaderMS::loadInfo(){


	if (mpIdleFile == 0) {
		return -1;
	}

	std::ifstream in(mpIdleFile);
	std::string contents((std::istreambuf_iterator<char>(in)), 
	std::istreambuf_iterator<char>());

	cJSON* idlefile = cJSON_Parse(contents.data());

	if (0 == idlefile) {
		CLogger::mainlog->error("ResourceLoaderMS: JSON parsing error");
		return -1;
	}

	int error = 0;

	cJSON* cpu = cJSON_GetObjectItem(idlefile, "cpu_power_avg");
	if (0 == cpu || cJSON_IsArray(cpu) == false) {
		CLogger::mainlog->error("ResourceLoaderMS: JSON cpu_power_avg invalid");
		error = 1;
	} else {
		cpu = cJSON_GetArrayItem(cpu , 0);
		if (0 == cpu) {
			CLogger::mainlog->error("ResourceLoaderMS: JSON cpu_power_avg invalid");
			error = 1;
		} else {
			cpu_power_avg = cpu->valuedouble;
		}
	}

	cJSON* gpu = cJSON_GetObjectItem(idlefile, "gpu_power_avg");
	if (0 == gpu || cJSON_IsArray(gpu) == false) {
		CLogger::mainlog->error("ResourceLoaderMS: JSON gpu_power_avg invalid");
		error = 1;
	} else {
		gpu = cJSON_GetArrayItem(gpu , 0);
		if (0 == gpu) {
			CLogger::mainlog->error("ResourceLoaderMS: JSON gpu_power_avg invalid");
			error = 1;
		} else {
			gpu_power_avg = gpu->valuedouble;
		}
	}

	cJSON* fpga = cJSON_GetObjectItem(idlefile, "fpga_power_avg");
	if (0 == fpga || cJSON_IsArray(fpga) == false) {
		CLogger::mainlog->error("ResourceLoaderMS: JSON fpga_power_avg invalid");
		error = 1;
	} else {
		fpga = cJSON_GetArrayItem(fpga , 0);
		if (0 == fpga) {
			CLogger::mainlog->error("ResourceLoaderMS: JSON fpga_power_avg invalid");
			error = 1;
		} else {
			fpga_power_avg = fpga->valuedouble;
		}
	}

	cJSON* all = cJSON_GetObjectItem(idlefile, "all_power_avg");
	if (0 == all || cJSON_IsArray(all) == false) {
		CLogger::mainlog->error("ResourceLoaderMS: JSON all_power_avg invalid");
		error = 1;
	} else {
		all = cJSON_GetArrayItem(all , 0);
		if (0 == all) {
			CLogger::mainlog->error("ResourceLoaderMS: JSON all_power_avg invalid");
			error = 1;
		} else {
			all_power_avg = all->valuedouble;
		}
	}

	cJSON_Delete(idlefile);

	if (1 == error) {
		CLogger::mainlog->error("ResourceLoaderMS: JSON invalid");
		return -1;
	}

	return 0;
}


void CResourceLoaderMS::clearInfo(){

}


void CResourceLoaderMS::getInfo(CResource* resource){

	if (strcmp("IntelXeon",resource->mName.c_str()) == 0) {
		// CPU
		(resource->mAttributes)[std::string("idle_power")] = &cpu_power_avg;

	} else
	if (strcmp("NvidiaTesla",resource->mName.c_str()) == 0) {
		// GPU
		(resource->mAttributes)[std::string("idle_power")] = &gpu_power_avg;

	} else
	if (strcmp("MaxelerVectis",resource->mName.c_str()) == 0) {
		// FPGA
		(resource->mAttributes)[std::string("idle_power")] = &fpga_power_avg;

	} else {

		CLogger::mainlog->info("ResourceLoaderMS: no info for resource %s", resource->mName.c_str());

	}

}
