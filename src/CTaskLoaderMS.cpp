// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#include <sys/stat.h>
#include <dirent.h>
#include "CTaskLoaderMS.h"
#include "CTask.h"
#include "CConfig.h"
#include "CLogger.h"
using namespace sched::task;


CTaskLoaderMS::CTaskLoaderMS(){

	// load path from config
	CConfig* config = CConfig::getConfig();
	std::string* path_str = 0;
	int res = 0;
	res = config->conf->getString((char*)"taskloadermspath", &path_str);
	if (-1 == res) {
		CLogger::mainlog->error("TaskLoaderMS: \"taskloadermspath\" not found in config");
	} else {
		mpInfoDir = path_str->c_str();
		CLogger::mainlog->info("TaskLoaderMS: path %s", mpInfoDir);
	}
}

CTaskLoaderMS::~CTaskLoaderMS(){
}

int CTaskLoaderMS::loadInfo(){


	if (mpInfoDir == 0) {
		return -1;
	}
	struct stat dirstat = {};
	int ret = stat(mpInfoDir, &dirstat);
	if (ret != 0) {
		CLogger::mainlog->error("TaskLoaderMS: configured info directory invalid: %s",strerror(errno));
		return -1;
	}
	if (S_ISDIR(dirstat.st_mode) == 0) {
		CLogger::mainlog->error("TaskLoaderMS: configured info directory is not a directory");
		return -1;
	}

	// iterate over all directory entries
	DIR* dir = opendir(mpInfoDir);
	if (dir == 0) {
		CLogger::mainlog->error("TaskLoaderMS: failed to check info directory: %s", strerror(errno));
		return -1;
	}
	struct dirent* file = 0;
	errno = 0;
	int error = 0;
	do {
		file = readdir(dir);
		if (file == 0) {
			error = errno;
			continue;
		}
		// load file
		if (file->d_type & DT_REG) {
			char* name = 0;
			int size = 0;
			char* resource = 0;
			char* type = 0; // time, energy
			//example: "ms_markov(200)@NvidiaTesla_time.csv"
//			ret = scanf(file->d_name, "%ms_%ms(%d)@%ms_%ms.csv",
			ret = sscanf(file->d_name, "%*1[m]%*1[s]%*1[_]%m[^(]%*1[(]%d%*1[)]%*1[@]%m[^_]%*1[_]%m[^.].csv",
				&name, &size, &resource, &type);
			if (ret < 4 || (strcmp(type, "time") != 0 && strcmp(type, "energy") != 0)) {
				CLogger::mainlog->debug("TaskLoaderMS: ignore %x %s", file->d_name, file->d_name);
				if (name != 0) {
					free(name);
				}
				if (resource != 0) {
					free(resource);
				}
				if (type != 0) {
					free(type);
				}

				continue;
			}
		
			double* info = loadTaskInfo(file->d_name);
			if (info != 0) {
				addTaskInfo(info, name, size, resource, type);
				// addTaskInfo consumes info
			}
			free(name);
			free(resource);
			free(type);
		} else {
			CLogger::mainlog->debug("TaskLoaderMS: ignore %s", file->d_name);
		}
		
	} while(file != 0 && error == 0);
	closedir(dir);
	if (error != 0) {
		CLogger::mainlog->error("TaskLoaderMS: failed to check info directory: %s", strerror(error));
		return -1;
	}

	

	return 0;
}

void CTaskLoaderMS::addTaskInfo(double* info, char* name, int size, char* resource, char* type){
	
	std::map<int, std::map<std::string, double*>*>* sizemap = 0;
	std::map<std::string, double*>* resmap = 0;
	double* arr = 0;

	std::map<std::string, std::map<int, std::map<std::string, double*>*>*>::iterator itname = mInfo.find(std::string(name));
	if (itname != mInfo.end()) {
		sizemap = itname->second;
	} else {
		sizemap = new std::map<int, std::map<std::string, double*>*>();
		mInfo[std::string(name)] = sizemap;
	}

	
	std::map<int, std::map<std::string, double*>*>::iterator itsize = sizemap->find(size);
	if (itsize != sizemap->end()) {
		resmap = itsize->second;
	} else {
		resmap = new std::map<std::string, double*>();
		(*sizemap)[size] = resmap;
	}

	std::map<std::string, double*>::iterator itres = resmap->find(std::string(resource));
	if (itres != resmap->end()) {
		arr = itres->second;
	}

	if (arr == 0) {
		(*resmap)[std::string(resource)] = info;
	} else {
		if (strcmp(type, "energy") == 0) {
			arr[0] = info[0]; // E_total
			arr[1] = info[1]; // T_total
			arr[2] = info[2]; // E_Task
		}
		if (strcmp(type, "time") == 0) {
			arr[4] = info[4]; // T_init
			arr[5] = info[5]; // T_comp
			arr[6] = info[6]; // T_fini
		}
		delete[] info;
	}
}

double* CTaskLoaderMS::loadTaskInfo(char* fname){
	CLogger::mainlog->debug("TaskLoaderMS: load %s", fname);

	// create path
	int len = strlen(mpInfoDir) + strlen(fname) + 2;
	char path[len];
	memset(path, 0, len*sizeof(char));
	strcat(path, mpInfoDir);
	strcat(path, (const char*)"/");
	strcat(path, fname);

	FILE* file = fopen(path, "r");
	if (file == 0) {
		CLogger::mainlog->error("TaskLoaderMS: failed to open file: %s , %s", path, strerror(errno));
		return 0;
	}
	int nlcount = 0; // newline count
	char next = 0;
	size_t read;
	// skip two lines, read lines until newline is read two times
	do {
		do {
			read = fread(&next, 1, 1, file);
			if (read <= 0) {
				if (feof(file) != 0) {
					CLogger::mainlog->error("TaskLoaderMS: file too short: %s", path);
				} else
				if (ferror(file) != 0) {
					CLogger::mainlog->error("TaskLoaderMS: file read error: %s", path);
				} else {
					CLogger::mainlog->error("TaskLoaderMS: unknown file read error: %s", path);
				}
				fclose(file);
				return 0;
			}
		} while (next != 0x0a);
		nlcount++;
	} while(nlcount < 2);
	int ret = 0;
	int error = 0;
	int line = 2;
	std::vector<double*>* list = new std::vector<double*>();
	double* arr = new double[7];
	do {
		ret = fscanf(file, "%lf;%lf;%lf;%lf;%lf;%lf;%lf;\n",
			&arr[0], &arr[1], &arr[2],
			&arr[3], &arr[4], &arr[5], &arr[6]);
		if (ret < 7) {
			if (ret == EOF) {
				if (ferror(file) != 0) {
					CLogger::mainlog->error("TaskLoaderMS: file read error: %s , %s", path, strerror(errno));
				} else {
					// file ended before any match
					break;
				}
			} else {
				CLogger::mainlog->error("TaskLoaderMS: file read error: %s , invalid line %d", path, line);
			}
			error = 1;
			break;
		}
		list->push_back(arr);
		arr = new double[7];
		line++;
	} while(true);
	double* result = 0;
	// no error occured, compute average
	if (error == 0 && line > 2) {
		unsigned int num = list->size();
		for (int i=0; i<7; i++) {
			double avg = 0.0;
			for (unsigned int j=0; j<num; j++) {
				avg += (*list)[j][i] / num;
			}
			(*list)[num-1][i] = avg; // store result in last array
		}
		result = (*list)[num-1];
		list->pop_back();
	}
	// clean up
	if (arr != 0) {
		delete[] arr;
	}
	while (list->empty() == false) {
		delete[] list->back();
		list->pop_back();
	}
	delete list;
	fclose(file);
	// in case of error return nothing
	if (error == 1 || line == 2) {
		CLogger::mainlog->error("TaskLoaderMS: load %s failed", fname);
		return 0;
	}
	return result;
}

void CTaskLoaderMS::clearInfo(){

	std::map<std::string, std::map<int, std::map<std::string, double*>*>*>::iterator itname = mInfo.begin();
	for (;itname!=mInfo.end(); itname++) {

	std::map<int, std::map<std::string, double*>*>* sizemap = itname->second;

		std::map<int, std::map<std::string, double*>*>::iterator itsize = sizemap->begin();
		for (;itsize!=sizemap->end(); itsize++) {


			std::map<std::string, double*>* resmap = itsize->second;
			std::map<std::string, double*>::iterator itres = resmap->begin();
			for(;itres!=resmap->end(); itres++) {

				delete[] itres->second;

			}
			resmap->clear();
			delete resmap;
		}
		sizemap->clear();
		delete sizemap;

	}
	mInfo.clear();
	
}

void CTaskLoaderMS::getInfo(CTask* task){

	std::map<int, std::map<std::string, double*>*>* sizemap = 0;
	std::map<std::string, double*>* resmap = 0;

	std::map<std::string, std::map<int, std::map<std::string, double*>*>*>::iterator itname = mInfo.find(*(task->mpName));
	if (itname == mInfo.end()) {
		CLogger::mainlog->error("TaskLoaderMS: no info for task %s", task->mpName->c_str());
		return;
	} else {
		sizemap = itname->second;
	}


	std::map<int, std::map<std::string, double*>*>::iterator itsize = sizemap->find(task->mSize);
	if (itsize == sizemap->end()) {
		CLogger::mainlog->error("TaskLoaderMS: no info for task %s with size %d", task->mpName->c_str(), task->mSize);
		return;
	} else {
		resmap = itsize->second;
	}

	(*task->mpAttributes)[std::string("msresults")] = resmap;

}
