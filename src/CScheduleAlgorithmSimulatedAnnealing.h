// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#ifndef __CSCHEDULEALGORITHMSIMULATEDANNEALING_H__
#define __CSCHEDULEALGORITHMSIMULATEDANNEALING_H__
#include <random>
#include <cstring>
#include <unordered_map>
#include "CLogger.h"
#include "CScheduleAlgorithm.h"
#include "CScheduleAlgorithmGenetic.h"

namespace sched {
namespace algorithm {

	class CEstimation;

/*	struct HEFTEntry {
		int taskix;
		int taskid;
		double start;
		double stop;
	};
*/
/*
	class CGeneticSchedule{
		public:
			int max_num;
			int machines;
			// array for $max_num tasks per machine
			int* tasks; // max_num * machines
			// array of number of tasks per array in $tasks
			int* num; // real max index for each machine
			double fitness;
			double mfitness; // max_fitness so far - fitness
		public:
			CGeneticSchedule(int max_num, int machines) {
				this->max_num = max_num;
				this->machines = machines;
				this->tasks = new int[max_num*machines]();
				this->num = new int[machines]();
				this->fitness = 0;
				mfitness = 0;
				
				//CLogger::mainlog->debug("ScheduleAlgorithmGenetic: max_num %d machines %d %x", max_num, machines, this);
			}
			bool check (){
				for (int tix=0; tix<max_num; tix++) {
					int f = 0;
					for (int mix=0; mix<machines; mix++) {
						for (int rix=0; rix<num[mix]; rix++) {
							if (tasks[max_num*mix + rix] == tix) {
								f = 1;
								mix = machines;
								break;
							}
						}
					}
					if (f == 0) {
						return false;
					}
				}
				return true;
			}
			CGeneticSchedule* copy(){
				CGeneticSchedule* c = new CGeneticSchedule(max_num, machines);
				this->transfer(c);
				return c;
			}
			void transfer(CGeneticSchedule* b){
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
				
				for (int i=0; i<machines*max_num; i++) {
					//CLogger::mainlog->debug("ScheduleAlgorithmGenetic: ix %d ", i);
					b->tasks[i] = 
						this->tasks[i];
				}
				
				//memcpy(b->tasks, tasks, sizeof(int)*max_num*machines);
				b->fitness = this->fitness;
				b->mfitness = this->mfitness;
			}
			~CGeneticSchedule(){
				//CLogger::mainlog->debug("ScheduleAlgorithmGenetic: ~ %x", this);
				delete[] tasks;
				delete[] num;
			}
	};

	enum EGeneticFitnessType {
		GENETIC_FITNESS_MAKESPAN,
		GENETIC_FITNESS_ENERGY
	};
*/
	class CScheduleAlgorithmSimulatedAnnealing : public CScheduleAlgorithmGenetic {

		protected:
			double mInitialProb = 0.0;
			double mLoopsFactor = 0.0;
			double mReduce = 0.0;
			double mMinProb = 0.0;

		private:

			void randomize(CGeneticSchedule* schedule, int* height, int machines, int height_num, std::vector<int>* height_sets, int max_num);

		public:
			CScheduleAlgorithmSimulatedAnnealing(std::vector<CResource*>& rResources, enum EGeneticFitnessType fitness);
			virtual ~CScheduleAlgorithmSimulatedAnnealing();

			int init();
			void fini();
			virtual CSchedule* compute(std::vector<CTaskCopy>* pTasks, std::vector<CTaskCopy*>* runningTasks, volatile int* interrupt, int updated);
	};

} }
#endif
