// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#include <limits>
#include <list>
#include <algorithm>
#include <unordered_map>
#include "CScheduleAlgorithmHEFT.h"
#include "CTask.h"
#include "CTaskCopy.h"
#include "CEstimation.h"
#include "CSchedule.h"
#include "CLogger.h"
using namespace sched::algorithm;
using sched::task::CTask;
using sched::schedule::STaskEntry;
using sched::schedule::CSchedule;



CScheduleAlgorithmHEFT::CScheduleAlgorithmHEFT(std::vector<CResource*>& rResources)
	: CScheduleAlgorithm(rResources)
{
	mpEstimation = CEstimation::getEstimation();
}

CScheduleAlgorithmHEFT::~CScheduleAlgorithmHEFT() {
	delete mpEstimation;
	mpEstimation = 0;
}

int CScheduleAlgorithmHEFT::init() {
	return 0;
}

void CScheduleAlgorithmHEFT::fini() {
}

CSchedule* CScheduleAlgorithmHEFT::compute(std::vector<CTaskCopy>* pTasks, std::vector<CTaskCopy*>* runningTasks, volatile int* interrupt, int updated) {

	int tasks = pTasks->size();
	int machines = mrResources.size();

	CSchedule* sched = new CSchedule(tasks, machines, mrResources);
	sched->mpOTasks = pTasks;

	// generate map for global id to local id conversion
	std::unordered_map<int,int> taskmap;
	for (unsigned int i=0; i<pTasks->size(); i++) {
		taskmap[(*pTasks)[i].mId] = i;
	}

	// average execution costs
	double* w = new double[tasks]();
	for (int tix = 0; tix < tasks; tix++) {
		double tix_w = 0.0;
		CTask* task = &(*pTasks)[tix];
		int validmachines = 0;
		for (int mix = 0; mix < machines; mix++) {
			CResource* res = mrResources[mix];
			if (task->validResource(res) == false) {
				continue;
			}
			tix_w += mpEstimation->taskTimeInit(task, res) +
					 mpEstimation->taskTimeCompute(task, res, task->mProgress, task->mCheckpoints) +
					 mpEstimation->taskTimeFini(task, res);
			validmachines++;
		}
		w[tix] = tix_w / validmachines;
	}

	// upward ranks
	double* upward = new double[tasks]();
	int changed = 0;
	do {
		changed = 0;
		for (int tix = 0; tix < tasks; tix++) {
			if (upward[tix] != 0) {
				continue;
			}
			CTask* task = &(*pTasks)[tix];
			// check successors
			int unmarked = 0;
			double max_succ_rank = 0.0;
			for (int succIx = 0; succIx < task->mSuccessorNum; succIx++) {
				int succId = task->mpSuccessorList[succIx];
				// find local task index
				int ix = -1;
				std::unordered_map<int,int>::const_iterator ix_it = taskmap.find(succId);
				if (taskmap.end() != ix_it) {
					ix = ix_it->second;
				}

/* Replaced by map lookup
				for (int j = 0; j<tasks; j++) {
					if ((*pTasks)[j].mId == succId) {
						ix = j;
						break;
					}
				}
*/
				if (ix == -1) {
					continue;
				}
				CLogger::mainlog->debug("HEFT tix %d succ %d", tix, ix);
				if (upward[ix] == 0.0) {
					unmarked++;
					break;
				}
				if (upward[ix] > max_succ_rank) {
					max_succ_rank = upward[ix];
				}
			}
			if (unmarked > 0) {
				continue;
			}
			// compute upward rank
			CLogger::mainlog->debug("HEFT compute upward rank for task %d w %f max_succ_rank %f succ_num %d", tix, w[tix], max_succ_rank, task->mSuccessorNum);
			upward[tix] = w[tix] + max_succ_rank;
			changed++;
		}
		
	} while(changed > 0);

	std::list<int> priorityListIx;
	for (int tix = 0; tix < tasks; tix++) {
		priorityListIx.push_back(tix);
	}

//	std::sort(priorityList.begin(), priorityList.end(), 
//		[upward](int i, int j) {
//			return upward[i] < upward[j];
//		}
//	);
	// sort by decreasing upward rank
	priorityListIx.sort( 
		[upward](int i, int j) {
			return upward[i] > upward[j];
		}
	);
	/*
	std::list<CTaskCopy*> priorityList;
	std::list<int>::iterator it = priorityListIx.begin();
	for (int tix = 0; tix < tasks; tix++) {
		priorityList.push_back(&((*pTasks)[*it]));
		CLogger::mainlog->debug("HEFT task %d upward rank %f", (*pTasks)[*it].mId, upward[*it]);
		it++;
		
	}
	*/

	// machine ready times
	double* R = new double[machines]();
	std::list<HEFTEntry>* entries = new std::list<HEFTEntry>[machines]();

	int listTasks = tasks;
	// for all elements in priority list
	while (listTasks > 0) {

		int ix = priorityListIx.front();
		priorityListIx.pop_front();
		CTaskCopy* task = &(*pTasks)[ix];
		listTasks--;

		// get finish time of predecessor tasks
		double preFinish = 0.0;
		int preNum = 0;
		for (int pix = 0; pix<task->mPredecessorNum; pix++) {
			int pid = task->mpPredecessorList[pix];
			if (taskmap.count(pid) > 0) {
				preNum++;
			}
		}
		if (preNum > 0) {
			CLogger::mainlog->debug("HEFT: tid %d preNum %d", task->mId, preNum);
			for (int pmix = 0; pmix < machines; pmix++) {
				std::list<HEFTEntry>::iterator pit = entries[pmix].end();
				do {
					pit--;
					if (pit == entries[pmix].end()) {
						break;
					}
					int pid = (*pit).taskid;
					// check if entry is predecessor
					for (int pix = 0; pix < task->mPredecessorNum; pix++) {
						if (task->mpPredecessorList[pix] == pid) {
							CLogger::mainlog->debug("HEFT: pred stop %d %lf", pid, (*pit).stop);
							if ((*pit).stop > preFinish) {
								preFinish = (*pit).stop;
							}
							preNum--;
							break;
						}
					}
					if (preNum == 0) {
						break;
					}
					//pit--;
				}
				while(pit != entries[pmix].begin()) ;
				if (preNum == 0) {
					break;
				}
			}
		}
		CLogger::mainlog->debug("HEFT: tid %d preFinish %lf", task->mId, preFinish);

		// find slot with earliest finishing time over all machines
		int eft_mix = -1;
		double start = 0.0;
		double finish = 0.0;
		int eft_mix_slot_index = 0;
		std::list<HEFTEntry>::iterator eft_slot_it;
		// iterate over every machine
		for (int mix = 0; mix < machines; mix++) {
			CResource* res = mrResources[mix];

			if (std::find(task->mpResources->begin(),
					task->mpResources->end(), res)
				== task->mpResources->end()) {
				// task is not compatible with resource 
				continue;
			}

			std::list<HEFTEntry>::iterator it = entries[mix].begin();
			std::list<HEFTEntry>::iterator preit = it;
			// compute task duration for this machine
			// (independent of slot)
			double dur =
				mpEstimation->taskTimeInit(task, res) +
				mpEstimation->taskTimeCompute(task, res, task->mProgress, task->mCheckpoints) + 
				mpEstimation->taskTimeFini(task, res);

			// find first slot on this machine that
			// * is large enough for this task
			// * has a start time that is after the predecessor finishing time
			int slot_index = 0; // new slot is inserted before the element at slot_index
			double slot_start = 0.0; // first fitting slot start time
			double slot_stop = 0.0; // first fitting slot stop time
			do {
				if (it != entries[mix].end()) {
					CLogger::mainlog->debug("HEFT: it start %lf stop %lf taskid %d", (*it).start, (*it).stop, (*it).taskid);
				}
				// entries for this machine are empty
				if (it == entries[mix].begin() &&
					it == entries[mix].end()) {
					slot_start = preFinish;
					slot_stop = preFinish + dur;
					slot_index = 0;
					break;
				} else
				// first entry
				if (it == entries[mix].begin()) {
					if((*it).start == 0.0) {
						// no time before first slot
						preit = it;
						it++;
						slot_index++;
						continue;
					}
					if (((*it).start-preFinish) >= dur) {
						// task fits before first slot
						slot_start = preFinish;
						slot_stop = preFinish + dur;
						break;
					}
				} else
				// end node
				if (it == entries[mix].end()) {
					// use slot after last one
					slot_start = (*preit).stop;
					CLogger::mainlog->debug("HEFT: pre start %lf", slot_start);
					if (slot_start < preFinish) {
						slot_start = preFinish;
					}
					slot_stop = slot_start + dur;
					break;
				} else
				// entry in the middle
				{
					// check if there is free time
					double last_stop = (*preit).stop;
					if (last_stop < preFinish) {
						last_stop = preFinish;
					}
					if ((*it).start - last_stop >= dur) {
						// enough time for task
						slot_start = last_stop;
						slot_stop = slot_start + dur;
						break;
					}
				}
				// advance iterator
				preit = it;
				it++;
				slot_index++;
			} while(true);// (it != entries[mix].end());
			CLogger::mainlog->debug("HEFT: tid %d mix %d stop %lf", task->mId, mix, slot_stop);

			// use found slot if it is better or the first machine
			if (eft_mix == -1 || slot_stop < finish) {
				eft_mix = mix;
				start = slot_start;
				finish = slot_stop;
				eft_mix_slot_index = slot_index;
				eft_slot_it = it;
			}
		}

		//STaskEntry* entry = new STaskEntry();
		//entry->taskcopy = task;
		//entry->taskid = entry->taskcopy->mId;
		//entry->stopProgress = entry->taskcopy->mCheckpoints;
		//(*(sched->mpTasks))[eft_mix]->push_back(entry);
		HEFTEntry hentry;
		hentry.taskix = ix;
		hentry.taskid = task->mId;
		hentry.start = start;
		hentry.stop = finish;
		//entries[eft_mix].insert(entries[eft_mix].begin()+eft_mix_slot_index, hentry);
		entries[eft_mix].insert(eft_slot_it, hentry);
		CLogger::mainlog->debug("HEFT: add tid %d to mix %d slot_index %d", task->mId, eft_mix, eft_mix_slot_index);

	}

	// copy entries from HEFTEntry lists to STaskEntry lists
	for (int mix = 0; mix < machines; mix++) {
		std::list<HEFTEntry>::iterator it = entries[mix].begin();
		while (it != entries[mix].end()) {
			CTaskCopy* task = &(*pTasks)[(*it).taskix];

			STaskEntry* entry = new STaskEntry();
			entry->taskcopy = task;
			entry->taskid = entry->taskcopy->mId;
			entry->stopProgress = entry->taskcopy->mCheckpoints;
			(*(sched->mpTasks))[mix]->push_back(entry);
			it++;
		}
	}
	

//	// completion time matrix
//	double* C = new double[tasks * machines]();
//	// list of unmapped tasks
//	// 0 -> unmapped
//	// 1 -> mapped
//	int* active = new int[tasks]();
//	// machine ready times
//	double* R = new double[machines]();
//	// array for chosen machine in first pass
//	//int* chosenMachine = new int[tasks];
//	
//	// fill matrix
//	for (int tix = 0; tix<tasks; tix++) {
//		CTask* task = (CTask*)  &((*pTasks)[tix]);
//		for (int mix = 0; mix<machines; mix++) {
//			CResource* res = mrResources[mix];
//
//			// completion time =
//			// ready time +
//			// init time +
//			// compute time +
//			// fini time
//			double complete = 
//				R[mix] +
//				mpEstimation->taskTimeInit(task, res) +
//				mpEstimation->taskTimeCompute(task, res, task->mProgress, task->mCheckpoints) + 
//				mpEstimation->taskTimeFini(task, res);
//			C[tix*machines + mix] = complete;
//		}
//	}
//
//	int unmappedTasks = tasks;
//
//	while (unmappedTasks > 0) {
//
//		// task with earliest completion time
//		int min_tix = 0;
//		int min_tix_mix = 0;
//		double min_tix_comp = std::numeric_limits<double>::max();
//
//		for (int tix = 0; tix<tasks; tix++) {
//			// skip already mapped tasks
//			if (active[tix] == 1) {
//				continue;
//			}
//			// machine with earliest completion time
//			int min_mix = 0;
//			double min_comp = C[tix*machines];
//			for (int mix = 1; mix<machines; mix++) {
//				double new_comp = C[tix*machines + mix];
//				if (new_comp < min_comp) {
//					min_comp = new_comp;
//					min_mix = mix;
//				}
//			}
//			//chosenMachine[tix] = min_mix;
//			if (min_comp < min_tix_comp) {
//				min_tix = tix;
//				min_tix_mix = min_mix;
//				min_tix_comp = min_comp;
//			}
//		}
//
//		// add task to machine queue
//		STaskEntry* entry = new STaskEntry();
//		entry->taskcopy = &((*pTasks)[min_tix]);
//		entry->taskid = entry->taskcopy->mId;
//		entry->stopProgress = entry->taskcopy->mCheckpoints;
//		(*(sched->mpTasks))[min_tix_mix]->push_back(entry);
//
//		// save previous ready time
//		double old_ready = R[min_tix_mix];
//		// update ready time
//		double new_ready = min_tix_comp;
//		R[min_tix_mix] = new_ready;
//
//		active[min_tix] = 1;
//
//		unmappedTasks--;
//		if (unmappedTasks == 0) {
//			break;
//		}
//
//		// update completion time matrix
//		for (int tix = 0; tix<tasks; tix++) {
//			if (active[tix] == 1) {
//				continue;
//			}
//			// exchange old completion time with new completion time
//			C[tix*machines + min_tix_mix] = C[tix*machines + min_tix_mix] - old_ready + new_ready;
//		}
//
//	}
//
//	sched->mActiveTasks = tasks;
//
//	// clean up
//	delete[] C;
//	delete[] active;
//	delete[] R;

	//clean up
	delete[] w;
	delete[] upward;
	delete[] R;
	delete[] entries;


	sched->mActiveTasks = tasks;
	sched->computeTimes();

	return sched;
}
