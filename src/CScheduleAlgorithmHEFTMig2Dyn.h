// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#define __CSCHEDULEALGORITHMHEFTMIG2DYN_H__
#ifdef __CSCHEDULEALGORITHMHEFTMIG2DYN_H__
#include "CScheduleAlgorithm.h"

namespace sched {
namespace algorithm {

	class CEstimation;


	class CScheduleAlgorithmHEFTMig2Dyn : public CScheduleAlgorithm {

		private:
			CEstimation* mpEstimation;

		public:
			CScheduleAlgorithmHEFTMig2Dyn(std::vector<CResource*>& rResources);
			~CScheduleAlgorithmHEFTMig2Dyn();

			int init();
			void fini();
			CSchedule* compute(std::vector<CTaskCopy>* pTasks, std::vector<CTaskCopy*>* runningTasks, volatile int* interrupt, int updated);
	};

} }
#endif
