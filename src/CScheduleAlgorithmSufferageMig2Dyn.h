// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#define __CSCHEDULEALGORITHMSUFFERAGEMIG2DYN_H__
#ifdef __CSCHEDULEALGORITHMSUFFERAGEMIG2DYN_H__
#include "CScheduleAlgorithm.h"

namespace sched {
namespace algorithm {

	class CEstimation;

	class CScheduleAlgorithmSufferageMig2Dyn : public CScheduleAlgorithm {

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
			double sufferage;
		};

		private:
			CEstimation* mpEstimation;

		public:
			CScheduleAlgorithmSufferageMig2Dyn(std::vector<CResource*>& rResources);
			~CScheduleAlgorithmSufferageMig2Dyn();

			int init();
			void fini();
			CSchedule* compute(std::vector<CTaskCopy>* pTasks, std::vector<CTaskCopy*>* runningTasks, volatile int* interrupt, int updated);
	};

} }
#endif
