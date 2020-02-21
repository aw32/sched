// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#include <limits>
#include <list>
#include <algorithm>
#include "CScheduleAlgorithmSimulatedAnnealing.h"
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



CScheduleAlgorithmSimulatedAnnealing::CScheduleAlgorithmSimulatedAnnealing(std::vector<CResource*>& rResources, enum EGeneticFitnessType fitness)
	: CScheduleAlgorithmGenetic(rResources, fitness)
{
	//mpEstimation = CEstimation::getEstimation();

	//mFitnessType = fitness;
}

CScheduleAlgorithmSimulatedAnnealing::~CScheduleAlgorithmSimulatedAnnealing(){
}

int CScheduleAlgorithmSimulatedAnnealing::init() {

	uint64_t seed = 0;

	CConfig* config = CConfig::getConfig();
	int ret = config->conf->getUint64((char*)"genetic_seed", &seed);
	if (-1 == ret) {
		CLogger::mainlog->error("ScheduleAlgorithmSimulatedAnnealing: genetic_seed not found in config");
		return -1;
	}
	CLogger::mainlog->info("ScheduleAlgorithmSimulatedAnnealing: genetic_seed: %ld", seed);
	CLogger::eventlog->info("\"event\":\"ALGORITHM_PARAM\",\"name\":\"genetic_seed\",\"value\":%ld", seed);

	mRandom = std::mt19937(seed);


	ret = config->conf->getDouble((char*)"simann_init_prob", &mInitialProb);
	if (-1 == ret) {
		CLogger::mainlog->error("ScheduleAlgorithmSimulatedAnnealing: simann_init_prob not found in config");
		return -1;
	}
	CLogger::mainlog->info("ScheduleAlgorithmSimulatedAnnealing: simann_init_prob: %lf", mInitialProb);
	CLogger::eventlog->info("\"event\":\"ALGORITHM_PARAM\",\"name\":\"simann_init_prob\",\"value\":%lf", mInitialProb);


	ret = config->conf->getDouble((char*)"simann_loops_factor", &mLoopsFactor);
	if (-1 == ret) {
		CLogger::mainlog->error("ScheduleAlgorithmSimulatedAnnealing: simann_loops_factor not found in config");
		return -1;
	}
	CLogger::mainlog->info("ScheduleAlgorithmSimulatedAnnealing: simann_loops_factor: %lf", mLoopsFactor);
	CLogger::eventlog->info("\"event\":\"ALGORITHM_PARAM\",\"name\":\"simann_loops_factor\",\"value\":%lf", mLoopsFactor);


	ret = config->conf->getDouble((char*)"simann_reduce", &mReduce);
	if (-1 == ret) {
		CLogger::mainlog->error("ScheduleAlgorithmSimulatedAnnealing: simann_reduce not found in config");
		return -1;
	}
	CLogger::mainlog->info("ScheduleAlgorithmSimulatedAnnealing: simann_reduce: %lf", mReduce);
	CLogger::eventlog->info("\"event\":\"ALGORITHM_PARAM\",\"name\":\"simann_reduce\",\"value\":%lf", mReduce);

	ret = config->conf->getDouble((char*)"simann_min_prob", &mMinProb);
	if (-1 == ret) {
		CLogger::mainlog->error("ScheduleAlgorithmSimulatedAnnealing: simann_min_prob not found in config");
		return -1;
	}
	CLogger::mainlog->info("ScheduleAlgorithmSimulatedAnnealing: simann_min_prob: %lf", mMinProb);
	CLogger::eventlog->info("\"event\":\"ALGORITHM_PARAM\",\"name\":\"simann_min_prob\",\"value\":%lf", mMinProb);


	return 0;
}

void CScheduleAlgorithmSimulatedAnnealing::fini() {
	CScheduleAlgorithmGenetic::fini();
}

void CScheduleAlgorithmSimulatedAnnealing::randomize(CGeneticSchedule* schedule, int* height, int machines, int height_num, std::vector<int>* height_sets, int max_num){

	// single mutate
	mutate(schedule, height, machines, height_num, height_sets, max_num);

}

CSchedule* CScheduleAlgorithmSimulatedAnnealing::compute(std::vector<CTaskCopy>* pTasks, std::vector<CTaskCopy*>* runningTasks, volatile int* interrupt, int updated) {

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
	

/*
	// create initial population
	int pop_num = 20;
	std::vector<CGeneticSchedule*>* pop = new std::vector<CGeneticSchedule*>();
	for (int popix=0; popix<pop_num; popix++) {
		pop->push_back(generateSchedule(height_sets, height_num, machines, max_num));
	}

	
	// compute fitness of initial population
	double max_fitness = 0.0;
	int best_ix = -1;
	double best_fitness = 0.0;
	for (int pix=0; pix<pop_num; pix++) {
		fitness((*pop)[pix], tasks, height_sets, height, machines, height_num, pTasks, max_num, &taskmap);

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
		newpop->push_back(new CGeneticSchedule(max_num, machines));
		tmp->push_back(new CGeneticSchedule(max_num, machines));
	}

	CLogger::mainlog->debug("ScheduleAlgorithmGenetic: best fitness initial: %f", best_fitness);

	double min_fitness = std::numeric_limits<double>::max();
	int nochange = 0;
	int max_nochange = 10;
	double crossover_prob = 0.6;
	double mutation_prob = 0.6;
	int count = 0;
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
			int tmpsize = tmp->size();
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
				mutate((*tmp)[pix], height, machines, height_num, height_sets, max_num);
			}
		}



		// recompute fitness for new population
		int worst_pix = -1;
		double worst_fitness = 0.0;
		int new_best_ix = -1;
		double new_best_fitness = 0.0;
		for (int pix=0; pix<pop_num; pix++) {
			fitness((*tmp)[pix], tasks, height_sets, height, machines, height_num, pTasks, max_num, &taskmap);
			if ((*tmp)[pix]->fitness > max_fitness) {
				max_fitness = (*tmp)[pix]->fitness;
			}
			if (worst_pix == -1 || (*tmp)[pix]->fitness > max_fitness) {
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

	} while ( nochange < max_nochange );
*/


	// simulated annealing

	int loops_num = tasks * mLoopsFactor;

	// compute initial solution

	CGeneticSchedule* initialSolution = generateSchedule(height_sets, height_num, machines, max_num, pTasks);


	double double_max = 1000000000.0; // punishment value for invalid schedules for the initial run

	CGeneticSchedule* solution = new CGeneticSchedule(max_num, machines);
	initialSolution->transfer(solution);
	
	fitness(solution, tasks, height_sets, height, machines, height_num, pTasks, max_num, &taskmap, double_max, runningTasks);

	CGeneticSchedule* bestSolution = new CGeneticSchedule(max_num, machines);
	solution->transfer(bestSolution);

	CGeneticSchedule* neighbor = new CGeneticSchedule(max_num, machines);


	// initial run to compute temperature for initial probability
	int initialRun_num = 2*loops_num;
	double initialDiff[initialRun_num]; // occured
	for (int i=0; i<initialRun_num; i++) {
		initialDiff[i] = 0.0;
	}
	//double avgFitness = 0.0;
	int posDiffIx = 0;

	for (int loopIx = 0; loopIx < initialRun_num; loopIx++) {

		// compute new solution
		// compute new neighbor
		solution->transfer(neighbor);
		for (int i=0; i<1; i++) {
			randomize(neighbor, height, machines, height_num, height_sets, max_num);
		}

		// cost difference = cost(solution) - cost(neighbor)
		fitness(neighbor, tasks, height_sets, height, machines, height_num, pTasks, max_num, &taskmap, double_max, runningTasks);
		double diff = solution->fitness - neighbor->fitness;

		// save cost differences for worse neighbors
		//if (diff < 0.0) {
		initialDiff[posDiffIx] = abs(diff);
		posDiffIx++;
		//}
		//avgFitness += cost_neighbor / loops_num;

		CLogger::mainlog->debug("ScheduleAlgorithmSimulatedAnnealing: loopIx %d fitness %lf diff %lf", loopIx, neighbor->fitness, diff);
	}

	// compute initial temperature
	// sort diffs
	std::sort(
		initialDiff,
		initialDiff+posDiffIx,
		[](double i, double j){
			return i < j;
		}
	);
	// index after X % of the samples
	int diffIx = initialRun_num * mInitialProb;
	double diffPerc = initialDiff[diffIx];
	// compute T such as prob = e^(- diff/T )
	// T = - (d  / ln(prob))
	double initialTemperature = - (diffPerc/log(mInitialProb));
	double temperature = initialTemperature;

	CLogger::mainlog->debug("ScheduleAlgorithmSimulatedAnnealing: initial fitness %lf", solution->fitness);
	
	CLogger::mainlog->debug("ScheduleAlgorithmSimulatedAnnealing: diffPerc %lf", diffPerc);

	CLogger::mainlog->debug("ScheduleAlgorithmSimulatedAnnealing: initial temperature %lf", temperature);

	int lowProbCount = 0; // number of moves with low acceptance probability

	bool interrupted = false;

	// punishment value for invalid schedules for the real run
	//double_max = std::numeric_limits<double>::max(); 
	double_max = 1000000000000000000.0;

	do {
		lowProbCount = 0;

		for (int loopIx = 0; loopIx < loops_num; loopIx++) {

			double prob = 0.0;

			// compute new neighbor
			solution->transfer(neighbor);
			for (int i=0; i<1; i++) {
				randomize(neighbor, height, machines, height_num, height_sets, max_num);
				if (neighbor->check() == false) {
					CLogger::mainlog->debug("ScheduleAlgorithmSimulatedAnnealing: oops");
					solution->print();
					neighbor->print();
				}
			}
			// cost difference = cost(solution) - cost(neighbor)
				fitness(neighbor, tasks, height_sets, height, machines, height_num, pTasks, max_num, &taskmap, double_max, runningTasks);

			double diff = solution->fitness - neighbor->fitness;
			// probability to adopt the neighbor, in case of diff > 0.0
			// use e^(d/T) instead of e^(d/T), because positive diff is uphill
			// negative diff is downhill
			// 0 diff -> 1.0 prob, 0.5T diff -> 0.6 prob, 1T diff -> 0.36 prob
			prob = exp( (diff / temperature));
			if (diff == 0.0) {
				prob = 1.0;
			}

			bool adopt = false;
			if (diff >= 0.0) {
				// downhill move, solution gets better
				adopt = true;
			} else {
				// uphill move, neighbor is worse
				// diff < 0.0, adopt with probability e^(diff/temperature)
				double pick = ((double)mRandom()) / ((double)mRandom.max());
				if (pick < prob) {
					adopt = true;
				}
			}

			CLogger::mainlog->debug("ScheduleAlgorithmSimulatedAnnealing: fitness %lf neighbor %lf diff %lf prob %lf temp %lf", 
			solution->fitness, neighbor->fitness, diff, prob, temperature);
			if (adopt == true) {
				// adopt
				neighbor->transfer(solution);
			}

			// count low acceptance moves
			CLogger::mainlog->debug("ScheduleAlgorithmSimulatedAnnealing: mMinProb - prob %.40lf", mMinProb - prob);
			if (prob < mMinProb) {
				CLogger::mainlog->debug("ScheduleAlgorithmSimulatedAnnealing: prob %lf minProb %lf lowProbCount %d", prob, mMinProb, lowProbCount);
				lowProbCount++;
			}

			// save best solution
			if (solution->fitness < bestSolution->fitness) {
				solution->transfer(bestSolution);
			}

			if (*interrupt == 1) {
				// oops
				interrupted = true;
				//	CLogger::mainlog->debug("ScheduleAlgorithmSimulatedAnnealing: oops");
				break;
			}
		}

		// reduce temperature
		temperature = mReduce * temperature;

		if (temperature == 0.0) {
			break;
		}
		
	} while( lowProbCount < 5 && interrupted == false );

	bestSolution->print();

	if (interrupted == false) {
		// add task to machine queue
		for (int mix=0; mix<machines; mix++) {
			for (int rix=0; rix<bestSolution->num[mix]; rix++) {
				int tix = bestSolution->tasks[max_num*mix + rix];
				STaskEntry* entry = new STaskEntry();
				entry->taskcopy = &((*pTasks)[tix]);
				entry->taskid = entry->taskcopy->mId;
				entry->stopProgress = entry->taskcopy->mCheckpoints;
				(*(sched->mpTasks))[mix]->push_back(entry);
			}
		}
		sched->computeTimes();
	} else {
		delete sched;
		sched = 0;
	}

	// clean up
	delete[] height_sets;
	delete[] height;
/*
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
*/

	// simulated annealing

	delete initialSolution;
	delete bestSolution;
	delete solution;
	delete neighbor;

	return sched;
}
