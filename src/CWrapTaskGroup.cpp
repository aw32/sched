// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#include <fstream>
#include <unistd.h>
#include <cstring>
#include <sys/wait.h>
#include <signal.h>
#include "CWrapTaskGroup.h"
#include "CTaskDefinitions.h"
#include "CTaskWrapper.h"
#include "CLogger.h"
#include "CTask.h"
//#include <stdlib.h> // for mtrace
using namespace sched::wrap;
using sched::task::ETaskState;

CWrapTaskApp::CWrapTaskApp(
	CTaskDefinitions& defs,
	char* name,
	int task_num,
	int* tasks,
	int** dependencies,
	bool exec,
	int size):
	mrTaskDefinitions(defs),
	name(name),
	task_num(task_num),
	tasks(tasks),
	dependencies(dependencies),
	exec(exec),
	size(size) {

}

CWrapTaskApp::~CWrapTaskApp(){

	if (started == true && stopped == false) {
		if (allFinished() == false) {
			CLogger::mainlog->info("WrapTaskGroup: stop all tasks");
			stop();
		}
	}

	if (started == true && joined == false) {
		join();
	}

	std::ostringstream ss;
	for (unsigned int i = 0; this->tasks[i] != -1; i++) {
		ss << tasks[i];
		if ((long) i < this->task_num-1) {
			ss << ",";
		}
	}
	std::string ss_str = ss.str();

	std::ostringstream task_ss;
    long long addedl = 0;
    long long startedl = 0;
    long long finishedl = 0;
    long long abortedl = 0;

	if (globalTasks.size() > 0) {
		CTaskWrapper* task = globalTasks[0];
		
		addedl = task->mTimes.Added.time_since_epoch().count();
		startedl = task->mTimes.Started.time_since_epoch().count();
		finishedl = task->mTimes.Finished.time_since_epoch().count();
		abortedl = task->mTimes.Aborted.time_since_epoch().count();

		task_ss << "{\"id\":" << task->mId << ",";
    	task_ss << "\"times\":{\"added\":" << addedl << ",";
		task_ss << "\"started\":" << startedl << ",";
		task_ss << "\"finished\":" << finishedl << ",";
		task_ss << "\"aborted\":" << abortedl << "}";
		task_ss << ",\"state\":\"" << task->stateStrings[task->mState] << "\"}";
	} else {
		task_ss << "{}";
	}
	std::string task_ss_str = task_ss.str();

	CLogger::eventlog->info("\"event\":\"WRAPAPP\","
		"\"tasks\":[%s],"
		"\"name\":\"%s\","
		"\"size\":%d,"
		"\"status\":%d,"
		"\"signaled\":%d,"
		"\"signaled_signal\":%d,"
		"\"endtask\":%s"
		,
		ss_str.c_str(),
		this->name,
		this->size,
		this->status,
		this->signaled,
		this->signaled_signal,
		task_ss_str.c_str()
	);

	if (tasks != 0) {
		delete[] tasks;
		tasks = 0;
	}

	if (dependencies != 0) {
		for (int ix=0; ix < task_num; ix++) {
			if (dependencies[ix] != 0) {
				delete[] dependencies[ix];
			}
		}
		delete[] dependencies;
		dependencies = 0;
	}
}

bool CWrapTaskApp::containsTask(int id) {
	for (int ix=0; tasks[ix]!=-1; ix++) {
		if (tasks[ix] == id) {
			return true;
		}
	}
	return false;
}

CTaskWrapper* CWrapTaskApp::getTaskWithId(int id) {
	for (unsigned int ix=0; tasks[ix]!=-1; ix++) {
		if (tasks[ix] == id) {
			if (ix < globalTasks.size()) {
				return globalTasks[ix];
			} else {
				return 0;
			}
		}
	}
	return 0;
}

bool CWrapTaskApp::allFinished() {
	bool allFinished = true;
	for (unsigned int ix=0; tasks[ix]!=-1; ix++) {

		CTaskWrapper* task = globalTasks[ix];

		if (task->mState != ETaskState::POST && 
			task->mState != ETaskState::ABORTED) {

			allFinished = false;
			return allFinished;
		}
	}
	return allFinished;
}

int CWrapTaskApp::start(int number, char* logprefix, char* wrapsocketpath) {

	CTaskDefinition* def = mrTaskDefinitions.findTask(name);

	if (def == 0) {
		CLogger::mainlog->debug("WrapTaskApp: app not found in definitions %s", name);
		return -1;
	}

	char* wd = def->wd;
	char** arguments = def->arg;
	char** oenv = def->env;

	CLogger::mainlog->debug("WrapTaskGroup: WD %s", wd);
	for (int i=0; arguments[i]!=0; i++) {
		CLogger::mainlog->debug("WrapTaskGroup: ARG %s", arguments[i]);
	}

	int envnum = 0;
	for (envnum=0; oenv[envnum]!=0; envnum++);

	// count application env
	int aenvnum = 0;
	for (aenvnum=0; environ[aenvnum]!=0; aenvnum++);

	char* env[envnum+aenvnum+3];
	//char* env[envnum+aenvnum+4]; // including mtrace
	for (int i=0; i<envnum+aenvnum+3; i++) {
		env[i] = 0;
	}

	int nextix = 0;
	// copy env entry pointers to final env array
	for (int i=0; i<envnum; i++) {
		if (strncmp("SCHED_TASKSIZE=",oenv[i],15) == 0) {
			continue;
		}
		if (strncmp("WRAP_SOCKET=",oenv[i],12) == 0) {
			continue;
		}
		if (strncmp("SCHED_SOCKET=",oenv[i],12) == 0) {
			continue;
		}
		env[nextix] = oenv[i];
		nextix++;
	}
	// copy application env entry pointers to final env array
	for (int i=0; i<aenvnum; i++) {
		if (strncmp("SCHED_TASKSIZE=",environ[i],15) == 0) {
			continue;
		}
		if (strncmp("WRAP_SOCKET=",environ[i],12) == 0) {
			continue;
		}
		if (strncmp("SCHED_SOCKET=",environ[i],12) == 0) {
			continue;
		}
		// check if already in list
		char* eqindex = (char*) index(environ[i], (int)(*"="));
		if (eqindex == 0) {
			// no "=" sign
			continue;
		}
		int maxstr = (eqindex-environ[i])/(sizeof(char));
		bool exist = false;
		for (int j=0; j<nextix; j++) {
			if (strncmp(env[j],environ[i],maxstr) == 0) {
				// already in env list
				exist = true;
				break;
			}
		}
		if (exist == true) {
			continue;
		}
		env[nextix] = environ[i];
		nextix++;
	}


	// add tasksize
	char tasksize_str[100] = {};
	snprintf(tasksize_str, 100, "SCHED_TASKSIZE=%d", size);
	env[nextix] = tasksize_str;
	nextix++;

	// add wrap socket path
	char wrapsocket_str[4096] = {};
	snprintf(wrapsocket_str, 4096, "SCHED_SOCKET=%s", wrapsocketpath);
	env[nextix] = wrapsocket_str;
	nextix++;

//	// add mtrace path
//	char mtrace_str[100] = {};
//	snprintf(mtrace_str, 100, "MALLOC_TRACE=%s_mtrace_%s_%d", wrapsocketpath, rindex(arguments[0], '/')+1,rand());
//	env[nextix] = mtrace_str;
//	nextix++;


	for (int i=0; env[i]!=0; i++) {
		CLogger::mainlog->debug("WrapTaskGroup: ENV %s", env[i]);
	}

	// prepare paths for logging
	char stdout_log[4096] = {};
	snprintf(stdout_log, 4096, "%s_%d.out", logprefix, number);
	char stderr_log[4096] = {};
	snprintf(stderr_log, 4096, "%s_%d.err", logprefix, number);


	// start process
	pid = fork();
	if (pid == -1) {
		// fork failed
		int error = errno;
		errno = 0;
		CLogger::mainlog->debug("WrapTaskApp: execute %s failed %d %s", name, error, strerror(error));
		return -1;
	} else
	if (pid == 0) {

		// open stdout/stderr files
		int stdout_fd = open(stdout_log, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
		if (stdout_fd == -1) {
			int error = errno;
			errno = 0;
			printf("Failed to open stdout file %s %d %s\n", stdout_log, error, strerror(error));
			fflush(stdout);
			_exit(1);
		}

		int stderr_fd = open(stderr_log, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
		if (stderr_fd == -1) {
			int error = errno;
			errno = 0;
			printf("Failed to open stderr file %s %d %s\n", stderr_log, error, strerror(error));
			fflush(stdout);
			_exit(1);
		}

		// redirect stdout/stderr to files
		int ret = dup2(stdout_fd, STDOUT_FILENO);
		if (ret == -1) {
			int error = errno;
			errno = 0;
			printf("Failed to redirect stdout file %s %d %s\n", stdout_log, error, strerror(error));
			fflush(stdout);
			_exit(1);
		}
		close(stdout_fd);

		ret = dup2(stderr_fd, STDERR_FILENO);
		if (ret == -1) {
			int error = errno;
			errno = 0;
			printf("Failed to redirect stderr file %s %d %s\n", stderr_log, error, strerror(error));
			fflush(stdout);
			_exit(1);
		}
		close(stderr_fd);

		// child
		execve(arguments[0], (char* const*) arguments, (char* const*) env);
		// exec should never return
		_exit(1);
	} else {
		// parent
		CLogger::mainlog->debug("WrapTaskApp: started process %d", pid);
		started = true;
		return 0;
	}
}


void CWrapTaskApp::stop() {
	if (started == true && stopped == false) {
		CLogger::mainlog->debug("WrapTaskApp: stop %s", name);
		stopped = true;
		int ret = kill(pid, SIGTERM);
		if (-1 == ret) {
			int error = errno;
			errno = 0;
			CLogger::mainlog->debug("WrapTaskApp: %s kill failed %d %s", name, error, strerror(error));
		}

	}
}

void CWrapTaskApp::join() {
	if (started == true && joined == false) {
		CLogger::mainlog->debug("WrapTaskApp: join %s", name);
		joined = true;
		int status_info = 0;
		pid_t ret = waitpid(pid, &status_info, 0);
		// TODO: add timeout
		if (-1 == ret) {
			int error = errno;
			errno = 0;
			CLogger::mainlog->debug("WrapTaskApp: %s waitpid failed %d %s", name, error, strerror(error));
		} else {
			stopped = true;
			this->status = WEXITSTATUS(status_info);
			this->signaled = WIFSIGNALED(status_info);
			this->signaled_signal = 0;
			if (signaled) {
				this->signaled_signal = WTERMSIG(status_info);
			}
			CLogger::mainlog->debug("WrapTaskApp: %s pid %d exit code %d exit by signal %d exit-signal %d", name, ret, status, signaled, signaled_signal);
		}
	}
}

int CWrapTaskApp::joinIfDead() {
	if (started == true && joined == false) {
		int status_info = 0;
		int options = WNOHANG;
		pid_t ret = waitpid(pid, &status_info, options);
		if (-1 == ret) {
			int error = errno;
			errno = 0;
			CLogger::mainlog->debug("WrapTaskApp: %s waitpid failed %d %s", name, error, strerror(error));
			return -1;
		} else {
			if (0 == ret) {
				// process is not yet dead
				return -1;
			} else {
				joined = true;
				stopped = true;
				this->status = WEXITSTATUS(status_info);
				this->signaled = WIFSIGNALED(status_info);
				this->signaled_signal = 0;
				if (signaled) {
					this->signaled_signal = WTERMSIG(status_info);
				}
				CLogger::mainlog->debug("WrapTaskApp: %s pid %d exit code %d exit by signal %d exit-signal %d", name, ret, status, signaled, signaled_signal);
				return 0;
			}
		}
	}
	return -1;
}



CWrapTaskGroup::~CWrapTaskGroup(){

	for (unsigned int ix=0; ix < mTaskApps.size(); ix++) {
		delete mTaskApps[ix];
	}
	mTaskApps.clear();

	if (mpJson != 0) {
		cJSON_Delete(mpJson);
		mpJson = 0;
	}
}

CWrapTaskGroup* CWrapTaskGroup::loadTaskGroups(char* groupFile, CTaskDefinitions& defs){

	CLogger::mainlog->info("TaskGroup: read group file %s", groupFile);

	std::ifstream in(groupFile);
	std::string contents((std::istreambuf_iterator<char>(in)), 
	    std::istreambuf_iterator<char>());
	const char* file_contents = contents.c_str();

	cJSON* json = cJSON_Parse(file_contents);

	if (json == 0) {

		CLogger::mainlog->error("TaskGroups json error");
		const char* errptr = cJSON_GetErrorPtr();
		if (errptr != 0) {
			CLogger::mainlog->debug("TaskGroups json error pos %d", (errptr-file_contents)/sizeof(char));
		}
		return 0;
	}

	if (cJSON_IsArray(json) == 0) {

		CLogger::mainlog->error("TaskGroups: JSON top element is not an array");
		cJSON_Delete(json);
		return 0;
	}

	int num = cJSON_GetArraySize(json);
	if (num <= 0) {
		CLogger::mainlog->error("TaskGroups: JSON array is empty");
		cJSON_Delete(json);
		return 0;
	}

	CWrapTaskGroup* group = new CWrapTaskGroup();

	int error = 0;
	int current = 0;
	for (current = 0; current < num; current++) {
		cJSON* entry = cJSON_GetArrayItem(json, current);
		if (entry == 0 || cJSON_IsObject(entry) == 0) {
			CLogger::mainlog->error("TaskGroups: entry in array is not an object");
			error = 1;
			break;
		}

		CWrapTaskApp* app = loadTaskApp(entry, defs);
		if (app == 0) {
			CLogger::mainlog->error("TaskGroups: error on loading taskgroup");
			error = 1;
			break;
		}

		group->mTaskApps.push_back(app);
		CLogger::mainlog->debug("TaskGroups: task app loaded for %s", app->name);
	}

	if (error == 1) {
		delete group;
		cJSON_Delete(json);
		return 0;
	}

	group->mpJson = json;

	return group;
}


CWrapTaskApp* CWrapTaskGroup::loadTaskApp(cJSON* json, CTaskDefinitions& defs) {

	int error = 0;

	char* name = 0;
	int task_num = 0;
	int* tasks = 0;
	int** dependencies = 0;
	bool exec = false;
	int size = 0;

	do {

		// name
		cJSON* name_obj = cJSON_GetObjectItem(json, "name");
		if (name_obj == 0 || cJSON_IsString(name_obj) == 0) {
			CLogger::mainlog->error("TaskGroups: name is invalid");
			error = 1;
			break;
		}
		name = cJSON_GetStringValue(name_obj);

		// tasks
		cJSON* tasks_array = cJSON_GetObjectItem(json, "tasks");
		if (tasks_array == 0 || cJSON_IsArray(tasks_array) == 0) {
			CLogger::mainlog->error("TaskGroups: tasks is invalid");
			error = 1;
			break;
		}
		int num = cJSON_GetArraySize(tasks_array);
		if (num <= 0) {
			CLogger::mainlog->error("TaskGroups: tasks array is empty");
			error = 1;
			break;
		}
		task_num = num;
		tasks = new int[num+1]();
		tasks[num] = -1;
		int current = 0;
		for (current = 0; current < num; current++) {
			cJSON* entry = cJSON_GetArrayItem(tasks_array, current);
			if (entry == 0 || cJSON_IsNumber(entry) == 0) {
				CLogger::mainlog->error("TaskGroups: tasks array: entry in array is not a number");
				error = 1;
				break;
			}
			tasks[current] = (int) entry->valuedouble;
		}
		if (error == 1) {
			break;
		}

		// dependencies
		cJSON* dep_array = cJSON_GetObjectItem(json, "dependencies");
		if (dep_array == 0 || cJSON_IsArray(dep_array) == 0) {
			CLogger::mainlog->error("TaskGroups: dependencies is invalid");
			error = 1;
			break;
		}
		num = cJSON_GetArraySize(dep_array);
		if (num <= 0) {
			CLogger::mainlog->error("TaskGroups: dependencies array is empty");
			error = 1;
			break;
		}
		if (num != task_num) {
			CLogger::mainlog->error("TaskGroups: dependencies array size is not identical to number of tasks");
			error = 1;
			break;
		}
		dependencies = new int*[num+1]();
		for (current = 0; current < num; current++) {
			cJSON* entry = cJSON_GetArrayItem(dep_array, current);
			if (entry == 0 || cJSON_IsArray(entry) == 0) {
				CLogger::mainlog->error("TaskGroups: dependencies array: entry in array is not an array");
				error = 1;
				break;
			}
			int task_dep_num = cJSON_GetArraySize(entry);
			if (task_dep_num < 0) {
				CLogger::mainlog->error("TaskGroups: dependencies array: entry in array is invalid");
				error = 1;
				break;
			}
			if (task_dep_num == 0) {
				continue;
			}
			int* task_dep = new int[task_dep_num+1]();
			task_dep[task_dep_num] = -1;
			int task_dep_ix = 0;
			for (task_dep_ix = 0; task_dep_ix < task_dep_num; task_dep_ix++) {
				cJSON* task_dep_entry = cJSON_GetArrayItem(entry, task_dep_ix);
				if (task_dep_entry == 0 || cJSON_IsNumber(task_dep_entry) == 0) {
					
					CLogger::mainlog->error("TaskGroups: dependencies array: entry in array is invalid");
					error = 1;
					break;
				}
				task_dep[task_dep_ix] = (int) task_dep_entry->valuedouble;
			}
			dependencies[current] = task_dep;
			if (error == 1) {
				break;
			}
		}
		if (error == 1) {
			break;
		}

		// exec
		cJSON* exec_obj = cJSON_GetObjectItem(json, "exec");
		if (exec_obj == 0) {
			CLogger::mainlog->error("TaskGroups: exec is invalid");
			error = 1;
			break;
		}
		if (cJSON_IsTrue(exec_obj) != 0) {
			exec = true;
		} else
		if (cJSON_IsFalse(exec_obj) != 0) {
			exec = false;
		} else {
			CLogger::mainlog->error("TaskGroups: exec is not a bool");
			error = 1;
			break;
		}

		// size
		cJSON* size_obj = cJSON_GetObjectItem(json, "size");
		if (size_obj == 0 || cJSON_IsNumber(size_obj) == 0) {
			CLogger::mainlog->error("TaskGroups: size is invalid");
			error = 1;
			break;
		}
		size = (int) size_obj->valuedouble;
		
	} while(0);

	if (error == 1) {

		if (tasks != 0) {
			delete[] tasks;
			tasks = 0;
		}

		if (dependencies != 0) {
			for (int ix=0; ix < task_num; ix++) {
				if (dependencies[ix] != 0) {
					delete[] dependencies[ix];
				}
			}
			delete[] dependencies;
			dependencies = 0;
		}

		return 0;
	}

	CWrapTaskApp* app = new CWrapTaskApp(
		defs, name, task_num, tasks, dependencies, exec, size);

	return app;
	
}
