// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#define __CSCHEDULEALGORITHMMAXMIN2DYN_H__
#ifdef __CSCHEDULEALGORITHMMAXMIN2DYN_H__
#include "CScheduleAlgorithm.h"

namespace sched {
namespace algorithm {

	class CEstimation;

	class CScheduleAlgorithmMaxMin2Dyn : public CScheduleAlgorithm {

		private:
			CEstimation* mpEstimation;

		public:
			CScheduleAlgorithmMaxMin2Dyn(std::vector<CResource*>& rResources);
			~CScheduleAlgorithmMaxMin2Dyn();

			int init();
			void fini();
			CSchedule* compute(std::vector<CTaskCopy>* pTasks, std::vector<CTaskCopy*>* runningTasks, volatile int* interrupt, int updated);
	};

} }
#endif
