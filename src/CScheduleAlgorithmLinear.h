// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#ifndef __CSCHEDULEALGORITHMLINEAR_H__
#define __CSCHEDULEALGORITHMLINEAR_H__
#include "CScheduleAlgorithm.h"
namespace sched {
namespace algorithm {

	class CScheduleAlgorithmLinear : public CScheduleAlgorithm {

		private:
			int mCurrentResourceIndex = 0;

		public:
			CScheduleAlgorithmLinear(std::vector<CResource*>& rResources);
			int init();
			CSchedule* compute(std::vector<CTaskCopy>* pTasks, std::vector<CTaskCopy*>* runningTasks, volatile int* interrupt, int updated);
			void fini();
			~CScheduleAlgorithmLinear();

	};

} }
#endif
