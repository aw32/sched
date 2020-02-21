// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#include <fstream>
#include <cstring>
#include <signal.h>
#include <unistd.h>
#include <sys/syscall.h>
#include "cjson/cJSON.h"
#include "CSimQueue.h"
#include "CResource.h"
#include "CFeedback.h"
#include "CTaskWrapper.h"
#include "CTaskDatabase.h"
#include "CScheduleComputerMain.h"
#include "CScheduleExecutorMain.h"
#include "CSchedule.h"
#include "CEstimation.h"
#include "CLogger.h"
#include "CSimMain.h"
#include "CConfig.h"
using namespace sched::sim;
using sched::schedule::CResource;
	const char* CSimEvent::eventTypeStrings[] = {
				"SIMEVENT_NEWTASK",
				"SIMEVENT_TASK_CHANGE",
				"SIMEVENT_TIMER_END",
				"SIMEVENT_ALGO_END"
			};

	const char* CSimTaskState::statusStrings[] = {
				"SIM_TASK_STATUS_IDLE",
				"SIM_TASK_STATUS_STARTING",
				"SIM_TASK_STATUS_WORKING",
				"SIM_TASK_STATUS_SUSPENDING",
				"SIM_TASK_STATUS_SUSPENDED",
				"SIM_TASK_STATUS_FINISHED"
			};

CSimQueue::CSimQueue(std::vector<CResource*>& rResources,CTaskDatabase& rTaskDatabase):
	mrResources(rResources),
	mrTaskDatabase(rTaskDatabase)
{


    for (unsigned int i=0; i<mrResources.size(); i++) {
        mrResources[i]->setFeedback(this);
    } 


	mpEstimation = CEstimation::getEstimation();

	this->mSimulationThread = std::thread(&CSimQueue::runSimulation, this);
}

int CSimQueue::init(){

	mpScheduleExecutor = new CScheduleExecutorMain(mrResources, mrTaskDatabase);


	mpScheduleComputer = new CScheduleComputerMain(mrResources, *this ,mrTaskDatabase);


	mpScheduleComputer->setScheduleExecutor(this);

	mpScheduleExecutor->setScheduleComputer(this);

	int ret = mpScheduleComputer->loadAlgorithm();
	if (-1 == ret) {
		return -1;
	}
	mpScheduleComputer->start();

	mpScheduleExecutor->start();
	return 0;
}

CSimQueue::~CSimQueue(){

	CLogger::mainlog->info("Simulation: ~");
	// in case simulation was not started or stopped
	if (mStartSimulation == 0) {
		stopSimulation();
		startSimulation(); // notify thread
	} else
	if (mStopSimulation == 0) {
		stopSimulation();
	}

	if (mSimulationThread.joinable()) {
		mSimulationThread.join();
	}

	delete mpEstimation;
	mpEstimation = 0;

	// clear events
	while(mQueue.begin() != mQueue.end()) {
		CSimEvent* event = *(mQueue.begin());
		mQueue.pop_front();
		delete event;
	}

	while(mPassedQueue.begin() != mPassedQueue.end()) {
		CSimEvent* event = *(mPassedQueue.begin());
		mPassedQueue.pop_front();
		delete event;
	}

	while(mTaskStates.empty() == false) {
		CSimTaskState* state = mTaskStates.back();
		mTaskStates.pop_back();
		delete state;
	}

	delete mpScheduleComputer;
	delete mpScheduleExecutor;
}

int CSimQueue::loadTaskEvents(){

	char* simfile = 0;

	// loading environment variable SCHED_SIMFILE
	char* envSimfile = std::getenv("SCHED_SIMFILE");

	// loading config key "simulation_file"
	std::string* configSimfile = 0;
	CConfig* config = CConfig::getConfig();
	int res = 0;
    res = config->conf->getString((char*)"simulation_file", &configSimfile);

	// checking loaded variables
	if (0 == res) {
        CLogger::mainlog->info("Simulation: config \"simulation_file\" found");
		simfile = (char*) configSimfile->c_str();
	} else {
        CLogger::mainlog->info("Simulation: config key \"simulation_file\" not found");
	}

	if (0 != envSimfile) {
        CLogger::mainlog->info("Simulation: environment variable SCHED_SIMFILE found");
		simfile = envSimfile;
	} else {
        CLogger::mainlog->info("Simulation: environment variable SCHED_SIMFILE not found");
	}

	if (0 == simfile) {
        CLogger::mainlog->error("Simulation: simulation file not found !");
		return -1;
	}

	CLogger::mainlog->info("Simulation: loading simulation file: %s", simfile);
	std::ifstream in(simfile);
	std::string contents((std::istreambuf_iterator<char>(in)), 
    std::istreambuf_iterator<char>());
	CLogger::mainlog->info("Simulation: reading simulation file %s", simfile);
	cJSON* sim = cJSON_Parse(contents.data());
	//cJSON* sim = cJSON_Parse(contents.c_str());

	if (sim == 0) {
		CLogger::mainlog->error("Simulation: JSON parsing error");
		return -1;
	}

	if (cJSON_IsArray(sim) == 0) {
		CLogger::mainlog->error("Simulation: JSON top element is not an array");
		cJSON_Delete(sim);
		return -1;
	}

	int num = cJSON_GetArraySize(sim);
	if (num <= 0) {
		CLogger::mainlog->error("Simulation: JSON array is empty");
		cJSON_Delete(sim);
		return -1;
	}

	int error = 0;
	int current = 0;
	int currentid = 0;
	for (current = 0; current < num; current++) {
		cJSON* entry = cJSON_GetArrayItem(sim, current);
		if (entry == 0 || cJSON_IsObject(entry) == 0) {
			CLogger::mainlog->error("Simulation: entry in array is not an object");
			error = 1;
			break;
		}
		cJSON* type_obj = cJSON_GetObjectItem(entry, "type");
		if (type_obj == 0 || cJSON_IsString(type_obj) == 0) {
			CLogger::mainlog->error("Simulation: type is invalid");
			error = 1;
			break;
		}
		char* type = cJSON_GetStringValue(type_obj);
		if (type == 0) {
			CLogger::mainlog->error("Simulation: type is invalid");
			error = 1;
			break;
		}
		if (strcmp("TASKDEF",type) == 0) {
			// read id
			cJSON* id_obj = cJSON_GetObjectItem(entry, "id");
			if (id_obj == 0 || cJSON_IsNumber(id_obj) == 0) {
				CLogger::mainlog->error("Simulation: id is invalid");
				error = 1;
				break;
			}
			int id = id_obj->valueint;

			// id check
			if (currentid != id) {
				CLogger::mainlog->error("Simulation: wrong task id");
				error = 1;
				break;
			}
			currentid++;

			// read name
			cJSON* name_obj = cJSON_GetObjectItem(entry, "name");
			if (name_obj == 0 || cJSON_IsString(name_obj) == 0) {
				CLogger::mainlog->error("Simulation: task name not found");
				error = 1;
				break;
			}
			std::string* name = new std::string(name_obj->valuestring);

			// read size
			cJSON* size_obj = cJSON_GetObjectItem(entry, "size");
			if (size_obj == 0 || cJSON_IsNumber(size_obj) == 0) {
				CLogger::mainlog->error("Simulation: task size invalid");
				error = 1;
				delete name;
				break;
			}
			int size = size_obj->valueint;

			// read checkpoints
			cJSON* checkpoints_obj = cJSON_GetObjectItem(entry, "checkpoints");
			if (checkpoints_obj == 0 || cJSON_IsNumber(checkpoints_obj) == 0) {
				CLogger::mainlog->error("Simulation: checkpoints invalid");
				error = 1;
				delete name;
				break;
			}
			int checkpoints = checkpoints_obj->valueint;

			// read dependencies
			int dep_num = 0;
			int* deplist = 0;
			cJSON* dep_arr = cJSON_GetObjectItem(entry, "dependencies");
			if (dep_arr != 0 && cJSON_IsArray(dep_arr) == 1) {
				dep_num = cJSON_GetArraySize(dep_arr);
				if (dep_num > 0) {
					deplist = new int[dep_num];
				}
				int dep_ix = 0;
				for (dep_ix = 0; dep_ix < dep_num; dep_ix++) {
					cJSON* dep_obj = cJSON_GetArrayItem(dep_arr, dep_ix);
					if (dep_obj == 0 || cJSON_IsNumber(dep_obj) == 0) {
						CLogger::mainlog->error("Simulation: dependencies array for task %d entry is invalid", id);
						error = 1;
						break;
					}
					int dep_id = dep_obj->valueint;
					deplist[dep_ix] = dep_id;
					if (dep_id >= id) {
						CLogger::mainlog->error("Simulation: id of dependency of task %d larger than (or equal to) tasks id", id);
						error = 1;
						break;
					}
				}
				if (dep_num > 0 && dep_ix < dep_num) {
					CLogger::mainlog->error("Simulation: dependencies invalid");
					error = 1;
				}
			}
			if (error == 1) {
				if (deplist != 0) {
					delete[] deplist;
				}
				delete name;
				break;
			}

			// read resources
			int res_num = 0;
			
			cJSON* res_arr = cJSON_GetObjectItem(entry, "resources");
			if (res_arr == 0 || cJSON_IsArray(res_arr) == 0) {
				CLogger::mainlog->error("Simulation: resources invalid");
				if (deplist != 0) {
					delete[] deplist;
				}
				delete name;
				error = 1;
				break;
			}
			res_num = cJSON_GetArraySize(res_arr);
			if (res_num == 0) {
				CLogger::mainlog->error("Simulation: resources array empty");
				if (deplist != 0) {
					delete[] deplist;
				}
				delete name;
				error = 1;
				break;
			}
			std::vector<CResource*>* resources = new std::vector<CResource*>();
			for (int ri=0; ri<res_num; ri++) {
				cJSON* res_obj = cJSON_GetArrayItem(res_arr, ri);
				if (res_obj == 0 || res_obj->type != cJSON_String) {
					CLogger::mainlog->error("Simulation: resources array entry invalid");
					error = 1;
					break;
				}
				char* res_name = res_obj->valuestring;
				int res_id = -1;
				// find resource
				for (unsigned int rj=0; rj<mrResources.size(); rj++) {
					if (strcmp(mrResources[rj]->mName.c_str(), res_name) == 0) {
						res_id = rj;
						break;
					}
				}
				if (res_id == -1) {
					CLogger::mainlog->error("Simulation: resource %s unknown", res_name);
					error = 1;
					break;
				}
				resources->push_back(mrResources[res_id]);
			}
			if (error == 1 || resources->size() == 0) {
				CLogger::mainlog->error("Simulation: resources invalid");
				if (deplist != 0) {
					delete[] deplist;
				}
				delete name;
				delete resources;
				error = 1;
				break;
			}

			CTaskWrapper* taskwrap = new CTaskWrapper(name, size, checkpoints, resources, deplist, dep_num, this, mrTaskDatabase);
			mInputTasks.push_back(taskwrap);
			mTaskStates.push_back(new CSimTaskState(taskwrap));
		} else
		if (strcmp("TASKREG",type) == 0) {
			// read time
			cJSON* time_obj = cJSON_GetObjectItem(entry, "time");
			if (time_obj == 0 || cJSON_IsNumber(time_obj) == 0) {
				CLogger::mainlog->error("Simulation: time invalid");
				error = 1;
				break;
			}
			long long time = (long long) time_obj->valuedouble;

			// read tasks
			cJSON* tasks_arr = cJSON_GetObjectItem(entry, "tasks");
			if (tasks_arr == 0 || cJSON_IsArray(tasks_arr) == 0) {
				CLogger::mainlog->error("Simulation: tasks invalid");
				error = 1;
				break;
			}
			int tasks_num = cJSON_GetArraySize(tasks_arr);
			if (tasks_num <= 0) {
				CLogger::mainlog->error("Simulation: tasks array empty");
				error = 1;
				break;
			}
			
			CSimTaskRegEvent* event = new CSimTaskRegEvent();

			// read tasks from array into event
			for(int current = 0; current < tasks_num; current++) {
				cJSON* entry = cJSON_GetArrayItem(tasks_arr, current);
				if (entry == 0 || cJSON_IsNumber(entry) == 0) {
					CLogger::mainlog->error("Simulation: tasks array entry invalid");
					error = 1;
					break;
				}

				int taskid = entry->valueint;
				if (taskid < 0 || taskid >= currentid) {
					// invalid id
					CLogger::mainlog->error("Simulation: tasks array entry is invalid id");
					error = 1;
					break;
				}
				CTaskWrapper* taskwrapper = mInputTasks[taskid];
				event->tasks.push_back(taskwrapper);
			}
			// rewrite predecessor ids to create valid tasklist
			for (unsigned int current = 0; current < event->tasks.size(); current++) {
				CTaskWrapper* task = event->tasks[current];
				int prenum = task->mPredecessorNum;
				if (prenum == 0) {
					continue;
				}
				for (int pix = 0; pix < prenum; pix++) {
					int oldid = task->mpPredecessorList[pix];
					// find new position of referenced task
					int newid = -1;
					for (unsigned int newpos=0; newpos < current; newpos++) {
						cJSON* entry = cJSON_GetArrayItem(tasks_arr, newpos);
						if (entry->valueint == oldid) {
							newid = newpos;
							break;
						}
					}
					if (newid == -1) {
						CLogger::mainlog->error("Simulation: dependency error in TASKREG event, task with index %d has dependency to task id %d that is not found in TASKREG list", current, oldid);
						error = 1;
						break;
					}
					task->mpPredecessorList[pix] = newid;
				}
				if (error == 1) {
					break;
				}
			}

			if (error == 1) {
				delete event;
				break;
			}

			std::chrono::duration<long long,std::nano> mtime(time);
			std::chrono::time_point<std::chrono::steady_clock> stime(mtime);
			event->time = stime;

			mInputEvents.push_back(event);
		}
	}

	if (error == 1) {
		while (mInputEvents.size() > 0) {
			CSimEvent* entry = mInputEvents[mInputEvents.size()-1];
			mInputEvents.pop_back();
			delete entry;
		}
		while (mInputTasks.size() > 0) {
			CTaskWrapper* entry = mInputTasks[mInputTasks.size()-1];
			mInputTasks.pop_back();
			delete entry;
		}
		while (mTaskStates.size() > 0) {
			CSimTaskState* entry = mTaskStates[mTaskStates.size()-1];
			mTaskStates.pop_back();
			delete entry;
		}
		cJSON_Delete(sim);
		return -1;
	}

	// initialize queue with input events
	for(unsigned int index = 0; index < mInputEvents.size(); index++) {
		mQueue.push_back(mInputEvents[index]);
	}

	CLogger::mainlog->info("Simulation: Added %d tasks and %d events", mInputTasks.size(), mInputEvents.size());

	if (sim != 0) {
		cJSON_Delete(sim);
	}

	return 0;
}

void CSimQueue::addEvent(CSimEvent* event) {

	CLogger::mainlog->debug("Simulation: Add event %s at time %f", CSimEvent::eventTypeStrings[event->type], timeToSec(event->time));

	// linear search insert
	std::list<CSimEvent*>::iterator it = mQueue.begin();
	for (unsigned int index = 0; index < mQueue.size(); index++) {
		CSimEvent* current = *it;
		if (event->time < current->time) {
			if (index == 0) {
				// insert as first event
				mQueue.push_front(event);
				break;
			} else {
				// insert before current
				mQueue.insert(it, event);
				break;
			}
		}
		// increase iterator
		it++;
	}
	if (it == mQueue.end()) {
		// event not inserted
		// add to the back
		mQueue.push_back(event);
	}
}

void CSimQueue::getEventsByTime(std::list<CSimEvent*>* srcEvents, std::list<CSimEvent*>* events, double time){
	std::list<CSimEvent*>::iterator it = srcEvents->begin();
	for( ; it != srcEvents->end() ; it++) {
		if (timeToSec((*it)->time) == time) {
			events->push_back(*it);
		}
	}
}

void CSimQueue::getEventsByType(std::list<CSimEvent*>* srcEvents, std::list<CSimEvent*>* events, ESimEventType type){
	std::list<CSimEvent*>::iterator it = srcEvents->begin();
	for( ; it != srcEvents->end() ; it++) {
		if ((*it)->type == type) {
			events->push_back(*it);
		}
	}
}

int CSimQueue::countEventsByType(std::list<CSimEvent*>* events, ESimEventType type){
	
	int count = 0;
	std::list<CSimEvent*>::iterator it = mQueue.begin();
	for( ; it != mQueue.end() ; it++) {
		if ((*it)->type == type) {
			count++;
		}
	}
	return count;
}

CSimEvent* CSimQueue::mergeEventsByType(std::list<CSimEvent*>* events, ESimEventType type) {

	switch(type) {
		case ESimEventType::SIMEVENT_NEWTASK:
		{
			CSimTaskRegEvent* event = new CSimTaskRegEvent();
			// find events of type SIMEVENT_NEWTASK
			std::list<CSimEvent*>::iterator it = events->begin();
			for( ; it != events->end() ; it++) {
				if ((*it)->type == ESimEventType::SIMEVENT_NEWTASK) {
					CSimTaskRegEvent* regevent = (CSimTaskRegEvent*) (*it);
					// add tasks from event to new event
					for (unsigned int i=0; i<regevent->tasks.size(); i++) {
						event->tasks.push_back(regevent->tasks[i]);
					}
				}
			}
			return event;
		}
		break;
		default:
		// no merge for other event types yet
		break;
	}
	return 0;
}

void CSimQueue::removeEventsByType(std::list<CSimEvent*>* events, ESimEventType type, bool deleteEvent) {
	
	std::list<CSimEvent*>::iterator it = events->begin();
	while(it != events->end()) {
		if ((*it)->type == type) {
			// remove events
			if (deleteEvent) {
				delete (*it);
			}
			it = events->erase(it);
		} else {
			it++;
		}
	}
}

void CSimQueue::removeEventsByTypeAndTime(std::list<CSimEvent*>* events, ESimEventType type, double time, bool deleteEvent) {
	
	std::list<CSimEvent*>::iterator it = events->begin();
	while(it != events->end()) {
		if ((*it)->type == type && timeToSec((*it)->time) == time) {
			// remove events
			if (deleteEvent) {
				delete (*it);
			}
			it = events->erase(it);
		} else {
			it++;
		}
	}
}


void CSimQueue::mergeEvents(std::list<CSimEvent*>* events, bool deleteEvent){

	std::list<CSimEvent*> timeEvents;
	double time = -1.0;
	
	std::list<CSimEvent*>::iterator it = events->begin();
	while(it != events->end()) {

		int merged = 0;
		double newtime = timeToSec((*it)->time);

		// already seen time
		if (newtime == time) {
			it++;
			continue;
		}

		time = newtime;

		// get list of all events with this time
		getEventsByTime(events, &timeEvents, time);

		if (timeEvents.size() > 1) {

			int typeCount = 0;

			// try to merge events by type
			// SIMEVENT_NEWTASK
			typeCount = countEventsByType(&timeEvents, ESimEventType::SIMEVENT_NEWTASK);
			if (typeCount > 1) {
				int before = events->size();
				CSimEvent* merged_event = mergeEventsByType(&timeEvents, ESimEventType::SIMEVENT_NEWTASK);
				removeEventsByTypeAndTime(events, ESimEventType::SIMEVENT_NEWTASK, time, false);
				addEvent(merged_event);
				// clean event list for next event types
				// if the event objects should be deleted ($deleteEvent)
				// then the last pointers in $timedEvents are deleted
				removeEventsByType(&timeEvents, ESimEventType::SIMEVENT_NEWTASK, deleteEvent);
				merged = 1;
				int after = events->size();
				CLogger::mainlog->debug("Simulation: Merged %d NEWTASK events, before: %d after: %d", typeCount, before, after);
			}

		}

		// clear list
		timeEvents.clear();

		if (merged == 1) {
			// list was changed, reset iterator
			it = events->begin();;
		} else {
			// nothing changed, move on
			it++;
		}
	}

}

void CSimQueue::removeTaskChangeEvent(CSimTaskChangeEvent* taskevent){

	CTaskWrapper* task = taskevent->task;
	std::list<CSimEvent*>::iterator it = mQueue.begin();
	while (it != mQueue.end()) {
		CSimEvent* event = *it;
		if (event->type == ESimEventType::SIMEVENT_TASK_CHANGE) {
			CSimTaskChangeEvent* ittaskevent = (CSimTaskChangeEvent*) event;
			CTaskWrapper* ittask = ittaskevent->task;
			if (task == ittask) {
				it = mQueue.erase(it); // returns iterator following the erased one
				continue;
			}
		}
		it++;
	}
}

CSimTaskChangeEvent* CSimQueue::findTaskChangeEvent(CTaskWrapper* task) {
	
	std::list<CSimEvent*>::iterator it = mQueue.begin();
	while (it != mQueue.end()) {
		CSimEvent* event = *it;
		if (event->type == ESimEventType::SIMEVENT_TASK_CHANGE) {
			CSimTaskChangeEvent* ittaskevent = (CSimTaskChangeEvent*) event;
			if (ittaskevent->task == task) {
				return ittaskevent;
			}
		}
		it++;
	}
	return 0;
}

void CSimQueue::startSimulation(){
	{
		std::unique_lock<std::mutex> ul(mSimulationMutex);
		mStartSimulation = 1;
	}
	mSimulationCondVar.notify_one();
}

void CSimQueue::stopSimulation(){
	mStopSimulation = 1;
	mpScheduleComputer->stop();
	mpScheduleExecutor->stop();
	mNewScheduleInterrupt = true;
	mAlgoComputeCondVar.notify_one();
	mSimulationCondVar.notify_one();
	if (mSimulationThread.joinable() == true) {
		mSimulationThread.join();
	}
}

void CSimQueue::runSimulation(){

	pid_t pid = syscall(SYS_gettid);
	CLogger::mainlog->debug("Simulation: thread %d", pid);

	// mask signals for this thread
	int ret = 0;
	sigset_t sigset = {}; 
	ret = sigemptyset(&sigset);
	ret = sigaddset(&sigset, SIGINT);
	ret = sigaddset(&sigset, SIGTERM);
	//ret = sigprocmask(SIG_BLOCK, &sigset, 0); 
	ret = pthread_sigmask(SIG_BLOCK, &sigset, 0); 
	if (ret != 0) {
		CLogger::mainlog->error("Simulation: thread signal error: %s", strerror(errno));
	}

	// wait for start
	{
		std::unique_lock<std::mutex> ul(mSimulationMutex);
		while (mStartSimulation == 0 && mStopSimulation == 0) {
			mSimulationCondVar.wait(ul);
		}
	}

	if (mStopSimulation == 1) {
		return;
	}


	CLogger::mainlog->info("Simulation: Started");

	uint64_t sec = 0;
	uint64_t nsec = 0;
	CLogger::realtime(&sec, &nsec);
	CLogger::simlog->info("\"time\":\"%.9lf\", \"simevent\":\"SCHEDULER_START\",\"event\":\"SCHEDULER_START\",\"realtime\":\"%ld.%09ld\"", timeToSec(mCurrentTime), sec, nsec);

	// print resources
    std::ostringstream ss; 
    for (unsigned int i = 0; i<mrResources.size(); i++) {
        ss << "\"" << mrResources[i]->mName.c_str() << "\"";
        if (i < mrResources.size()-1) {
            ss << ",";
        }
    }   
    CLogger::simlog->info("\"time\":\"%.9lf\",\"simevent\":\"RESOURCES\",\"event\":\"RESOURCES\",\"resources\":[%s]", timeToSec(mCurrentTime), ss.str().c_str());

	// initial merge of events
	mergeEvents(&mQueue, true);


	while(mQueue.size() > 0 && mStopSimulation == 0) {

		// get next event
		CSimEvent* currentEvent = mQueue.front();
		mQueue.pop_front();


		// advance time
		mCurrentTime = currentEvent->time;
		CLogger::mainlog->debug("Simulation: Pick Event %s New Time %.9lf", 
			CSimEvent::eventTypeStrings[currentEvent->type], 
			timeToSec(mCurrentTime)
		);

		// execute event
		executeEvent(currentEvent);

		// add to passed events
		mPassedQueue.push_back(currentEvent);
	}

	double endsec = timeToSec(mCurrentTime);
	if (mStopSimulation == 1) {
		CLogger::simlog->info("\"time\":\"%.9lf\",\"simevent\":\"SCHEDULER_SIGNAL\",\"event\":\"SCHEDULER_SIGNAL\"", timeToSec(mCurrentTime));
	}

	// print endtask events
	for (unsigned int i=0; i<mInputTasks.size(); i++) {
		CTaskWrapper* task = mInputTasks[i];
		// check if task was registered
		if (task->mId == -1) {
			continue;
		}
	    long long addedl = task->mTimes.Added.time_since_epoch().count();
	    long long startedl = task->mTimes.Started.time_since_epoch().count();
	    long long finishedl = task->mTimes.Finished.time_since_epoch().count();
	    long long abortedl = task->mTimes.Aborted.time_since_epoch().count();
    	CLogger::simlog->info("\"time\":\"%.9lf\",\"simevent\":\"ENDTASK\",\"event\":\"ENDTASK\",\"id\":%d,\"times\":{\"added\":%ld,\"started\":%ld,\"finished\":%ld,\"aborted\":%ld},\"state\":\"%s\"",
			endsec,
	        task->mId,
	        addedl,
	        startedl,
	        finishedl,
	        abortedl,
    	    CTaskWrapper::stateStrings[task->mState]
		);
	}

	CLogger::simlog->info("\"time\":\"%.9lf\",\"simevent\":\"SCHEDULER_STOP\",\"event\":\"SCHEDULER_STOP\"", timeToSec(mCurrentTime));

	if (mrTaskDatabase.tasksDone() == false) {
		CLogger::mainlog->error("Simulation: No events but not all tasks are done");
	}

	if (mStopSimulation == 1) {
		CLogger::mainlog->info("Simulation: Stopped");
	} else {
		CLogger::mainlog->info("Simulation: Finished");
		CSimMain::shutdown();
	}

}

double CSimQueue::timeToSec(std::chrono::steady_clock::time_point time) {

	std::chrono::steady_clock::duration dur = time.time_since_epoch();
	return dur.count() / 1000000000.0;

}

void CSimQueue::computeNewSchedule(){

	// compute new schedule
	{
		std::unique_lock<std::mutex> ul(mAlgoComputeMutex);
		mpNewSchedule = 0;
		mNewScheduleInterrupt = false;
		// trigger new schedule
		mpScheduleComputer->computeSchedule();
		if (mpScheduleComputer->getRegisteredApplications() < mpScheduleComputer->getRequiredApplicationCount()) {
			CLogger::mainlog->info("Simulation: Schedule computation not ready because of low reqistered application number");
			// Return and ignore COMPUTER_ALGOSTART event
			return;
		}
		// wait for new schedule
		while (mpNewSchedule == 0 && mNewScheduleInterrupt == false) {
			mAlgoComputeCondVar.wait(ul);
			CLogger::mainlog->info("Simulation: comp wakeup");
		}
		// new schedule completed or interrupted
	}

	CLogger::simlog->info("\"time\":\"%.9lf\", \"simevent\":\"COMPUTER_ALGOSTART\",\"event\":\"COMPUTER_ALGOSTART\"",
		timeToSec(mCurrentTime));

	if (mNewScheduleInterrupt == true) {
		
		CLogger::mainlog->info("Simulation: Schedule computation interrupted");
		return;
	}
	CLogger::mainlog->info("Simulation: Schedule completed");

	// create new algo end event with new schedule
	CSimAlgorithmEndEvent* algoend_event = new CSimAlgorithmEndEvent();
	algoend_event->schedule = mpNewSchedule;
	mpNewSchedule = 0;
	algoend_event->time = mCurrentTime + algoend_event->schedule->mComputeDuration;

	// is there another schedule computation run at this point in time?
	std::list<CSimEvent*> events;
	getEventsByType(&mQueue, &events, ESimEventType::SIMEVENT_ALGO_END);
	if (events.size() == 1) {
		// a schedule computation is ongoing
		// new tasks arrived and triggered the current computation
		// -> let the current computation follow the ongoing one
		algoend_event->time = (*(events.begin()))->time + algoend_event->schedule->mComputeDuration;
	} else
	if (events.size() > 1) {
		// one schedule computation is ongoing
		// more computations are following (because of tasks arriving in the meantime)
		// -> replace the following computation with this new one, that includes all new tasks

		// remove the following computations by removing the SIMEVENT_ALGO_END entries (except the ongoing one)

		// let the current computation follow the ongoing one
		algoend_event->time = (*(events.begin()))->time + algoend_event->schedule->mComputeDuration;

		// remove the first entry from the temporary list	
		events.erase(events.begin());
		// remove all other entries from the main queue
		std::list<CSimEvent*>::iterator it = events.begin();
		for(;it != events.end(); it++) {
			// delete schedule, usually the schedule does not get deleted by the event destructor
			// since the schedule is processed and destructed later on
			CSimAlgorithmEndEvent* endevent = dynamic_cast<CSimAlgorithmEndEvent*> (*it);
			if (endevent != 0) { // actually this should always work
				delete endevent->schedule;
			}
			// delete event object
			delete (*it);
			// remove old entry from main queue
			mQueue.remove(*it);
		}

	}

	addEvent(algoend_event);

}

void CSimQueue::executeEvent(CSimEvent* event){
	switch(event->type) {
		case ESimEventType::SIMEVENT_NEWTASK:
		{
			CSimTaskRegEvent* taskreg_event = (CSimTaskRegEvent*) event;

			int ret = mrTaskDatabase.registerTasklist(&(taskreg_event->tasks));
			if (0 != ret) {
				// ?
			}
			CLogger::simlog->info("\"time\":\"%.9lf\", \"simevent\":\"SIMEVENT_NEWTASK\"", timeToSec(mCurrentTime));
			// print events for each task
			for (unsigned int i=0; i<taskreg_event->tasks.size(); i++) {
				CTaskWrapper* task = taskreg_event->tasks[i];

			    std::ostringstream ss;
    			for (unsigned int i=0; i<task->mpResources->size(); i++) {
			        ss << "\"" << (*(task->mpResources))[i]->mName.c_str() << "\"";
			        if (i<task->mpResources->size()-1) {
			            ss << ",";
			        }
			    }
				ss << "],\"dep\":[";
				std::vector<CTaskWrapper*>& deplist = task->getDependencies();
				for (unsigned int i=0; i<deplist.size(); i++) {
					ss << deplist[i]->mId;
					if (i<deplist.size()-1) {
						ss << ",";
					}
				}

				CLogger::simlog->info("\"time\":\"%.9lf\", \"simevent\":\"SIMEVENT_NEWTASK\",\"event\":\"NEWTASK\",\"id\":%d,\"res\":[%s],\"name\":\"%s\",\"size\":%d,\"checkpoints\":%d",
					timeToSec(mCurrentTime), task->mId, ss.str().c_str(), task->mpName->c_str(), task->mSize, task->mCheckpoints);
				
			}


			
			CLogger::mainlog->info("Simulation: Going to compute new schedule");

			computeNewSchedule();

		}
		break;	
		case ESimEventType::SIMEVENT_TASK_CHANGE:
		{
			CSimTaskChangeEvent* taskchange_event = (CSimTaskChangeEvent*) event;
			CTaskWrapper* task = taskchange_event->task;
			CSimTaskState* state = mTaskStates[task->mId];

			CLogger::simlog->info("\"time\":\"%.9lf\", \"simevent\":\"SIMEVENT_TASK_CHANGE\",\"id\":%d,\"oldstatus\":\"%s\",\"newstatus\":\"%s\",\"reached_checkpoint\":%d",
				timeToSec(mCurrentTime),
				task->mId,
				CSimTaskState::statusStrings[taskchange_event->oldStatus],
				CSimTaskState::statusStrings[taskchange_event->newStatus],
				taskchange_event->reached_checkpoint);

			switch (taskchange_event->newStatus) {
				case ESimTaskStatus::SIM_TASK_STATUS_WORKING:
				{
					state->status = ESimTaskStatus::SIM_TASK_STATUS_WORKING;
					state->start_time = mCurrentTime;
					double compute_sec = mpEstimation->taskTimeCompute(task, state->current_res, state->current_checkpoint, state->target_checkpoint);
					CSimTaskChangeEvent* newevent = new CSimTaskChangeEvent();
					newevent->task = task;
					newevent->oldStatus = ESimTaskStatus::SIM_TASK_STATUS_WORKING;
					newevent->newStatus = ESimTaskStatus::SIM_TASK_STATUS_SUSPENDING;
					std::chrono::duration<long long,std::nano> ntime((long long)(compute_sec*1000000000.0));
					newevent->reached_checkpoint = state->target_checkpoint;
					newevent->start_checkpoint = state->current_checkpoint;
					newevent->time = mCurrentTime + ntime;
					addEvent(newevent);
					state->current_res->taskStarted(*task);
					// resource does not call operationDone() after taskStarted()
					// therefore no reason to wait for executor loop
					CLogger::simlog->info("\"time\":\"%.9lf\",\"simevent\":\"TASK_STARTED\",\"event\":\"TASK_STARTED\",\"id\":%d,\"res\":\"%s\"",
						timeToSec(mCurrentTime),
						task->mId,
						state->current_res->mName.c_str());
				}	
				break;
				case ESimTaskStatus::SIM_TASK_STATUS_SUSPENDING:
				{
					state->status = ESimTaskStatus::SIM_TASK_STATUS_SUSPENDING;
					state->start_time = mCurrentTime;
					double fini_sec = mpEstimation->taskTimeFini(task, state->current_res);
					CSimTaskChangeEvent* newevent = new CSimTaskChangeEvent();
					newevent->task = task;
					newevent->oldStatus = ESimTaskStatus::SIM_TASK_STATUS_SUSPENDING;

					state->current_checkpoint = taskchange_event->reached_checkpoint;

					CLogger::mainlog->debug("Simulation: task change to SUSPENDING taskid %d reached %d / %d", task->mId, state->current_checkpoint, task->mCheckpoints);

					if (state->current_checkpoint == task->mCheckpoints) {
						newevent->newStatus = ESimTaskStatus::SIM_TASK_STATUS_FINISHED;
					} else {
						newevent->newStatus = ESimTaskStatus::SIM_TASK_STATUS_SUSPENDED;
					}

					std::chrono::duration<long long,std::nano> ntime((long long)(fini_sec*1000000000.0));
					newevent->time = mCurrentTime + ntime;
					addEvent(newevent);
				}
				break;
				case ESimTaskStatus::SIM_TASK_STATUS_SUSPENDED:
				{
					CLogger::simlog->info("\"time\":\"%.9lf\",\"simevent\":\"TASK_SUSPENDED\",\"event\":\"TASK_SUSPENDED\",\"id\":%d,\"progress\":%d",
						timeToSec(mCurrentTime),
						task->mId,
						state->current_checkpoint);

					state->status = ESimTaskStatus::SIM_TASK_STATUS_SUSPENDED;
					state->start_time = mCurrentTime;
					CResource* oldres = state->current_res;
					state->current_res = 0;
					int currentLoop = mpScheduleExecutor->getCurrentLoop();
					oldres->taskSuspended(*task, state->current_checkpoint);
					currentLoop = mpScheduleExecutor->getNextLoop(currentLoop);
				}
				break;
				case ESimTaskStatus::SIM_TASK_STATUS_FINISHED:
				{
					state->status = ESimTaskStatus::SIM_TASK_STATUS_FINISHED;
					state->start_time = mCurrentTime;
					CResource* oldres = state->current_res;
					state->current_res = 0;
					int currentLoop = mpScheduleExecutor->getCurrentLoop();
					oldres->taskFinished(*task);
					currentLoop = mpScheduleExecutor->getNextLoop(currentLoop);
					CLogger::simlog->info("\"time\":\"%.9lf\",\"simevent\":\"TASK_FINISHED\",\"event\":\"TASK_FINISHED\",\"id\":%d",
						timeToSec(mCurrentTime),
						task->mId);

				}
				break;
				case ESimTaskStatus::SIM_TASK_STATUS_IDLE:
				// this can never occure
				break;
				case ESimTaskStatus::SIM_TASK_STATUS_STARTING:
				// this is handled when the task is initially started
				break;
			}
		}
		break;
		
		case ESimEventType::SIMEVENT_TIMER_END:
		{
			CLogger::simlog->info("\"time\":\"%.9lf\",\"simevent\":\"SIMEVENT_TIMER_END\" !!!!!!!!!! !!!!!!! !!!!!!!!1",
				timeToSec(mCurrentTime));
		}
		break;

		case ESimEventType::SIMEVENT_ALGO_END:
		{
			CSimAlgorithmEndEvent* algoend_event = (CSimAlgorithmEndEvent*) event;
			
		    double nseconds = double(algoend_event->schedule->mComputeDuration.count())
    		    * std::chrono::steady_clock::period::num
		        / std::chrono::steady_clock::period::den;

			CLogger::simlog->info("\"time\":\"%.9lf\", \"simevent\":\"SIMEVENT_ALGO_END\",\"event\":\"COMPUTER_ALGOSTOP\",\"duration\":%f",
				timeToSec(mCurrentTime),
				nseconds);

			int currentLoop = mpScheduleExecutor->getCurrentLoop();
			CLogger::mainlog->info("going to update schedule %d", currentLoop);
			// update schedule
			std::ostringstream scheduleJson;
			algoend_event->schedule->printJson(scheduleJson);
			std::string jsonstr = scheduleJson.str();
			CLogger::simlog->info("\"time\":\"%.9lf\", \"event\":\"SCHEDULE\",\"schedule\":%s",
				timeToSec(mCurrentTime),
				jsonstr.c_str());

			CLogger::simlog->info("\"time\":\"%.9lf\",\"simevent\":\"EXECUTOR_NEWSCHEDULE\",\"event\":\"EXECUTOR_NEWSCHEDULE\",\"id\":%d",
				timeToSec(mCurrentTime),
				algoend_event->schedule->mId
			);
			mpScheduleExecutor->updateSchedule(algoend_event->schedule);
			CLogger::mainlog->info("sent update schedule");
			// wait for next loop
			currentLoop = mpScheduleExecutor->getNextLoop(currentLoop);
			CLogger::mainlog->info("schedule updated: %d", currentLoop);
		}
		break;
	}
}


// CComClient
int CSimQueue::start(CResource& resource, int targetProgress, ETaskOnEnd onEnd, CTaskWrapper& task) { 

	CLogger::mainlog->debug("Simulation: start task id %d target progress %d", task.mId, targetProgress);

	CLogger::simlog->info("\"time\":\"%.9lf\",\"simevent\":\"TASK_START\",\"event\":\"TASK_START\",\"id\":%d,\"res\":\"%s\"",
		timeToSec(mCurrentTime),
		task.mId,
		resource.mName.c_str());

	int id = task.mId;
	CSimTaskState* state = mTaskStates[id];

	if (state->status == ESimTaskStatus::SIM_TASK_STATUS_STARTING) {
		// task is starting, target checkpoint already set
		// reset target checkpoint and go on with already created TaskChangeEvent
		state->target_checkpoint = targetProgress;
		state->onend = onEnd;
		return 0;
	} else
	if (state->status == ESimTaskStatus::SIM_TASK_STATUS_WORKING) {
		// target checkpoint can be rewritten, but TaskChangeEvent has to be rewritten

		int oldTarget = state->target_checkpoint;
		int newTarget = targetProgress;
		int minTarget = oldTarget < newTarget ? oldTarget : newTarget;
		int maxTarget = oldTarget < newTarget ? newTarget : oldTarget;
		
		double diff = mpEstimation->taskTimeCompute(&task, state->current_res, minTarget, maxTarget);
		if (newTarget == minTarget) {
			diff = -diff;
		}

		std::chrono::duration<long long,std::nano> difftime((long long)(diff*1000000000.0));
		
		state->target_checkpoint = targetProgress;
		state->onend = onEnd;

		// find old event
		CSimTaskChangeEvent* event = findTaskChangeEvent(&task);
		if (event == 0) {
			CLogger::mainlog->error("Simulation: can't find event, that should be there!");
			return -1;
		}

		// remove event from queue
		mQueue.remove(event); // this does not destroy the object

		// rewrite 
		event->reached_checkpoint = targetProgress;
		event->time += difftime;

		// add event back into queue
		addEvent(event);
		return 0;
	} else
	if (state->status == ESimTaskStatus::SIM_TASK_STATUS_SUSPENDING ||
		state->status == ESimTaskStatus::SIM_TASK_STATUS_FINISHED) {
		// too late, do nothing
		return 0;
	}

	// task is idling, start task
	state->current_res = &resource;
	state->onend = onEnd;
	state->target_checkpoint = targetProgress;
	state->status = ESimTaskStatus::SIM_TASK_STATUS_STARTING;
	state->start_time = mCurrentTime;

	CSimTaskChangeEvent* event = new CSimTaskChangeEvent();
	event->task = &task;
	event->oldStatus = ESimTaskStatus::SIM_TASK_STATUS_STARTING;
	event->newStatus = ESimTaskStatus::SIM_TASK_STATUS_WORKING;
	// get estimated init time
	double init_sec = mpEstimation->taskTimeInit(&task, &resource);
	// get init time in milliseconds
	std::chrono::duration<long long,std::nano> ntime((long long )(init_sec*1000000000.0));
	// set target time
	event->time = mCurrentTime + ntime;

	// add new event to event queue
	addEvent(event);

	return 0;
}
int CSimQueue::suspend(CTaskWrapper& task) {
	int id = task.mId;
	CSimTaskState* state = mTaskStates[id];
	switch(state->status) {
		case ESimTaskStatus::SIM_TASK_STATUS_WORKING:
		{

			// find previous TaskChangeEvent
			CSimTaskChangeEvent* event = findTaskChangeEvent(&task);
			
			if (event == 0) {
				CLogger::mainlog->error("Simulation: can't find TaskChangeEvent that should be there for task %d", task.mId);
				return -1;
			}

			// rewrite event
			event->task = &task;
			event->oldStatus = state->status;
			event->newStatus = ESimTaskStatus::SIM_TASK_STATUS_SUSPENDING;
			// compute reached checkpoints
			std::chrono::duration<long long, std::nano> dur = mCurrentTime - state->start_time;
			long long ntime = dur.count();

			int start_checkpoint = state->current_checkpoint;
			if (start_checkpoint != event->start_checkpoint) {
				// checkpoint was changed (by progress event) since start of working phase
				start_checkpoint = event->start_checkpoint;
			}

			int checkpoints = mpEstimation->taskTimeComputeCheckpoint(&task, state->current_res, start_checkpoint, ntime/1000000000.0);
			// compute time after checkpoints+1 for status change event
			double change_sec = mpEstimation->taskTimeCompute(&task, state->current_res, 
				start_checkpoint + checkpoints, start_checkpoint + checkpoints + 1);
			std::chrono::duration<long long,std::nano> change_ntime((long long )(change_sec*1000000000.0));
			event->time = mCurrentTime + change_ntime;

			int new_checkpoint = start_checkpoint + checkpoints + 1;
			event->reached_checkpoint = new_checkpoint;

			mQueue.remove(event); // this does not destroy the object
			// remove old events for this task
			
			// readd new event
			addEvent(event);
		}
		break;
		case ESimTaskStatus::SIM_TASK_STATUS_STARTING:
		{
			// find old start finishing event for this task
			CSimTaskChangeEvent* event = findTaskChangeEvent(&task);
			if (event == 0) {
				CLogger::mainlog->error("Simulation: unable to find start finishing event for task %d", task.mId);
			} else {
				// change event to SUSPENDING
				event->newStatus = ESimTaskStatus::SIM_TASK_STATUS_SUSPENDING;
				event->reached_checkpoint = state->current_checkpoint;
			}
		}
		break;
		case ESimTaskStatus::SIM_TASK_STATUS_SUSPENDING:
		{
			// ignore since already suspending
		}
		break;
		case ESimTaskStatus::SIM_TASK_STATUS_SUSPENDED:
		{
			// ignore since already suspended
		}
		break;
		case ESimTaskStatus::SIM_TASK_STATUS_FINISHED:
		{
			// ignore since already finished
		}
		break;
		//TODO: other cases?
		default:
		{
				CLogger::mainlog->error("Simulation: TODO case %d not considered!!!!", state->status);
		}
		break;
	}
	return 0;
}

int CSimQueue::abort(CTaskWrapper& task) {
	CLogger::mainlog->error("Simulation: TODO abort for task %d not implemented", task.mId);
	return 0;
}

int CSimQueue::progress(CTaskWrapper& task) {
	CLogger::mainlog->error("Simulation: This method should never be called, progress requests should be handled by the CFeedback methods");
	return 0;
}

// CScheduleComputer
void CSimQueue::executorSuspended() {
	CLogger::simlog->info("\"time\":\"%.9lf\", \"simevent\":\"EXECUTOR_SUSPENDED\",\"event\":\"EXECUTOR_SUSPENDED\"",
		timeToSec(mCurrentTime));
	mpScheduleComputer->executorSuspended();
}

void CSimQueue::computeSchedule() {
	CLogger::simlog->info("\"time\":\"%.9lf\",\"simevent\":\"COMPUTER_UPDATE\",\"event\":\"COMPUTER_UPDATE\"", timeToSec(mCurrentTime));
	computeNewSchedule();	
}

int CSimQueue::getRequiredApplicationCount() {
	return mpScheduleComputer->getRequiredApplicationCount();
}

int CSimQueue::getRegisteredApplications() {
	return mpScheduleComputer->getRegisteredApplications();
}


// CScheduleExecutor
void CSimQueue::suspendSchedule() {
	CLogger::simlog->info("\"time\":\"%.9lf\", \"simevent\":\"EXECUTOR_SUSPEND\",\"event\":\"EXECUTOR_SUSPEND\"",
		timeToSec(mCurrentTime));
	mpScheduleExecutor->suspendSchedule();
}

void CSimQueue::updateSchedule(CSchedule* pSchedule) {

	if (pSchedule != 0) {
		{
			std::lock_guard<std::mutex> lg(mAlgoComputeMutex);
			mpNewSchedule = pSchedule;
		}
		mAlgoComputeCondVar.notify_one();
	} else {
		CLogger::simlog->info("\"time\":\"%.9lf\",\"simevent\":\"EXECUTOR_RESUME\",\"event\":\"EXECUTOR_RESUME\"",
			timeToSec(mCurrentTime)
		);
	}
}


// CFeedback
int CSimQueue::getProgress() {

	CLogger::mainlog->debug("Simulation: getProgress");
	for (unsigned int i=0; i<mrResources.size(); i++) {
	
		CTaskWrapper* task = 0;
		mrResources[i]->getStatus(0, 0, &task, 0);
		if (task == 0) {
			continue;
		}

		CLogger::mainlog->debug("Simulation: getProgress for resource %s and task %d", mrResources[i]->mName.c_str(), task->mId);
		CSimTaskState* state = mTaskStates[task->mId];

		switch(state->status) {
			case ESimTaskStatus::SIM_TASK_STATUS_WORKING:
			{
				// compute reached checkpoints
				std::chrono::duration<long long, std::nano> dur = mCurrentTime - state->start_time;
				long long ntime = dur.count();
				int checkpoints = mpEstimation->taskTimeComputeCheckpoint(task, state->current_res, state->current_checkpoint, ntime/1000000000.0);
				int progress = state->current_checkpoint + checkpoints;
				// shortcut resource
				//state->current_res->taskProgress(task, progress);
				task->gotProgress(progress);
				//mpFeedback->gotProgress(*(state->current_res));
			}
			break;
			default:
			{
				if (state->current_res != 0) {
					// shortcut resource
					//state->current_res->taskProgress(task, state->current_checkpoint);
					task->gotProgress(state->current_checkpoint);
					//mpFeedback->gotProgress(*(state->current_res));
				}
			}
			break;
		}
	} 


	return 0;
}

