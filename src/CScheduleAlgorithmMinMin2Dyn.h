// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#define __CSCHEDULEALGORITHMMINMIN2DYN_H__
#ifdef __CSCHEDULEALGORITHMMINMIN2DYN_H__
#include "CScheduleAlgorithm.h"

namespace sched {
namespace algorithm {

	class CEstimation;

	class CScheduleAlgorithmMinMin2Dyn : public CScheduleAlgorithm {

		private:
			CEstimation* mpEstimation;

		public:
			CScheduleAlgorithmMinMin2Dyn(std::vector<CResource*>& rResources);
			~CScheduleAlgorithmMinMin2Dyn();

			int init();
			void fini();
			CSchedule* compute(std::vector<CTaskCopy>* pTasks, std::vector<CTaskCopy*>* runningTasks, volatile int* interrupt, int updated);
	};

} }
#endif
