// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#include <limits>
#include <list>
#include <algorithm>
#include <cmath>
#include "CScheduleAlgorithmGeneticMigDyn.h"
#include "CScheduleAlgorithmGeneticMigSolverLPDyn.h"
#include "CScheduleAlgorithmGeneticMigSolverLPEnergyDyn.h"
#include "CTask.h"
#include "CTaskCopy.h"
#include "CEstimation.h"
#include "CSchedule.h"
#include "CLogger.h"
#include "CConfig.h"
using namespace sched::algorithm;
using sched::task::CTask;
using sched::schedule::STaskEntry;
using sched::schedule::CSchedule;

CScheduleAlgorithmGeneticMigDyn::CGeneticSchedule::CGeneticSchedule(int max_num, int machines) {
	this->max_num = max_num;
	this->machines = machines;
	this->tasks = new struct SPart[max_num*machines](); // times two for two parts per task
	this->num = new int[machines]();
	this->fitness = 0;
	mfitness = 0;
	
	//CLogger::mainlog->debug("ScheduleAlgorithmGenetic: max_num %d machines %d %x", max_num, machines, this);
}

CScheduleAlgorithmGeneticMigDyn::SPart* CScheduleAlgorithmGeneticMigDyn::CGeneticSchedule::get(int mix, int rix) {

	return &(tasks[mix*max_num + rix]);

}

bool CScheduleAlgorithmGeneticMigDyn::CGeneticSchedule::find(int tid, int part, int* mixout, int* rixout) {

	// find position of second task in schedule
	int fmix = -1;
	int frix = -1;
	for (int mix=0; mix<machines; mix++) {
		for (int rix=0; rix<num[mix]; rix++) {
			if (get(mix, rix)->taskid == tid &&
				(part != -1 || get(mix, rix)->part == part)) {
				fmix = mix;
				frix = rix;
				break;
			}
		}
		if (fmix != -1) {
			break;
		}
	}
	if (fmix != -1) {
		*mixout = fmix;
		*rixout = frix;
		return true;
	}
	return false;
}

bool CScheduleAlgorithmGeneticMigDyn::CGeneticSchedule::check (){
	for (int tix=0; tix<max_num/2; tix++) {
		int f = 0;
		int part_a = 0;
		int part_b = 0;
		int part_a_mix = -1;
		int part_b_mix = -1;
		int part_a_slot = -1;
		int part_b_slot = -1;
		for (int mix=0; mix<machines; mix++) {
			for (int rix=0; rix<num[mix]; rix++) {
				if (get(mix, rix)->taskid == tix) {
					f++;
					if (get(mix, rix)->part == 0) {
						part_a++;
						part_a_mix = mix;
						part_a_slot = rix;
					}
					if (get(mix, rix)->part == 1) {
						part_b++;
						part_b_mix = mix;
						part_b_slot = rix;
					}
				}
			}
		}
		if (f <= 1 || f > 2) {
			CLogger::mainlog->debug("Genetic: schedule check failed: tid %d num %d != 2", tix, f);
			return false;
		}
		if (part_a != 1 || part_b != 1) {
			CLogger::mainlog->debug("Genetic: schedule check failed: tid %d a %d b %d", tix, part_a, part_b);
			return false;
		}
		if (part_a_mix == part_b_mix && part_b_slot < part_a_slot) {
			CLogger::mainlog->debug("Genetic: schedule check failed: tid %d direct machine order violation mixa %d mixb %d slota %d slotb %d", tix, part_a_mix, part_b_mix, part_a_slot, part_b_slot);
			return false;
		}
	}
	return true;
}

void CScheduleAlgorithmGeneticMigDyn::CGeneticSchedule::printTask(int mix, int rix){

	CLogger::mainlog->debug("Task: mix %d rix %d taskid %d part %d height %d ratio %lf",
		mix,
		rix,
		get(mix,rix)->taskid,
		get(mix,rix)->part,
		get(mix,rix)->height,
		get(mix,rix)->ratio
	);

}

void CScheduleAlgorithmGeneticMigDyn::CGeneticSchedule::print(){
	CLogger::mainlog->debug(">>> max_num %d machines %d fitness %lf mfitness %lf", max_num, machines, fitness, mfitness);
	for (int mix=0; mix<machines; mix++) {
		std::ostringstream ss;
		ss << mix << ":";
		for(int rix=0; rix<num[mix]; rix++) {
			ss << " " << get(mix, rix)->taskid << "." << 
			get(mix, rix)->height << "." <<
			(get(mix, rix)->part == 0 ? "A" :
			 (get(mix, rix)->part == 1 ? "B" : "?"));
			//<< "/" << get(mix, rix)->part_num;
		}
		CLogger::mainlog->debug(ss.str());
	}
	CLogger::mainlog->debug("<<<");
}

CScheduleAlgorithmGeneticMigDyn::CGeneticSchedule* CScheduleAlgorithmGeneticMigDyn::CGeneticSchedule::copy(){
	CGeneticSchedule* c = new CGeneticSchedule(max_num, machines);
	this->transfer(c);
	return c;
}

void CScheduleAlgorithmGeneticMigDyn::CGeneticSchedule::transfer(CGeneticSchedule* b){
	if (this == b) {
		CLogger::mainlog->debug("ScheduleAlgorithmGenetic: GeneticSchedule transfer on itself");
		return;
	}
	//CLogger::mainlog->debug("ScheduleAlgorithmGenetic: this %x b %x num ", this, b);
	
	for (int i=0; i<machines; i++) {
		b->num[i] = 
			this->num[i];
	}
	
	//memcpy(b->num, num, sizeof(int)*machines);

	CLogger::mainlog->debug("Genetic: transfer b max_num %d this max_num %d", b->max_num, this->max_num);
	
	for (int i=0; i<machines*max_num; i++) {
		//CLogger::mainlog->debug("ScheduleAlgorithmGenetic: ix %d ", i);
		b->tasks[i] = 
			this->tasks[i];
	}
	
	//memcpy(b->tasks, tasks, sizeof(int)*max_num*machines);
	b->fitness = this->fitness;
	b->mfitness = this->mfitness;
	b->timestamp = this->timestamp;
}

CScheduleAlgorithmGeneticMigDyn::CGeneticSchedule::~CGeneticSchedule(){
	//CLogger::mainlog->debug("ScheduleAlgorithmGenetic: ~ %x", this);
	delete[] tasks;
	delete[] num;
}


CScheduleAlgorithmGeneticMigDyn::CScheduleAlgorithmGeneticMigDyn(std::vector<CResource*>& rResources, enum EGeneticFitnessType fitness)
	: CScheduleAlgorithm(rResources)
{
	mpEstimation = CEstimation::getEstimation();

	mFitnessType = fitness;
}

CScheduleAlgorithmGeneticMigDyn::~CScheduleAlgorithmGeneticMigDyn(){
	delete mpEstimation;
	mpEstimation = 0;
}

int CScheduleAlgorithmGeneticMigDyn::init() {

	uint64_t seed = 0;

	CConfig* config = CConfig::getConfig();
	int ret = config->conf->getUint64((char*)"genetic_seed", &seed);
	if (-1 == ret) {
		CLogger::mainlog->error("ScheduleAlgorithmGenetic: genetic_seed not found in config");
		return -1;
	}
	CLogger::mainlog->info("ScheduleAlgorithmGenetic: genetic_seed: %ld", seed);
	CLogger::eventlog->info("\"event\":\"ALGORITHM_PARAM\",\"name\":\"genetic_seed\",\"value\":%ld", seed);

	mRandom = std::mt19937(seed);

	std::string* solver = 0;
	ret = config->conf->getString((char*)"geneticmig_solver", &solver);
	if (-1 == ret) {
		CLogger::mainlog->error("ScheduleAlgorithmGenetic: geneticmig_solver not found in config");
		return -1;
	}
	CLogger::mainlog->info("ScheduleAlgorithmGenetic: solver: %s", solver);

	mpSolverPath = (char*) solver->c_str();
	CLogger::eventlog->info("\"event\":\"ALGORITHM_PARAM\",\"name\":\"geneticmig_solver\",\"value\":\"%s\"", mpSolverPath);

	return 0;
}

void CScheduleAlgorithmGeneticMigDyn::fini() {
}

CScheduleAlgorithmGeneticMigDyn::CGeneticSchedule* CScheduleAlgorithmGeneticMigDyn::generateSchedule(
	
	//std::list<CTaskCopy*>* height_sets, 

	std::vector<int>* height_sets,
	int height_num, int machines,
	int max_num, std::vector<CTaskCopy>* pTasks
){
	
	CGeneticSchedule* sched = new CGeneticSchedule(max_num, machines);

	// for all heights
	// for each task in height
	// assign randomly a machine

	for (int hix=0; hix<height_num; hix++) {

		int hix_num =  height_sets[hix].size();

		for (int rix=0; rix<hix_num; rix++) {
			
			int tix = height_sets[hix][rix];

			CTaskCopy* task = &((*pTasks)[tix]);

			// number of task machines
			int t_mix_num = task->mpResources->size();

			// pick machine for task tix
			int t_mix = (int)(((double)mRandom() / (double)mRandom.max())*(t_mix_num));
			if (t_mix == t_mix_num) {
				t_mix--;
			}

			// find resource index
			CResource* res = (*(task->mpResources))[t_mix];
			int mix = std::distance(mrResources.begin(),
				std::find(mrResources.begin(),
					mrResources.end(), res));

			// add parts to schedule
			// part a
			sched->get(mix, sched->num[mix])->taskid = tix;
			sched->get(mix, sched->num[mix])->part = 0;
			sched->get(mix, sched->num[mix])->part_num = 2;
			sched->get(mix, sched->num[mix])->height = hix;
			sched->get(mix, sched->num[mix])->subheight = hix*2;
			// part b
			sched->get(mix, sched->num[mix]+1)->taskid = tix;
			sched->get(mix, sched->num[mix]+1)->part = 1;
			sched->get(mix, sched->num[mix]+1)->part_num = 2;
			sched->get(mix, sched->num[mix]+1)->height = hix;
			sched->get(mix, sched->num[mix]+1)->subheight = hix*2 + 1;

			sched->get(mix, sched->num[mix])->taskid = tix;
			sched->num[mix]+=2;
		}
	}

	for (int mix=0; mix<machines; mix++) {
		for (int rix=0; rix<sched->num[mix]; rix++) {

			CLogger::mainlog->debug("generateSchedule: %d", 
				sched->get(mix, rix)->taskid);
		}
	}

	if (sched->check() == false) {
		
		CLogger::mainlog->debug("ScheduleAlgorithmGenetic: fuck");
	}
	return sched;
}

bool CScheduleAlgorithmGeneticMigDyn::mutate(CGeneticSchedule* sched, int* height, int machines, int height_num, std::vector<int>* height_sets, int max_num) {

	// schedule needs to utilize at least one machine
	int usedMachines = 0;
	for (int mix=0; mix<machines; mix++) {
		if (sched->num[mix] > 0) {
			usedMachines++;
		}
	}
	if (usedMachines == 0) {
		return false;
	}

	// pick first random task
	int mix1 = 0;
	int mix1_num = 0;
	do {
		mix1 = (((double)mRandom() / (double)mRandom.max())*(machines));
		if (mix1 == machines) {
			mix1--;
		}
		mix1_num = sched->num[mix1];
	} while (mix1_num == 0);

	// pick position in machine queue
	int rix1 = (((double)mRandom() / (double)mRandom.max())*(mix1_num));
	if (rix1 == mix1_num) {
		rix1--;
	}

	// get task index
	//CLogger::mainlog->debug("ScheduleAlgorithmGenetic: rix %d mix_num %d mix %d", rix, mix_num, mix);
	int tix1 = sched->get(mix1, rix1)->taskid;
	//CLogger::mainlog->debug("ScheduleAlgorithmGenetic: tix %d rix %d mix_num %d mix %d", tix, rix, mix_num, mix);
	int part1 = sched->get(mix1, rix1)->part;
	//int tix_height = height[tix];
	int tix1_height = sched->get(mix1, rix1)->height;

	// pick second random task with same height
	int height_set_num = height_sets[tix1_height].size();
	int hix = (((double)mRandom() / (double)mRandom.max())*(height_set_num*2));
	if (hix == height_set_num*2) {
		hix--;
	}
	// height was doubled for parts
	// even height index is part 0
	// odd height index is part 1
	int part2 = hix%2;
	// get real height index
	hix = hix / 2;
	int tix2 = height_sets[tix1_height][hix];

	// second task == first task
	if (tix2 == tix1 && part1 == part2) {
		return false;
	}

	// find position of second task in schedule
	int mix2 = -1;
	int rix2 = -1;
	bool found = sched->find(tix2, part2, &mix2, &rix2);

	if (found == false) {
		//CLogger::mainlog->debug("ScheduleAlgorithmGenetic: tix %d not found, sched %x, tix_height %d, hix %d, machines %d", tix2, sched, tix_height, hix, machines);
		if (sched->check() == false) {
		}
		CLogger::mainlog->debug("ScheduleAlgorithmGenetic: not found fuck");
	}


	// find position of other part for first part
	int mix1_b = -1;
	int rix1_b = -1;
	sched->find(tix1, part1 == 0 ? 1: 0, &mix1_b, &rix1_b);

	// find position of other part for second part
	int mix2_b = -1;
	int rix2_b = -1;
	sched->find(tix2, part2 == 0 ? 1: 0, &mix2_b, &rix2_b);

	// check legality of mutate operation
	// if part one is moved to part two's position
	// will this be the same machine as part one's second part
	if (mix2 == mix1_b) {
		// will the new position of part one violate the part order?
		if (part1 == 0 && rix2 > rix1_b) {
			return false;
		} else
		if (part1 == 1 && rix2 < rix1_b) {
			return false;
		}
	}
	if (mix1 == mix2_b) {
		if (part2 == 0 && rix1 > rix2_b) {
			return false;
		} else
		if (part2 == 1 && rix1 < rix2_b) {
			return false;
		}
	}



	// swap
	
	//CLogger::mainlog->debug("ScheduleAlgorithmGenetic: max_num %d mix %d rix %d mix2 %d rix2 %d ", max_num, mix, rix, mix2, rix2);
	if (max_num*mix1 + rix1 < 0 || max_num*mix1 + rix1 > max_num*machines) {
		CLogger::mainlog->debug("ScheduleAlgorithmGenetic: oops");
	}
	if (max_num*mix2 + rix2 < 0 || max_num*mix2 + rix2 > max_num*machines) {
		CLogger::mainlog->debug("ScheduleAlgorithmGenetic: oops");
	}
	SPart p1 = *(sched->get(mix1, rix1));
	SPart p2 = *(sched->get(mix2, rix2));
	*(sched->get(mix1, rix1)) = p2;
	*(sched->get(mix2, rix2)) = p1;

//	int subheight1 = sched->get(mix1, rix1)->subheight;
//	int subheight2 = sched->get(mix2, rix2)->subheight;
//	sched->get(mix1, rix1)->taskid = tix2;
//	sched->get(mix1, rix1)->part = part2;
//	sched->get(mix1, rix1)->subheight = subheight2;
//	sched->get(mix2, rix2)->taskid = tix1;
//	sched->get(mix2, rix2)->part = part1;
//	sched->get(mix2, rix2)->subheight = subheight1;
	return true;
}


void CScheduleAlgorithmGeneticMigDyn::crossover(CGeneticSchedule* a, CGeneticSchedule* b, int height_num, int machines, int* height, int max_num){


	CLogger::mainlog->debug("Crossover before");
	a->print();
	b->print();

	int c = (((double)mRandom() / (double)mRandom.max())*(height_num));
	if (c == height_num) {
		c--;
	}

	CLogger::mainlog->debug("Crossover height %d", c);

	// for all machines
	for (int mix=0; mix<machines; mix++) {
		int a_c_rix = -1;
		int b_c_rix = -1;
		// find first node for machine mix with height > c in sched a
		for (int rix=0; rix<a->num[mix]; rix++) {
			if (a->get(mix, rix)->height > c) {
				a_c_rix = rix;
				break;
			}
		}
		if (a_c_rix == -1) {
			a_c_rix = a->num[mix];
		}
		// find first node for machine mix with height > c in sched b
		for (int rix=0; rix<b->num[mix]; rix++) {
			if (b->get(mix, rix)->height > c) {
				b_c_rix = rix;
				break;
			}
		}

		if (b_c_rix == -1) {
			b_c_rix = b->num[mix];
		}
		// swap tasks for this machines
		int a_max = a->num[mix];
		int b_max = b->num[mix];
		int max = b_max > a_max ? b_max : a_max;
		for (int rix=0; rix<max; rix++) {
			SPart a_tix = {};
			if (a_c_rix + rix < a_max) {
				a_tix = *(a->get(mix, a_c_rix + rix));
			}
			SPart b_tix = {};
			if (b_c_rix + rix < b_max) {
				b_tix = *(b->get(mix, b_c_rix + rix));
			}
			if (b_c_rix + rix < b_max) {
				*(a->get(mix, a_c_rix + rix)) = b_tix;
			}
			if (a_c_rix + rix < a_max) {
				*(b->get(mix, b_c_rix + rix)) = a_tix;
			}
		}
		a->num[mix] = a_c_rix + (b_max-b_c_rix);
		b->num[mix] = b_c_rix + (a_max-a_c_rix);
		CLogger::mainlog->debug("Crossover: a_c_rix %d b_c_rix %d", a_c_rix, b_c_rix);
	}

	CLogger::mainlog->debug("Crossover after");
	a->print();
	b->print();
}

void CScheduleAlgorithmGeneticMigDyn::reproduction(std::vector<CGeneticSchedule*>* pop, std::vector<CGeneticSchedule*>* newpop){

	// expect that newpop already is a complete population
	// just transfer data

	CLogger::mainlog->debug("ScheduleAlgorithmGenetic: Reproduction");
	int pop_size = pop->size();
	double roulette_sum = 0;
	int max_fitness_pix = -1;
	double max_fitness = -1;
	for (unsigned int pix=0; pix<pop->size(); pix++) {
		roulette_sum += (*pop)[pix]->mfitness;
		if (pix == 0 || (*pop)[pix]->fitness > max_fitness) {
			max_fitness = (*pop)[pix]->mfitness;
			max_fitness_pix = pix;
		}
	}
	CLogger::mainlog->debug("Reproduction: roulette_sum %lf", roulette_sum);
	for (int npix=0; npix<pop_size-1; npix++) {
		double pick = (((double)mRandom() / (double)mRandom.max())*(roulette_sum));
		if (pick == roulette_sum) {
			pick--;
		}
		// find picked node
		int pick_pix = -1;
		for (unsigned int pix=0; pix<pop->size(); pix++) {
			double fitness = (*pop)[pix]->mfitness;
			pick -= fitness;
			if (pick <= 0) {
				pick_pix = pix;
				break;
			}
		}
		// add picked node to new population
		pop->at(pick_pix)->transfer(newpop->at(npix));
	}
	// copy node with maximal fitness
	//CLogger::mainlog->debug("ScheduleAlgorithmGenetic: max_fitness_pix %d", max_fitness_pix);
	pop->at(max_fitness_pix)->transfer(newpop->back());
}

void CScheduleAlgorithmGeneticMigDyn::fitness(CGeneticSchedule* sched, int tasks, std::vector<int>* height_sets, int* height, int machines, int height_num, std::vector<CTaskCopy>* pTasks, int max_num, std::unordered_map<int,int>* taskmap,double double_max, std::vector<CTaskCopy*>* runningTasks){

	switch(mFitnessType) {
		case GENETIC_FITNESS_ENERGY:
			
			fitnessEnergy(sched,
				tasks,
				height_sets,
				height,
				machines,
				height_num,
				pTasks,
				max_num,
				taskmap,
				double_max,
				runningTasks);

		break;

		default:
		case GENETIC_FITNESS_MAKESPAN:

			fitnessMakespan(sched,
				tasks,
				height_sets,
				height,
				machines,
				height_num,
				pTasks,
				max_num,
				taskmap,
				double_max,
				runningTasks);

		break;
	}

}

void CScheduleAlgorithmGeneticMigDyn::fitnessMakespan(CGeneticSchedule* sched, int tasks, std::vector<int>* height_sets, int* height, int machines, int height_num, std::vector<CTaskCopy>* pTasks, int max_num, std::unordered_map<int,int>* taskmap, double double_max, std::vector<CTaskCopy*>* runningTasks){


	// check schedule for validity
	for (int mix=0; mix<machines; mix++) {
		CResource* res = mrResources[mix];
		for (int rix=0; rix<sched->num[mix]; rix++) {

			int tix = sched->get(mix, rix)->taskid;

			CTaskCopy* task = &((*pTasks)[tix]);

			if (task->validResource(res) == false) {
				sched->fitness = double_max;
				return;
			}

		}
	}

	CScheduleAlgorithmGeneticMigSolverLPDyn::solve(sched, mrResources, mpEstimation, pTasks, taskmap, double_max, runningTasks, mpSolverPath);

/*
	// finishing time
	double* finish = new double[tasks]();
	int* progress = new int[machines]();
	double max_finish = 0.0;
	for (int hix=0; hix<height_num; hix++) {

		for (int mix=0; mix<machines; mix++) {
			for (int rix=progress[mix]; rix<sched->num[mix]; rix++) {

				if (rix == sched->num[mix]) {
					continue;
				}

				int tix = sched->get(mix, rix)->taskid;
				if (sched->get(mix, rix)->height > hix) {
					break;
				}

				double preFinish = 0.0;

				// check finish time of previous task on same time
				if (rix > 0) {
					preFinish = finish[sched->get(mix, rix)->taskid];
				}

				// compute duration
				CTaskCopy* task = &((*pTasks)[tix]);
				CResource* res = mrResources[mix];
				double dur = 
					mpEstimation->taskTimeInit(task, res) +
					mpEstimation->taskTimeCompute(task, res, task->mProgress, task->mCheckpoints) +
					mpEstimation->taskTimeFini(task, res);

				// task not valid for this resource
				if (dur == 0) {
					//dur = std::numeric_limits<double>::max();
					//dur = 1000000000000.0;
					dur = double_max;
				}

				// find max finish time of preceeding tasks
				// preceeding tasks need to be in previous height_sets
				for (int six=0; six<task->mPredecessorNum; six++) {
					int sid = task->mpPredecessorList[six];
					int stask_ix = -1;
					// stask_ix should always be in the map
					stask_ix = (*taskmap)[sid];
					if (finish[stask_ix] > preFinish) {
						preFinish = finish[stask_ix];
					}
				}

				// set finish time
				finish[tix] = preFinish + dur;

				if (finish[tix] > max_finish) {
					max_finish = finish[tix];
				}

				progress[mix]++;
			}
		}
	}

	delete[] finish;
	delete[] progress;
*/
//	sched->fitness = max_finish;
}

void CScheduleAlgorithmGeneticMigDyn::fitnessEnergy(CGeneticSchedule* sched, int tasks, std::vector<int>* height_sets, int* height, int machines, int height_num, std::vector<CTaskCopy>* pTasks, int max_num, std::unordered_map<int,int>* taskmap, double double_max, std::vector<CTaskCopy*>* runningTasks){

	// check schedule for validity
	for (int mix=0; mix<machines; mix++) {
		CResource* res = mrResources[mix];
		for (int rix=0; rix<sched->num[mix]; rix++) {

			int tix = sched->get(mix, rix)->taskid;

			CTaskCopy* task = &((*pTasks)[tix]);

			if (task->validResource(res) == false) {
				sched->fitness = double_max;
				return;
			}

		}
	}

	CScheduleAlgorithmGeneticMigSolverLPEnergyDyn::solve(sched, mrResources, mpEstimation, pTasks, taskmap, double_max, runningTasks, mpSolverPath);

}


CSchedule* CScheduleAlgorithmGeneticMigDyn::compute(std::vector<CTaskCopy>* pTasks, std::vector<CTaskCopy*>* runningTasks, volatile int* interrupt, int updated) {

	int tasks = pTasks->size();
	int machines = mrResources.size();

	CSchedule* sched = new CSchedule(tasks, machines, mrResources);
	sched->mpOTasks = pTasks;

	if (tasks < 2) {
		CTaskCopy* task = &((*pTasks)[0]);
		double selectedComplete = 0.0;
		int selectedMix = -1;
		for (int mix = 0; mix<machines; mix++) {
			CResource* res = mrResources[mix];

			// completion time =
			// init time +
			// compute time +
			// fini time
			double complete = 
				mpEstimation->taskTimeInit(task, res) +
				mpEstimation->taskTimeCompute(task, res, task->mProgress, task->mCheckpoints) + 
				mpEstimation->taskTimeFini(task, res);
			if (mix == 0 || complete < selectedComplete) {
				selectedMix = mix;
				selectedComplete = complete;
			}
		}

		STaskEntry* entry = new STaskEntry();
		entry->taskcopy = task;
		entry->taskid = entry->taskcopy->mId;
		entry->stopProgress = entry->taskcopy->mCheckpoints;
		(*(sched->mpTasks))[selectedMix]->push_back(entry);
		return sched;
	}



	// generate map for global id to local id conversion
	std::unordered_map<int,int> taskmap;
	for (unsigned int i=0; i<pTasks->size(); i++) {
		taskmap[(*pTasks)[i].mId] = i;
	}

	// prepare tasks
	// compute height for every task
	int* height = new int[tasks]();
	for (int ix=0; ix<tasks; ix++) {
		height[ix] = -1;
	}
	int max_height = -1;
	// for each task traverse successor tasks to set height
	int* stack = new int[tasks*2](); // taskid, height
	int stackIx = 0; // next element
	for (int ix=0; ix<tasks; ix++) {
		if (height[ix] != -1) {
			// already set
			continue;
		}
		// set current task height to zero
		height[ix] = 0;
		// put all successor tasks on stack
		for (int six=0; six<(*pTasks)[ix].mSuccessorNum; six++) {
			stack[stackIx] = (*pTasks)[ix].mpSuccessorList[six];
			stack[stackIx+1] = 1;
			stackIx += 2;
		}
		// traverse graph using stack
		while (stackIx > 0) {
			// pop top task from stack
			int taskid = stack[stackIx-2];
			int taskheight = stack[stackIx-1];
			stackIx -= 2;
			// search for TaskCopy in task list
			CTaskCopy* task = 0;
			int taskix = -1;
			std::unordered_map<int,int>::const_iterator ix_it = taskmap.find(taskid);
			if (taskmap.end() != ix_it) {
				taskix = ix_it->second;
			}
/* Replaced by map lookup
			for (int tix=0; tix<tasks; tix++) {
				if ((*pTasks)[tix].mId == taskid) {
					task = &((*pTasks)[tix]);
					taskix = tix;
					break;
				}
			}
*/
			if (taskix == -1) {
				// task not found
				continue;
			}
			task = &((*pTasks)[taskix]);
			if (height[taskix] >= taskheight) {
				// already been here
				continue;
			}
			// set task height
			height[taskix] = taskheight;
			if (taskheight > max_height) {
				max_height = taskheight;
			}
			// add successors on stack
			for (int six=0; six<task->mSuccessorNum; six++) {
				stack[stackIx] = task->mpSuccessorList[six];
				stack[stackIx+1] = taskheight+1;
				stackIx += 2;
			}
		}
	}
	delete[] stack;

	// all tasks have height 0 ?
	if (tasks > 0 && max_height == -1) {
		max_height = 0;
	}

	int height_num = max_height + 1;
	int max_num = tasks;

	// create height sets
	std::vector<int>* height_sets = new std::vector<int>[height_num]();
	for (int tix=0; tix<tasks; tix++) {
		int th = height[tix];
		
		//CLogger::mainlog->debug("ScheduleAlgorithmGenetic: height_num %d th %d", height_num, th);
		height_sets[th].push_back(tix);
	}
	


	// create initial population
	int pop_num = 20;
	std::vector<CGeneticSchedule*>* pop = new std::vector<CGeneticSchedule*>();
	for (int popix=0; popix<pop_num; popix++) {
		// twice the tasks for number of parts
		pop->push_back(generateSchedule(height_sets, height_num, machines, max_num*2, pTasks));
	}

	// double height_num for parts

	//double double_max = std::numeric_limits<double>::max();
	double double_max = 10000000000.0;

	// compute fitness of initial population
	double max_fitness = 0.0;
	int best_ix = -1;
	double best_fitness = 0.0;
	for (int pix=0; pix<pop_num; pix++) {
		fitness((*pop)[pix], tasks, height_sets, height, machines, height_num, pTasks, max_num, &taskmap, double_max, runningTasks);

		if ((*pop)[pix]->fitness > max_fitness) {
			max_fitness = (*pop)[pix]->fitness;
		}
		if (best_ix == -1 || (*pop)[pix]->fitness < best_fitness) {
			best_ix = pix;
			best_fitness = (*pop)[pix]->fitness;
		}
	}
	for (int pix=0; pix<pop_num; pix++) {
		(*pop)[pix]->mfitness = max_fitness - (*pop)[pix]->fitness;
	}


	std::vector<CGeneticSchedule*>* newpop = new std::vector<CGeneticSchedule*>();
	std::vector<CGeneticSchedule*>* tmp = new std::vector<CGeneticSchedule*>();

	for(int pix=0; pix < pop_num; pix++) {
		newpop->push_back(new CGeneticSchedule(max_num*2, machines));
		tmp->push_back(new CGeneticSchedule(max_num*2, machines));
	}

	CLogger::mainlog->debug("ScheduleAlgorithmGenetic: best fitness initial: %f", best_fitness);

	int nochange = 0;
	int max_nochange = 10;
	double crossover_prob = 0.6;
	double mutation_prob = 0.6;
	int count = 0;
	bool interrupted = false;
	do {

		// reproduction
		for (int ix=0; ix<pop_num; ix++) {
			if(pop->at(ix)->check() == false) {
				CLogger::mainlog->debug("ScheduleAlgorithmGenetic: broken before reproduction");
			}
		}
		reproduction(pop, newpop);
		for (int ix=0; ix<pop_num; ix++) {
			if(pop->at(ix)->check() == false) {
				CLogger::mainlog->debug("ScheduleAlgorithmGenetic: broken after reproduction");
			}
		}
		//CLogger::mainlog->debug("ScheduleAlgorithmGenetic: pop_num %d",pop_num);
		// crossover
		CLogger::mainlog->debug("ScheduleAlgorithmGenetic: Crossover");
		for (int cix=0; cix<pop_num/2; cix++) {
			double guess = (double)mRandom() / (double)mRandom.max();
			(*newpop)[cix]->transfer((*tmp)[cix*2]);
		
			//CLogger::mainlog->debug("ScheduleAlgorithmGenetic: newpop %d tmp %d", pop_num/2 + cix, cix*2 + 1);
			(*newpop)[pop_num/2 + cix]->transfer((*tmp)[cix*2 + 1]);
			if (guess < crossover_prob) {
				// apply crossover
				if ((*tmp)[cix*2]->check() == false || (*tmp)[cix*2 + 1]->check() == false ) {
					CLogger::mainlog->debug("ScheduleAlgorithmGenetic: broken before crossover");
				}
					
				//CLogger::mainlog->debug("ScheduleAlgorithmGenetic: apply crossover");
				crossover((*tmp)[cix*2], (*tmp)[cix*2 + 1], height_num, machines, height, max_num);
				if ((*tmp)[cix*2]->check() == false || (*tmp)[cix*2 + 1]->check() == false ) {
					CLogger::mainlog->debug("ScheduleAlgorithmGenetic: broken after crossover");
					
				}
			}
		}

		// mutation
		CLogger::mainlog->debug("ScheduleAlgorithmGenetic: Mutation");
		for (int pix=0; pix<pop_num; pix++) {
			double guess = (double)mRandom() / (double)mRandom.max();
			if (guess < mutation_prob) {
				// apply mutation
				if ((*tmp)[pix]->check() == false) {
					CLogger::mainlog->debug("ScheduleAlgorithmGenetic: broken before mutation");
				}
				int tries = 0;
				bool mok = false;
				do {
					mok = mutate((*tmp)[pix], height, machines, height_num, height_sets, max_num*2);
					tries++;
				} while (mok == false && tries < 5);
				if ((*tmp)[pix]->check() == false) {
					CLogger::mainlog->debug("ScheduleAlgorithmGenetic: broken after mutation");
				}
				if (mok == false) {
					CLogger::mainlog->debug("ScheduleAlgorithmGenetic: Mutation failed");
					(*tmp)[pix]->check();
					CLogger::mainlog->debug("ScheduleAlgorithmGenetic: ---");
				}
			}
		}



		// recompute fitness for new population
		int worst_pix = -1;
		double worst_fitness = 0.0;
		int new_best_ix = -1;
		double new_best_fitness = 0.0;
		for (int pix=0; pix<pop_num; pix++) {
			CLogger::mainlog->debug("ScheduleAlgorithmGenetic: fitness call");
			fitness((*tmp)[pix], tasks, height_sets, height, machines, height_num, pTasks, max_num, &taskmap, double_max, runningTasks);
			if ((*tmp)[pix]->fitness > max_fitness) {
				max_fitness = (*tmp)[pix]->fitness;
			}
			if (worst_pix == -1 || (*tmp)[pix]->fitness > worst_fitness) {
				worst_pix = pix;
				worst_fitness = (*tmp)[pix]->fitness;
			}
			if (new_best_ix == -1 || (*tmp)[pix]->fitness < new_best_fitness) {
				new_best_ix = pix;
				new_best_fitness = (*tmp)[pix]->fitness;
			}
			
			CLogger::mainlog->debug("ScheduleAlgorithmGenetic: ix %d fitness %f", pix,  (*tmp)[pix]->fitness);
		}

		for (int pix=0; pix<pop_num; pix++) {
			(*pop)[pix]->mfitness = max_fitness - (*pop)[pix]->fitness;
		}

		// replace worst node with best node
		(*pop)[best_ix]->transfer((*tmp)[worst_pix]);
		best_ix = worst_pix;
		if (new_best_fitness < best_fitness) {
			best_ix = new_best_ix;
			best_fitness = new_best_fitness;
			nochange = 0;
		} else {
			nochange++;
		}
		std::vector<CGeneticSchedule*>* swap = pop;
		pop = tmp;
		tmp = swap;
		count++;
		CLogger::mainlog->debug("ScheduleAlgorithmGenetic: best fitness round %d: %f", count, best_fitness);

		if (*interrupt == 1) {
			interrupted = true;
		}

	} while ( nochange < max_nochange && interrupted == false);

	if (interrupted == false) {
		CGeneticSchedule* bestsched = (*pop)[best_ix];

		CLogger::mainlog->debug("ScheduleAlgorithmGenetic: final schedule fitness %f timestamp %ld", bestsched->fitness, bestsched->timestamp);
		
		if (bestsched->check() == true) {
			CLogger::mainlog->debug("ScheduleAlgorithmGenetic: schedule seems ok");
		} else {
			CLogger::mainlog->debug("ScheduleAlgorithmGenetic: schedule seems broken");
		}
		

		// add task to machine queue
		for (int mix=0; mix<machines; mix++) {
			for (int rix=0; rix<bestsched->num[mix]; rix++) {
				int tix = bestsched->get(mix, rix)->taskid;
				int part = bestsched->get(mix, rix)->part;
				double ratio = bestsched->get(mix, rix)->ratio;
				bestsched->printTask(mix, rix);
				if (ratio == 0.0) {
					CLogger::mainlog->debug("ScheduleAlgorithmGeneticMig: ignore part mix %d rix %d because ratio == 0.0", mix, rix);
					continue;
				}

				CTaskCopy* task =  &((*pTasks)[tix]);
				STaskEntry* entry = new STaskEntry();
				entry->taskcopy = task;
				entry->taskid = entry->taskcopy->mId;
				
				int rest = task->mCheckpoints - task->mProgress;
				int points = round(rest*ratio);
				if (points == 0) {
					CLogger::mainlog->debug("ScheduleAlgorithmGeneticMig: ignore part mix %d rix %d because points == 0, ratio == %lf", mix, rix, ratio);
					continue;
				}
				if (part == 0) {
					entry->startProgress = task->mProgress;
					entry->stopProgress = task->mProgress + points;
				} else {
					entry->startProgress = task->mCheckpoints - points;
					entry->stopProgress = task->mCheckpoints;
				}
				CLogger::mainlog->debug("ScheduleAlgorithmGeneticMig: ADD part to mix %d rix %d taskid %d part %d startP %d stopP %d", mix, rix, entry->taskid, part, entry->startProgress, entry->stopProgress);
				(*(sched->mpTasks))[mix]->push_back(entry);
			}
		}
		bestsched->print();
		sched->computeTimes();
	} else {
		delete sched;
		sched = 0;
	}

	// clean up
	delete[] height_sets;
	delete[] height;
	for (int pix=0; pix<pop_num; pix++) {
		delete pop->back();
		delete newpop->back();
		delete tmp->back();
		pop->pop_back();
		newpop->pop_back();
		tmp->pop_back();
	}
	delete pop;
	delete newpop;
	delete tmp;

	return sched;
}
