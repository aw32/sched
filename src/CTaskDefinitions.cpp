// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#include <fstream>
#include <cstring>
#include "CTaskDefinitions.h"
#include "CLogger.h"
using namespace::sched::wrap;


CTaskDefinition::CTaskDefinition(
		char* name,
		char* wd,
		char** arg,
		char** env,
		char** res,
		int** sizes,
		bool subtask) :

	name(name),
	wd(wd),
	arg(arg),
	env(env),
	res(res),
	sizes(sizes),
	subtask(subtask) {

}

CTaskDefinition::~CTaskDefinition(){

	if (arg != 0) {
		delete[] arg;
		arg = 0;
	}

	if (env != 0) {
		delete[] env;
		env = 0;
	}

	if (res != 0) {
		delete[] res;
		res = 0;
	}

	if (sizes != 0) {

		for (int ix=0; sizes[ix]!=0; ix++) {
			delete[] sizes[ix];
		}

		delete[] sizes;
		sizes = 0;
	}

}



CTaskDefinitions::~CTaskDefinitions() {

	for (unsigned int ix=0; ix < mDefinitions.size(); ix++) {
		delete mDefinitions[ix];
	}

	mDefinitions.clear();

	if (mpJson != 0) {
		cJSON_Delete(mpJson);
		mpJson = 0;
	}

}


CTaskDefinitions* CTaskDefinitions::loadTaskDefinitions(char* filepath) {

	std::ifstream in(filepath);
	std::string contents((std::istreambuf_iterator<char>(in)), 
	    std::istreambuf_iterator<char>());
	const char* file_contents = contents.c_str();

	cJSON* json = cJSON_Parse(file_contents);

	if (json == 0) {

		CLogger::mainlog->error("TaskDefinitions json error");
		const char* errptr = cJSON_GetErrorPtr();
		if (errptr != 0) {
			CLogger::mainlog->debug("TaskDefinitions json error pos %d", (errptr-file_contents)/sizeof(char));
		}
		return 0;
	}

	if (cJSON_IsObject(json) == 0) {

		CLogger::mainlog->error("TaskDefinitions: JSON top element is not an object");
		cJSON_Delete(json);
		return 0;
	}

	int num = cJSON_GetArraySize(json);
	if (num <= 0) {
		CLogger::mainlog->error("TaskDefinitions: JSON array is empty");
		cJSON_Delete(json);
		return 0;
	}

	CTaskDefinitions* defs = new CTaskDefinitions();

	int error = 0;
	int current = 0;
	for (current = 0; current < num; current++) {
		cJSON* entry = cJSON_GetArrayItem(json, current);
		if (entry == 0 || cJSON_IsObject(entry) == 0) {
			CLogger::mainlog->error("TaskDefinitions: entry in array is not an object");
			error = 1;
			break;
		}

		CTaskDefinition* def = loadTaskDefinition(entry);
		if (def == 0) {
			CLogger::mainlog->error("TaskDefinitions: error on loading taskdef");
			error = 1;
			break;
		}

		defs->mDefinitions.push_back(def);
		CLogger::mainlog->debug("TaskDefinitions: taskdef loaded for %s", def->name);
	}

	if (error == 1) {
		delete defs;
		cJSON_Delete(json);
		return 0;
	}

	defs->mpJson = json;

	return defs;
}

CTaskDefinition* CTaskDefinitions::loadTaskDefinition(cJSON* json) {

	int error = 0;

	char* name = 0;
	bool subtask = false;
	char* wd = 0;
	char** arg = 0;
	char** env = 0;
	char** res = 0;
	int** sizes = 0;

	do {

		// name
		name = json->string;

		// subtask
		cJSON* subtask_obj = cJSON_GetObjectItem(json, "subtask");
		if (subtask_obj == 0 || cJSON_IsBool(subtask_obj) == 0) {
			CLogger::mainlog->error("TaskDefinitions: subtask is invalid");
			error = 1;
			break;
		}
		subtask = false;
		if (cJSON_IsTrue(subtask_obj) == true) {
			subtask = true;
		} else
		if (cJSON_IsFalse(subtask_obj) == true) {
			subtask = false;
		} else {
			CLogger::mainlog->error("TaskDefinitions: subtask is invalid");
			error = 1;
			break;
		}

		// working directory
		cJSON* wd_obj = cJSON_GetObjectItem(json, "wd");
		if (wd_obj == 0 || cJSON_IsString(wd_obj) == 0) {
			CLogger::mainlog->error("TaskDefinitions: wd is invalid");
			error = 1;
			break;
		}
		wd = cJSON_GetStringValue(wd_obj);
		if (wd == 0) {
			CLogger::mainlog->error("TaskDefinitions: wd is invalid");
			error = 1;
			break;
		}

		// arguments
		cJSON* arg_array = cJSON_GetObjectItem(json, "arg");
		if (arg_array == 0 || cJSON_IsArray(arg_array) == 0) {
			CLogger::mainlog->error("TaskDefinitions: arg is invalid");
			error = 1;
			break;
		}
		int num = cJSON_GetArraySize(arg_array);
		if (num <= 0) {
			CLogger::mainlog->error("TaskDefinitions: arg array is empty");
			error = 1;
			break;
		}
		int current = 0;
		arg = new char*[num+1]();
		for (current = 0; current < num; current++) {
			cJSON* entry = cJSON_GetArrayItem(arg_array, current);
			if (entry == 0 || cJSON_IsString(entry) == 0) {
				CLogger::mainlog->error("TaskDefinitions: arg array: entry in array is not a string");
				error = 1;
				break;
			}
			char* arg_str = cJSON_GetStringValue(entry);
			if (arg_str == 0) {
				CLogger::mainlog->error("TaskDefinitions: arg array: entry in array is invalid");
				error = 1;
				break;
			}

			arg[current] = arg_str;
		}
		if (error == 1) {
			break;
		}

		// env
		cJSON* env_array = cJSON_GetObjectItem(json, "env");
		if (env_array == 0 || cJSON_IsArray(env_array) == 0) {
			CLogger::mainlog->error("TaskDefinitions: env is invalid");
			error = 1;
			break;
		}
		num = cJSON_GetArraySize(env_array);
		current = 0;
		env = new char*[num+1]();
		for (current = 0; current < num; current++) {
			cJSON* entry = cJSON_GetArrayItem(env_array, current);
			if (entry == 0 || cJSON_IsString(entry) == 0) {
				CLogger::mainlog->error("TaskDefinitions: env array: entry in array is not a string");
				error = 1;
				break;
			}
			char* env_str = cJSON_GetStringValue(entry);
			if (env_str == 0) {
				CLogger::mainlog->error("TaskDefinitions: env array: entry in array is invalid");
				error = 1;
				break;
			}

			env[current] = env_str;
		}
		if (error == 1) {
			break;
		}

		// res
		cJSON* res_array = cJSON_GetObjectItem(json, "res");
		if (res_array == 0 || cJSON_IsArray(res_array) == 0) {
			CLogger::mainlog->error("TaskDefinitions: res is invalid");
			error = 1;
			break;
		}
		num = cJSON_GetArraySize(res_array);
		current = 0;
		res = new char*[num+1]();
		for (current = 0; current < num; current++) {
			cJSON* entry = cJSON_GetArrayItem(res_array, current);
			if (entry == 0 || cJSON_IsString(entry) == 0) {
				CLogger::mainlog->error("TaskDefinitions: res array: entry in array is not a string");
				error = 1;
				break;
			}
			char* res_str = cJSON_GetStringValue(entry);
			if (res_str == 0) {
				CLogger::mainlog->error("TaskDefinitions: res array: entry in array is invalid");
				error = 1;
				break;
			}

			res[current] = res_str;
		}
		if (error == 1) {
			break;
		}

		// sizes
		cJSON* sizes_obj = cJSON_GetObjectItem(json, "sizes");
		if (sizes_obj == 0 || cJSON_IsObject(sizes_obj) == 0) {
			CLogger::mainlog->error("TaskDefinitions: sizes is invalid");
			error = 1;
			break;
		}
		num = cJSON_GetArraySize(sizes_obj);
		current = 0;
		sizes = new int*[num+1]();
		for (current = 0; current < num; current++) {
			cJSON* entry = cJSON_GetArrayItem(sizes_obj, current);
			if (entry == 0 || cJSON_IsArray(entry) == 0) {
				CLogger::mainlog->error("TaskDefinitions: sizes array: entry in array is not an array");
				error = 1;
				break;
			}
			char* sizes_res = entry->string;
			int resix = -1;
			for (int ix=0; res[ix] != 0; ix++) {
				if (strcmp(res[ix], sizes_res) == 0) {
					resix = ix;
					break;
				}
			}
			if (resix == -1) {
				CLogger::mainlog->error("TaskDefinitions: sizes array: entry in array has unknown resource");
				error = 1;
				break;
			}
			int sizes_num = cJSON_GetArraySize(entry);
			int* res_sizes = new int[sizes_num+1]();
			int sizes_ix = 0;
			for (sizes_ix = 0; sizes_ix < sizes_num; sizes_ix++) {
				cJSON* sizes_entry = cJSON_GetArrayItem(entry, sizes_ix);
				if (sizes_entry == 0 || cJSON_IsNumber(sizes_entry) == 0) {
					CLogger::mainlog->error("TaskDefinitions: sizes array: entry in array is not a number");
					error = 1;
					break;
				}

				res_sizes[sizes_ix] = (int) sizes_entry->valuedouble;
			}
			sizes[resix] = res_sizes;
			if (error == 1) {
				break;
			}
		}

	} while(0);

	if (error == 1) {

		if (arg != 0) {
			delete[] arg;
			arg = 0;
		}

		if (env != 0) {
			delete[] env;
			env = 0;
		}

		if (res != 0) {
			delete[] res;
			res = 0;
		}

		if (sizes != 0) {
			for (int ix=0; sizes[ix] != 0; ix++) {
				delete[] sizes[ix];
			}
			delete[] sizes;
			sizes = 0;
		}

		return 0;
	}

	return new CTaskDefinition(
		name, wd, arg, env, res, sizes, subtask);

}

CTaskDefinition* CTaskDefinitions::findTask(char* name) {
	for (unsigned int i=0; i<mDefinitions.size(); i++) {
		CTaskDefinition* def = mDefinitions[i];
		if (strcmp(def->name, name) == 0) {
			return def;
		}
	}
	return 0;
}
