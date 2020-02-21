// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#include <unordered_map>
#include "CSchedule.h"
#include "CLogger.h"
#include "CResource.h"
#include "CTask.h"
#include "CTaskCopy.h"
#include "CEstimation.h"
using namespace sched::schedule;
using sched::task::CTask;
using sched::task::ETaskState;
using sched::algorithm::CEstimation;

STaskEntry* CSchedule::getNextTaskEntry(CResource* res, int skip){

	if (mpTasks == 0) {
		return 0;
	}

	std::vector<STaskEntry*>* resTasks = (*mpTasks)[res->mId];
	STaskEntry* taskentry = 0;
	int skipped = 0;
	for (unsigned int i = 0; i<resTasks->size(); i++) {
		STaskEntry* entry = (*resTasks)[i];

		// safety check, this entry is useless and can be skipped
		if (entry->startProgress == entry->stopProgress) {
			entry->state = ETaskEntryState::DONE;
		}

		// taskentry already done?
		if (entry->state == ETaskEntryState::DONE) {
			//CLogger::mainlog->debug("Schedpick: %d entry task id %d entry DONE", res->mId, entry->taskid);
			continue;
		}
		CTaskCopy* ctask = entry->taskcopy;
		CTask* otask = ctask->getOriginal();
		// task already finished?
		if (otask->mState == ETaskState::POST) {
			//CLogger::mainlog->debug("Schedpick: %d entry task id %d task POST entry DONE", res->mId, entry->taskid);
			entry->state = ETaskEntryState::DONE;
			continue;
		}
		// task aborted?
		if (otask->mState == ETaskState::ABORTED) {
			//CLogger::mainlog->debug("Schedpick: %d entry task id %d task ABORTED entry ABORTED", res->mId, entry->taskid);
			entry->state = ETaskEntryState::ABORTED;
			continue;
		}
		// entry already done ?
		if (otask->mProgress >= entry->stopProgress) {
			entry->state = ETaskEntryState::DONE;
			continue;
		}
		// skipped enough tasks?
		if (skipped < skip) {
			//CLogger::mainlog->debug("Schedpick: %d entry task id %d skip", res->mId, entry->taskid);
			skipped++;
			continue;
		}
		taskentry = (*resTasks)[i];
		break;
	}

	return taskentry;

}

CSchedule::CSchedule(int tasks, int resources, std::vector<CResource*>& rResources) : mrResources(rResources) {
	mTaskNum = tasks;
	mResourceNum = resources;
	mpTasks = new std::vector<std::vector<STaskEntry*>*>(resources);
	for (int i=0; i<resources; i++) {
		(*mpTasks)[i] = new std::vector<STaskEntry*>();
	}
	mpEst = CEstimation::getEstimation();
}

CSchedule::~CSchedule() {
	if (mpTasks != 0) {
		for (unsigned int i = 0; i<mpTasks->size(); i++) {
			for (unsigned int j = 0; j<(*mpTasks)[i]->size(); j++) {
				delete (*((*mpTasks)[i]))[j];
			}
			delete (*mpTasks)[i];
		}
		delete mpTasks;
		mpTasks = 0;
	}
	if (mpOTasks != 0) {
		// simply delete vector
		// destructor will be called for all CTaskCopy objects
		//   only deleting correct members
		delete mpOTasks;
		mpOTasks = 0;
	}
	if (mpEst != 0) {
		delete mpEst;
		mpEst = 0;
	}
}

void CSchedule::setRunningTasks(std::vector<CTaskCopy*>* runningTasks) {
	mRunningTasks.assign(runningTasks->begin(), runningTasks->end());
}

int CSchedule::taskRunningResource(int tix) {

	CTaskCopy* task = &((*mpOTasks)[tix]);
	int taskid = task->mId;
	int runningMix = -1;

	// find out if task is already running
	for (unsigned int mix=0; mix<mrResources.size() && mix<mRunningTasks.size(); mix++) {
		if (mRunningTasks[mix] != 0 && mRunningTasks[mix]->mId == taskid) {
			runningMix = mix;
			break;
		}
	}
	return runningMix;
}

int CSchedule::taskRunningResourceTid(int tid) {

	int runningMix = -1;

	// find out if task is already running
	for (unsigned int mix=0; mix<mrResources.size() && mix<mRunningTasks.size(); mix++) {
		if (mRunningTasks[mix] != 0 && mRunningTasks[mix]->mId == tid) {
			runningMix = mix;
			break;
		}
	}
	return runningMix;
}


void CSchedule::printJson(std::ostream& out) {

	out << "{";
	out << "\"id\":" << mId;
	out << ",\"compute_start\":" << mComputeStart.time_since_epoch().count();
	out << ",\"compute_stop\":" << mComputeStop.time_since_epoch().count();
	out << ",\"compute_duration\":" << mComputeDuration.count();
	out << ",\"duration\":" << mDuration.count();
	out << ",\"active_tasks\":" << mActiveTasks;
	out << ",\"static_energy\":" << mStaticEnergy;
	out << ",\"dynamic_energy\":" << mDynamicEnergy;
	out << ",\"total_energy\":" << mTotalEnergy;

	out << ",\"tasks\":[";
	for (int res=0; res<mResourceNum; res++) {
		out << "[";
		int task_num = (*mpTasks)[res]->size();
		for (int index=0; index < task_num; index++) {
			STaskEntry* entry = 0;
			entry = (*((*mpTasks)[res]))[index];
			out << "{";
			out << "\"id\":" << entry->taskid;
			out << ",\"part\":" << entry->partNumber;
			out << ",\"start_progress\":" << entry->startProgress;
			out << ",\"stop_progress\":" << entry->stopProgress;
			out << ",\"current_progress\":" << entry->taskcopy->mProgress;
			out << ",\"current_state\":" << (int) entry->taskcopy->mState;
			out << ",\"duration_total\":" << entry->durTotal.count();
			out << ",\"time_ready\":" << entry->timeReady.count();
			out << ",\"time_finish\":" << entry->timeFinish.count();
			out << ",\"duration_total\":" << entry->durTotal.count();
			out << ",\"duration_init\":" << entry->durInit.count();
			out << ",\"duration_compute\":" << entry->durCompute.count();
			out << ",\"duration_fini\":" << entry->durFini.count();
			out << ",\"duration_break\":" << entry->durBreak.count();
			out << ",\"energy\":" << entry->energy;
			out << (index+1==task_num ? "}" : "},");
		}
		out << (res+1==mResourceNum ? "]" : "],");
	}
	out << "]";

	out << "}";

	for (int res=0; res<mResourceNum; res++) {
		std::ostringstream ss;
		int task_num = (*mpTasks)[res]->size();
		for (int index=0; index < task_num; index++) {
			
			STaskEntry* entry = 0;
			entry = (*((*mpTasks)[res]))[index];
			ss << entry->taskid << " ";
		}
		CLogger::mainlog->debug(ss.str());
	}
}

void CSchedule::computeTimes(){

	int machines = mResourceNum;

	// current progress in machine queues
	int cur_mix[machines];
	for (int mix=0; mix<machines; mix++) {
		cur_mix[mix] = 0;
	}

	// global id mapgs
	// resource index of the task
	std::unordered_map<int,int> taskRes;
	// index of task in the resource list
	// value is -1 if the task exist but was not yet updated
	std::unordered_map<int,int> taskExist;
	// number of migration parts per task
	std::unordered_map<int,int> partCount;
	// last seen stopProgress for task
	std::unordered_map<int,unsigned int> lastProgress;
	// last seen part number
	std::unordered_map<int,int> lastPart;
	// last seen part entry
	std::unordered_map<int,STaskEntry*> lastPartEntry;
	// parts list
	std::unordered_multimap<int,STaskEntry*> parts;

	int parts_num = 0;

	// fill maps
	for (int mix=0; mix<machines; mix++) {
		CResource* res = mrResources[mix];
		int mix_num = ((*mpTasks)[mix])->size();
		for (int rix=0; rix<mix_num; rix++) {
			STaskEntry* entry = (*((*mpTasks)[mix]))[rix];
			taskRes[entry->taskid] = -1;
			taskExist[entry->taskid] = -1;

			if (partCount.count(entry->taskid) == 0) {
				// not in map
				partCount[entry->taskid] = 1;
			} else {
				partCount[entry->taskid]++;
			}
			if (lastProgress.count(entry->taskid) == 0) {
				// not in map
				lastProgress[entry->taskid] = entry->startProgress;
			} else {
				if (lastProgress[entry->taskid] > entry->startProgress) {
					lastProgress[entry->taskid] = entry->startProgress;
				}
			}
			lastPart[entry->taskid] = -1;
			std::pair<int, STaskEntry*> partEntry(entry->taskid, entry);
			parts.insert(partEntry);
			// update execution time
			computeExecutionTime(entry, res, rix);
		}
		parts_num += mix_num;
	}

	int updated = 0;
	int updatedNum = 0;

	// go through machines and try to update unupdated entries
	do {

		updatedNum += updated; // from last iteration
		updated = 0;

		CLogger::mainlog->debug("Schedule: machine loop");
		for (int mix=0; mix<machines; mix++) {

			if (cur_mix[mix] == (long) ((*mpTasks)[mix])->size()) {
				// machine done
				continue;
			}

			STaskEntry* entry = (*((*mpTasks)[mix]))[cur_mix[mix]];

			CLogger::mainlog->debug("Schedule: check mix %d tid %d partCount %d entry->startProgress %d entry->stopProgress %d lastProgress %d", mix, entry->taskid, partCount[entry->taskid], entry->startProgress, entry->stopProgress, lastProgress[entry->taskid]);

			
			std::chrono::steady_clock::duration timeReady = std::chrono::duration<long long,std::nano>(0);

			// set ready time to previous entry finish time
			if (cur_mix[mix] > 0) {
				STaskEntry* prev_entry = (*((*mpTasks)[mix]))[cur_mix[mix]-1];
				timeReady = prev_entry->timeFinish;
			}

			bool partok = false;
			// check if this part is the first part
			if (partCount[entry->taskid] == 1) {
				// only one part
				entry->partNumber = 0;
				partok = true;
			} else {

				// check if there are x parts with lower stopProgress
				// where x is the number of already done parts
				std::pair<
					std::unordered_map<int,STaskEntry*>::iterator,
					std::unordered_map<int,STaskEntry*>::iterator
				> allparts = parts.equal_range(entry->taskid);
				std::unordered_map<int,STaskEntry*>::iterator entry_it = allparts.first;
				int lparts_count = 0;
				while(entry_it != parts.end()) {
					int pTaskid = (*entry_it).first;
					// range contains pairs with different keys
					if (pTaskid != entry->taskid) {
						entry_it++;
						continue;
					}
					STaskEntry* pEntry = (*entry_it).second;
					if (pEntry->stopProgress < entry->stopProgress) {
						CLogger::mainlog->debug("Schedule: check part dep count entry taskid %d", pEntry->taskid);
						lparts_count++;
					}
					entry_it++;
				}

				if (lastPart[entry->taskid]+1 < lparts_count) {
					// there are parts with lower stopProgress that has not been updated yet!
					CLogger::mainlog->debug("Schedule: part dep not met, %d parts with lower stopProgress, last part id: %d", lparts_count, lastPart[entry->taskid]);
					partok = false;
				} else
				if (lastProgress[entry->taskid] == entry->startProgress) {
					// this is the next part
					partok = true;
					
					if (lastPartEntry.count(entry->taskid) > 0) {
						if (lastPartEntry[entry->taskid]->timeFinish > timeReady) {
							timeReady = lastPartEntry[entry->taskid]->timeFinish;
						}
					}
				}
			}

			if (partok == false) {
				// part dependency not met
				CLogger::mainlog->debug("Schedule: part dep not met partCount %d lastProgress %d", partCount[entry->taskid], lastProgress[entry->taskid]);
				continue;
			}

			bool finalpart = false;
			if (entry->stopProgress == entry->taskcopy->mCheckpoints) {
				finalpart = true;
			}

			bool depok = false;
			// check if dependencies are met
			if (entry->taskcopy->mPredecessorNum == 0) {
				CLogger::mainlog->debug("Schedule: part dep ok");
				depok = true;
			} else {
				// task has dependencies
				//CLogger::mainlog->debug("Schedule: check deps for taskid %d", entry->taskid);
				depok = true;
				for (int six=0; six<entry->taskcopy->mPredecessorNum; six++) {
					// successor global task id
					int sid = entry->taskcopy->mpPredecessorList[six];
					if (taskExist.count(sid) == 0) {
						// not in map
						//CLogger::mainlog->debug("Schedule: >  dep taskid %d not in map taskExist", sid);
						CLogger::mainlog->debug("Schedule: dep sid %d not in map", sid);
						continue;
					}

					if (lastPartEntry.count(sid) == 0) {
						CLogger::mainlog->debug("Schedule: dep sid %d never seen a part", sid);
						depok = false;
						break;
					}
					STaskEntry* lastpart = lastPartEntry[sid];
					if (lastpart->stopProgress != lastpart->taskcopy->mCheckpoints) {
						// not yet finished
						CLogger::mainlog->debug("Schedule: dep sid %d not yet finished", sid);
						depok = false;
						break;
					}

					int succIx = taskExist[sid]; // taskExist contains ix of last part
					if (succIx == -1) {
						// task in schedule, but not yet updated
						CLogger::mainlog->debug("Schedule: dep sid %d not yet updated", sid);
						depok = false;
						break;
					}

					int smix = taskRes[sid];
					CLogger::mainlog->debug("Schedule: dep sid %d ready time check mix %d succIx %d", sid, smix, succIx);
					STaskEntry* stask = (*((*mpTasks)[smix]))[succIx];
					if (stask->timeFinish > timeReady) {
						timeReady = stask->timeFinish;
					}
					
				}
			}

			if (partok == false || depok == false) {
				CLogger::mainlog->debug("Schedule: dep not met");
				continue;
			}

			// update times
			entry->timeReady = timeReady;
			entry->timeFinish = timeReady + entry->durTotal;

			// update part maps
			entry->partNumber = lastPart[entry->taskid]+1;
			lastPart[entry->taskid]++;
			lastProgress[entry->taskid] = entry->stopProgress;
			lastPartEntry[entry->taskid] = entry;

			// entry was updated
			if (finalpart == true) {
				taskExist[entry->taskid] = cur_mix[mix];
				taskRes[entry->taskid] = mix;
			}
			CLogger::mainlog->debug("Schedule: entry updated tid %d mix %d slot %d start %ld stop %ld startP %d stopP %d", entry->taskid, mix, cur_mix[mix], entry->timeReady.count(), entry->timeFinish.count(), entry->startProgress, entry->stopProgress);
			cur_mix[mix]++;
			updated++;
			//CLogger::mainlog->error("Schedule: update taskid %d on mix %d", entry->taskid, mix);

		}

	} while(updated > 0);

	// check if all tasks were updated
	if (updatedNum != parts_num) {
		// this is pretty bad
		CLogger::mainlog->error("Schedule: only %d of %d parts were updated", updatedNum, parts_num);
	}


	std::chrono::steady_clock::duration finalTime = {};
	mStaticEnergy = 0.0;
	mDynamicEnergy = 0.0;
	// update break times
	for (int mix=0; mix<machines; mix++) {
		int mix_num = ((*mpTasks)[mix])->size();
		CResource* res = mrResources[mix];
		for (int rix=0; rix<mix_num; rix++) {
			STaskEntry* entry = (*((*mpTasks)[mix]))[rix];
			if (rix+1<mix_num) {
				
				STaskEntry* nextentry = (*((*mpTasks)[mix]))[rix+1];
				entry->durBreak = nextentry->timeReady - entry->timeFinish;
			}
			if (rix == mix_num-1) {
				if (entry->timeFinish > finalTime) {
					finalTime = entry->timeFinish;
				}
				mStaticEnergy += mpEst->resourceIdleEnergy(res, entry->timeFinish.count() / (double) 1000000000.0);
			}
			mDynamicEnergy += entry->energy;
		}
	}

	mDuration = finalTime;
	mTotalEnergy = mStaticEnergy + mDynamicEnergy;

}

void CSchedule::computeExecutionTime(STaskEntry* entry, CResource* res){
	computeExecutionTime(entry, res, -1);
}

void CSchedule::computeExecutionTime(STaskEntry* entry, CResource* res, int slot){


	long long initNs = (long long) (mpEst->taskTimeInit(entry->taskcopy, res) * 1000000000.0);
	double E_init = mpEst->taskEnergyInit(entry->taskcopy, res);

	// if slot is given, check if task is already running
	if (slot == 0) {
		int runMix = taskRunningResourceTid(entry->taskid);
		if (runMix == res->mId) {
			// omit init phase
			initNs = 0;
			E_init = 0.0;
		}
		CLogger::mainlog->debug("Schedule: check running task mix %d", runMix);
	}

	entry->durInit = std::chrono::duration<long long,std::nano>(initNs);

	long long computeNs = (long long) (mpEst->taskTimeCompute(entry->taskcopy, res, entry->startProgress, entry->stopProgress) * 1000000000.0);
	entry->durCompute = std::chrono::duration<long long,std::nano>(computeNs);

	long long finiNs = (long long) (mpEst->taskTimeFini(entry->taskcopy, res) * 1000000000.0);
	entry->durFini = std::chrono::duration<long long,std::nano>(finiNs);

	double E_compute = mpEst->taskEnergyCompute(entry->taskcopy, res, entry->startProgress, entry->stopProgress);
	double E_fini = mpEst->taskEnergyFini(entry->taskcopy, res);


	entry->durTotal = entry->durInit + entry->durCompute + entry->durFini;
	CLogger::mainlog->debug("Schedule: computeExecutionTime task %d res %d, single times init %lf compute 1 %lf fini %lf startP %d stopP %d", entry->taskid, res->mId, 
		mpEst->taskTimeInit(entry->taskcopy, res),
		mpEst->taskTimeCompute(entry->taskcopy, res, entry->startProgress, entry->stopProgress),
		mpEst->taskTimeFini(entry->taskcopy, res),
		 entry->startProgress, entry->stopProgress)
		;
	CLogger::mainlog->debug("Schedule: computeExecutionTime task %d res %d total dur %ld", entry->taskid, res->mId, entry->durTotal.count());



	entry->energy = E_init + E_compute + E_fini;
}

void CSchedule::updateEntryProgress(STaskEntry* entry, CTask* task, CResource* res, std::chrono::steady_clock::time_point currentTime, int updated){

	unsigned int newProgress = 0;

	if (updated == 1 ) {

		// task contains updated progress value
		newProgress = task->mProgress;

	} else {

		std::chrono::steady_clock::duration currentTimeDur = currentTime.time_since_epoch();
		double currentTimeD =  currentTimeDur.count() / 1000000000.0;

		std::chrono::steady_clock::duration startedTimeDur = task->mTimes.Added.time_since_epoch();
		double startedTimeD = startedTimeDur.count() / 1000000000.0;

		double diff = currentTimeD - startedTimeD;
		double initTimeD = mpEst->taskTimeInit(task, res);
		newProgress = task->mProgress +
			mpEst->taskTimeComputeCheckpoint(task, res, entry->taskcopy->mProgress, diff - initTimeD);

	}

	if (entry->stopProgress < newProgress || entry->startProgress > newProgress) {
		return;
	}
	entry->startProgress = newProgress;

}
