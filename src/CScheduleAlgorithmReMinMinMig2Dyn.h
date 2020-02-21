// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#define __CSCHEDULEALGORITHMREMINMINMIG2DYN_H__
#ifdef __CSCHEDULEALGORITHMREMINMINMIG2DYN_H__
#include "CScheduleAlgorithm.h"

namespace sched {
namespace algorithm {

	class CEstimation;

	class CScheduleAlgorithmReMinMinMig2Dyn : public CScheduleAlgorithm {

		struct MigPart {
			int mix;
			int startProgress;
			int stopProgress;
			double complete;
		};
	
		struct MigOpt {
			int tix;
			struct MigPart parts[2];
			double complete;
			double makespan;
			double energyTotal;
			double energyDynamic;
		};

		private:
			CEstimation* mpEstimation;

		public:
			CScheduleAlgorithmReMinMinMig2Dyn(std::vector<CResource*>& rResources);
			~CScheduleAlgorithmReMinMinMig2Dyn();

			int init();
			void fini();
			CSchedule* compute(std::vector<CTaskCopy>* pTasks, std::vector<CTaskCopy*>* runningTasks, volatile int* interrupt, int updated);
	};

} }
#endif
