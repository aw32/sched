// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#ifndef __CSCHEDULEALGORITHMSIMULATEDANNEALINGMIGDYN_H__
#define __CSCHEDULEALGORITHMSIMULATEDANNEALINGMIGDYN_H__
#include <random>
#include <cstring>
#include <unordered_map>
#include "CLogger.h"
#include "CScheduleAlgorithm.h"
#include "CScheduleAlgorithmGeneticMigDyn.h"

namespace sched {
namespace algorithm {

	class CEstimation;

	class CScheduleAlgorithmSimulatedAnnealingMigDyn : public CScheduleAlgorithmGeneticMigDyn {

		protected:
			double mInitialProb = 0.0;
			double mLoopsFactor = 0.0;
			double mReduce = 0.0;
			double mMinProb = 0.0;

		private:

			void randomize(CGeneticSchedule* schedule, int* height, int machines, int height_num, std::vector<int>* height_sets, int max_num);

		public:
			CScheduleAlgorithmSimulatedAnnealingMigDyn(std::vector<CResource*>& rResources, enum EGeneticFitnessType fitness);
			virtual ~CScheduleAlgorithmSimulatedAnnealingMigDyn();

			int init();
			void fini();
			virtual CSchedule* compute(std::vector<CTaskCopy>* pTasks, std::vector<CTaskCopy*>* runningTasks, volatile int* interrupt, int updated);
	};

} }
#endif
