// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#define __CSCHEDULEALGORITHMKPB2_H__
#ifdef __CSCHEDULEALGORITHMKPB2_H__
#include <unordered_map>
#include "CScheduleAlgorithm.h"

namespace sched {
namespace algorithm {

	class CEstimation;

	class CScheduleAlgorithmKPB2 : public CScheduleAlgorithm {

		private:
			CEstimation* mpEstimation;
			std::unordered_map<int,int> mTaskMap;
			CSchedule* mpPreviousSchedule = 0;
			double mPercentage = 0.0;

		public:
			CScheduleAlgorithmKPB2(std::vector<CResource*>& rResources);
			~CScheduleAlgorithmKPB2();

			int init();
			void fini();
			CSchedule* compute(std::vector<CTaskCopy>* pTasks, std::vector<CTaskCopy*>* runningTasks, volatile int* interrupt, int updated);
	};

} }
#endif
