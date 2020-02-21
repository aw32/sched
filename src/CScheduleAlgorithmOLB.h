// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#define __CSCHEDULEALGORITHMOLB_H__
#ifdef __CSCHEDULEALGORITHMOLB_H__
#include <unordered_map>
#include "CScheduleAlgorithm.h"

namespace sched {
namespace algorithm {

	class CEstimation;

	class CScheduleAlgorithmOLB : public CScheduleAlgorithm {

		private:
			CEstimation* mpEstimation;
			std::unordered_map<int,int> mTaskMap;
			CSchedule* mpPreviousSchedule = 0;

		public:
			CScheduleAlgorithmOLB(std::vector<CResource*>& rResources);
			~CScheduleAlgorithmOLB();

			int init();
			void fini();
			CSchedule* compute(std::vector<CTaskCopy>* pTasks, std::vector<CTaskCopy*>* runningTasks, volatile int* interrupt, int updated);
	};

} }
#endif
