// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#ifndef __CSCHEDULEALGORITHMGENETICDYN_H__
#define __CSCHEDULEALGORITHMGENETICDYN_H__
#include <random>
#include <cstring>
#include <unordered_map>
#include "CLogger.h"
#include "CScheduleAlgorithm.h"

namespace sched {
namespace algorithm {

	class CEstimation;




	class CScheduleAlgorithmGeneticDyn : public CScheduleAlgorithm {
		public:
		enum EGeneticFitnessType {
			GENETIC_FITNESS_MAKESPAN,
			GENETIC_FITNESS_ENERGY
		};

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
				CGeneticSchedule(int max_num, int machines);
				bool check();
				void print();
				CGeneticSchedule* copy();
				void transfer(CGeneticSchedule* b);
				~CGeneticSchedule();
		};

		protected:
			CEstimation* mpEstimation;
			std::mt19937 mRandom;
			enum EGeneticFitnessType mFitnessType = GENETIC_FITNESS_MAKESPAN;

		protected:
			CGeneticSchedule* generateSchedule(
				std::vector<int>* height_sets, int height_num, int machines, int max_num, std::vector<CTaskCopy>* pTasks
				);

			void mutate(CGeneticSchedule* sched, int* height, int machines, int height_num, std::vector<int>* height_sets, int max_num);


			void crossover(CGeneticSchedule* a, CGeneticSchedule* b, int height_num, int machines, int* height, int max_num);

			void reproduction(std::vector<CGeneticSchedule*>* pop, std::vector<CGeneticSchedule*>* newpop);


			void fitness(CGeneticSchedule* sched, int tasks, std::vector<int>* height_sets, int* height, int machines, int height_num, std::vector<CTaskCopy>* pTasks, int max_num, std::unordered_map<int,int>* taskmap, double double_max, std::vector<CTaskCopy*>* runningTasks);

			void fitnessMakespan(CGeneticSchedule* sched, int tasks, std::vector<int>* height_sets, int* height, int machines, int height_num, std::vector<CTaskCopy>* pTasks, int max_num, std::unordered_map<int,int>* taskmap, double double_max, std::vector<CTaskCopy*>* runningTasks);

			void fitnessEnergy(CGeneticSchedule* sched, int tasks, std::vector<int>* height_sets, int* height, int machines, int height_num, std::vector<CTaskCopy>* pTasks, int max_num, std::unordered_map<int,int>* taskmap, double double_max, std::vector<CTaskCopy*>* runningTasks);

			int findRunMix(std::vector<CTaskCopy*>* tasks, int tid);
		public:
			CScheduleAlgorithmGeneticDyn(std::vector<CResource*>& rResources, enum EGeneticFitnessType fitness);
			virtual ~CScheduleAlgorithmGeneticDyn();

			int init();
			void fini();
			virtual CSchedule* compute(std::vector<CTaskCopy>* pTasks, std::vector<CTaskCopy*>* runningTasks, volatile int* interrupt, int updated);
	};

} }
#endif
