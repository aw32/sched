// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#ifndef __CSCHEDULEALGORITHMGENETICMIGSOLVERLPENERGY_H__
#define __CSCHEDULEALGORITHMGENETICMIGSOLVERLPENERGY_H__
#include <vector>
#include <unordered_map>
#include "CScheduleAlgorithmGeneticMig.h"
#include "CTaskCopy.h"

namespace sched {
namespace algorithm {

	class CScheduleAlgorithmGeneticMigSolverLPEnergy {

		struct Task {

			int mix_a;
			int mix_b;
			int slot_a;
			int slot_b;
			bool merge;

			double rm_a = 0.0;
			double fm_a = 0.0;
			double rm_b = 0.0;
			double fm_b = 0.0;
			double rd_a = 0.0;
			double fd_a = 0.0;
			double rd_b = 0.0;
			double fd_b = 0.0;

		};

		public:
		static void solve(CScheduleAlgorithmGeneticMig::CGeneticSchedule* schedule,
		std::vector<CResource*>& rResources,
		CEstimation* est,
		std::vector<CTaskCopy>* pTasks,
		std::unordered_map<int,int>* taskmap,
		double max_double,
		std::vector<CTaskCopy*>* runningTasks,
		char* solverPath);
	};

} }

#endif
