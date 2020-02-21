// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#include "CScheduleExt.h"
#include "CTaskCopy.h"
#include "CResource.h"
#include "CLogger.h"
#include "CEstimation.h"
using namespace sched::schedule;
using sched::algorithm::CEstimation;

CScheduleExt::CScheduleExt(
	int tasks,
	int resources,
	std::vector<CResource*>& rResources,
	std::vector<CTaskCopy>* tasklist
) : 
CSchedule(tasks, resources, rResources)
{
	mpOTasks = tasklist;
	// fill maps
	for (unsigned int ix=0; ix<mpOTasks->size(); ix++) {
		CTaskCopy* task = &((*mpOTasks)[ix]);
		int id = task->mId;
		mTaskMap[id] = ix;
		mTaskReady[id] = 0.0;
		mTaskLastPart[id] = false;
		int preNum = 0;
		std::unordered_map<int,int>::const_iterator ix_it;
		for (int pix = 0; pix<task->mPredecessorNum; pix++) {
			int pid = task->mpPredecessorList[pix];
			// since tasks are topologically sorted in the tasklist
			// predecessor tasks are already added to the task map
			if (mTaskMap.count(pid) > 0) {
				preNum++;
			}
		}
		if (preNum == 0) {
			mTaskDep[id] = true;
		} else {
			mTaskDep[id] = false;
		}
	}
	mActiveTasks = tasklist->size();
}


CScheduleExt::~CScheduleExt(){

}

int CScheduleExt::resourceTasks(int mix) {
	return (*mpTasks)[mix]->size();
}

double CScheduleExt::taskReadyTimeResource(int tix, CResource* res, CEstimation* est) {

	CTaskCopy* task = &((*mpOTasks)[tix]);
	int taskid = task->mId;
	double taskReady = taskReadyTime(tix); // max finish time of task predecessors
	double taskFiniTime = est->taskTimeFini(task, res);
	int taskRunningMix = -1;
	// find out if task is already running
	for (unsigned int mix=0; mix<mrResources.size(); mix++) {
		if (mRunningTasks[mix] != 0 && mRunningTasks[mix]->mId == taskid) {
			taskRunningMix = mix;
			break;
		}
	}

	double resLastTaskReady = resourceReadyTime(res); // last finish time in resource queue
	// find out if resource has already running task
	double resRunningFiniTime = 0.0;
	if (mRunningTasks[res->mId] != 0) {
		resRunningFiniTime = est->taskTimeFini(mRunningTasks[res->mId], res);
	}

	// check different cases
	// new task already running on same resource
	if (taskRunningMix == res->mId) {
		// resource queue is empty
		if (resLastTaskReady == 0.0) {
			// new task will continue running, ready time is 0
			return 0.0;
		} else {
			// new task will stop and continue later on same resource
			return resLastTaskReady;
		}
	} else 
	if (taskRunningMix != -1) { // task already running on different resource
		double finalReadyTime = resLastTaskReady > resRunningFiniTime ? resLastTaskReady : resRunningFiniTime;
		finalReadyTime = taskFiniTime > finalReadyTime ? taskFiniTime : finalReadyTime;
		return finalReadyTime;
	} else { // task not running yet
		double finalReadyTime = resLastTaskReady > resRunningFiniTime ? resLastTaskReady : resRunningFiniTime;
		finalReadyTime = taskReady > finalReadyTime ? taskReady : finalReadyTime;
		return finalReadyTime;
	}

/*
	double max = taskReady>resourceReady?taskReady:resourceReady;
	if (runningTask != 0 && runningTask->mId == task->mId) {
		// task is already running on resource
		if (runningMix != res->mId) {
			// task should be mapped to other resource than running?
			// then consider fini time from original resource
			max = max > finiTime ? max : finiTime;
		}
		return max;
	} else {
		// task is not running
		return max;
	}
*/
}

double CScheduleExt::taskCompletionTime(int tix, CResource* res, int slot, double ready, double init, double compute, double fini) {

	CTaskCopy* task = &((*mpOTasks)[tix]);
	int taskid = task->mId;
	int runningMix = -1;

	// find out if task is already running
	for (unsigned int mix=0; mix<mrResources.size(); mix++) {
		if (mRunningTasks[mix] != 0 && mRunningTasks[mix]->mId == taskid) {
			runningMix = mix;
			break;
		}
	}

	if (runningMix == res->mId && slot == 0) {
		// task continues running on resource it already runs on
		return ready + compute + fini;
	} else {
		// task runs on a different resource
		return ready + init + compute + fini;
	}
}

void CScheduleExt::addEntry(STaskEntry* entry, CResource* res, int position) {

	// get resource index
	int mix = res->mId;
	std::vector<STaskEntry*>* queue = (*mpTasks)[mix];

	// compute times
	computeExecutionTime(entry, res, position);

	double ready = 0.0;
	// find previous part
	// get local index
	std::pair<
		std::unordered_map<int,STaskEntry*>::iterator,
		std::unordered_map<int,STaskEntry*>::iterator
	> entry_range = mTaskParts.equal_range(entry->taskid);
	STaskEntry* prevEntry = 0;
	std::unordered_map<int,STaskEntry*>::iterator entry_it = entry_range.first;
	while(entry_it != mTaskParts.end()) {
		int pTaskid = (*entry_it).first;
		// range contains pairs with different keys
		if (pTaskid != entry->taskid) {
			entry_it++;
			continue;
		}
		STaskEntry* pEntry = (*entry_it).second;
		if (pEntry->stopProgress == entry->startProgress) {
			prevEntry = pEntry;
		}
		entry_it++;
	}
	// previous part found ?
	if (prevEntry != 0) {
		ready = prevEntry->timeFinish.count() / 1000000000.0;
	}




	double resReady = 0.0;

	// find resource ready time for given position
	if (position == -1) {
		// add to end
		resReady = resourceReadyTime(res);
	} else {
		// add somewhere else
		if (position == 0) {
			resReady = 0.0;
		} else
		if ( (long) queue->size() > position-1) {
			STaskEntry* preEntry = (*queue)[position-1];
			resReady = preEntry->timeFinish.count() / 1000000000.0;
		} else {
			CLogger::mainlog->warn("ScheduleExt: addEntry invalid position %d for queue of length %d, max ok position: %d", position, queue->size(), queue->size());
			resReady = resourceReadyTime(res);
		}
	}


	CLogger::mainlog->debug("ScheduleExt: res ready %d %lf", res->mId, resReady);
	if (resReady > ready) {
		ready = resReady;
	}
	double taskReady = taskReadyTime(entry->taskcopy);
	if (taskReady > ready) {
		ready = taskReady;
	}
	CLogger::mainlog->debug("ScheduleExt: resulting ready time %lf", ready);
	entry->timeReady = std::chrono::duration<long long,std::nano>((long long)(ready * 1000000000.0));
	entry->timeFinish = entry->timeReady + entry->durTotal;
	CLogger::mainlog->debug("ScheduleExt: entry ready %ld dur %ld finish %ld", entry->timeReady.count(), entry->durTotal.count(), entry->timeFinish.count());

	// add to queue
	if (position == -1) {
		//position = queue->size();
		queue->push_back(entry);
	} else {
		std::vector<STaskEntry*>::iterator it = queue->begin()+position;
		queue->insert(it, entry);
	}

	CLogger::mainlog->debug("ScheduleExt: new res ready time %d %lf", res->mId, resourceReadyTime(res));


	// add to task exist map
	mTaskExist[entry->taskid] = true;

	// final part ?
	if (entry->stopProgress == entry->taskcopy->mCheckpoints) {
		mTaskLastPart[entry->taskid] = true;
		CTaskCopy* task = entry->taskcopy;
		double finish = entry->timeFinish.count() / 1000000000.0;
		// check satisfied dependencies
		for (int six=0; six<task->mSuccessorNum; six++) {
			int sid = task->mpSuccessorList[six];

			// task in task set ?
			if (mTaskMap.count(sid) == 0) {
				continue;
			}

			// update ready time
			if (mTaskReady[sid] < finish) {
				mTaskReady[sid] = finish;
			}

			// check if successor task is dependency free now
			CTaskCopy* stask = &((*mpOTasks)[mTaskMap[sid]]);
			bool free = true;
			for (int pix = 0; pix<stask->mPredecessorNum; pix++) {
				int pid = stask->mpPredecessorList[pix];			
				if (mTaskMap.count(pid) == 0) {
					continue;
				}
				if (mTaskLastPart[pid] == false) {
					free = false;
					break;
				}
			}
			if (free == true) {
				CLogger::mainlog->debug("CScheduleExt: task %d dep freed", sid);
				mTaskDep[sid] = true;
			}
		}
	}

	// add to part list
	std::pair<int, STaskEntry*> partEntry(entry->taskid, entry);
	mTaskParts.insert(partEntry);


}

double CScheduleExt::resourceReadyTime(CResource* res) {
	
	// get resource index
	return resourceReadyTime(res->mId);

}

double CScheduleExt::resourceReadyTime(int mix) {

	std::vector<STaskEntry*>* queue = (*mpTasks)[mix];
	if (queue->size() == 0) {
		return 0.0;
	}
	return (*queue)[queue->size()-1]->timeFinish.count() / 1000000000.0;

}


double CScheduleExt::taskReadyTime(CTask* task) {

	return mTaskReady[task->mId];
}

double CScheduleExt::taskReadyTime(int tix) {
	return taskReadyTime(&((*mpOTasks)[tix]));
}



bool CScheduleExt::taskLastPartMapped(int tix) {
	return taskLastPartMapped(&((*mpOTasks)[tix]));
}

bool CScheduleExt::taskLastPartMapped(CTask* task) {
	return mTaskLastPart[task->mId];
}

bool CScheduleExt::taskDependencySatisfied(CTask* task) {
	return mTaskDep[task->mId];
}

bool CScheduleExt::taskDependencySatisfied(int tix) {

	return taskDependencySatisfied(&((*mpOTasks)[tix]));
}


void CScheduleExt::copyEntries(CSchedule* old, int updated) {

	std::chrono::steady_clock::time_point currentTime =
		std::chrono::steady_clock::now();

	for (int mix=0; mix<mResourceNum; mix++) {
		std::vector<STaskEntry*>* queue = (*(old->mpTasks))[mix];
		CResource* res = mrResources[mix];

		for (unsigned int tix=0; tix<queue->size(); tix++) {
			STaskEntry* entry = (*queue)[tix];

			if (mTaskMap.count(entry->taskid) == 0) {
				// task already done
				continue;
			}
			int taskix = mTaskMap[entry->taskid];
			CTaskCopy* task = &((*mpOTasks)[taskix]);

			unsigned int progress = task->mProgress;
			if (entry->stopProgress < progress) {
				// this part is finished
				continue;
			}
			STaskEntry* newentry = new STaskEntry();
			newentry->taskcopy = task;
			newentry->taskid = task->mId;
			newentry->startProgress = entry->startProgress;
			newentry->stopProgress = entry->stopProgress;

			updateEntryProgress(newentry, task, res, currentTime, updated);
		
			addEntry(newentry, res, -1);
		}
	}
}

int CScheduleExt::findSlot(int mix, double dur, double start, int startSlot, double* slotStartOut, double* slotStopOut){

	std::vector<STaskEntry*>* queue = (*mpTasks)[mix];

	if (queue->size() == 0) {
		// queue is empty
		*slotStartOut = start;
		*slotStopOut = start + dur;
		return 0;
	}

	{
		for(unsigned int qix=0; qix<queue->size(); qix++) {
			STaskEntry* entry = (*queue)[qix];
			double entryFinish = entry->timeFinish.count() / 1000000000.0;
			double entryReady = entry->timeReady.count() / 1000000000.0;
			CLogger::mainlog->debug("ScheduleExt: findSlot QUEUE ix %d entry taskid %d ready %lf finish %lf",qix, entry->taskid, entryReady, entryFinish);
		}
	}

	int slot = -1;
	double slotStart = 0.0;
	double slotStop = 0.0;
	double lastFinish = 0.0;
	if (startSlot > 0) {
		STaskEntry* entry = (*queue)[startSlot - 1];
		lastFinish = entry->timeFinish.count() / 1000000000.0;
	}
	for (unsigned int ix=startSlot; ix<queue->size(); ix++) {
		STaskEntry* entry = (*queue)[ix];
		double entryFinish = entry->timeFinish.count() / 1000000000.0;
		double entryReady = entry->timeReady.count() / 1000000000.0;
		//CLogger::mainlog->debug("ScheduleExt: findSlot ix %d start %lf stop %lf taskid %d", ix, entryReady, entryFinish, entry->taskid);
		if (entryFinish < start) {
			// entry ends before start time
			// no slot before this entry
			lastFinish = entryFinish;
			continue;
		}
		if (ix == 0) {
			// first entry
			if (entryReady == 0.0) {
				// no slot before first entry
				lastFinish = entryFinish;
				continue;
			}
			if (entryReady-start >= dur) {
				// task fits before first slot
				slot = 0;
				slotStart = start;
				slotStop = start + dur;
				break;
			}
		} else {
			// other entry
			double newstart = lastFinish;
			if (newstart < start) {
				newstart = start;
			}
			if (entryReady-newstart >= dur) {
				CLogger::mainlog->debug("ScheduleExt: slot ix %d lastFinish %lf entryReady %lf task start %lf task dur %lf", ix, lastFinish, entryReady, start, dur);
				slotStart = newstart;
				slotStop = slotStart + dur;
				slot = ix;
				break;
			}
		}
		lastFinish = entryFinish;
	}
	if (slot == -1) {
		// no slot before machine ready time
		slot = queue->size();
		slotStart = lastFinish;
		if (slotStart < start) {
			slotStart = start;
		}
		slotStop = slotStart + dur;
	}
	*slotStartOut = slotStart;
	*slotStopOut = slotStop;
	return slot;
}
