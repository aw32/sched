// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#define __CSCHEDULEALGORITHMSA2_H__
#ifdef __CSCHEDULEALGORITHMSA2_H__
#include <unordered_map>
#include "CScheduleAlgorithm.h"
namespace sched {
namespace schedule {
	class CScheduleExt;
} }

namespace sched {
namespace algorithm {

	using sched::schedule::CScheduleExt;

	class CEstimation;

	class CScheduleAlgorithmSA2 : public CScheduleAlgorithm {

		enum ESAMode {
			SA_MODE_MCT,
			SA_MODE_MET
		};

		private:
			CEstimation* mpEstimation;
			std::unordered_map<int,int> mTaskMap;
			CSchedule* mpPreviousSchedule = 0;
			double mRatioLower = 0.0;
			double mRatioHigher = 0.0;
			double mRatio = 0.0;
			ESAMode mMode = SA_MODE_MCT;

			void updateRatio(CScheduleExt* sched, int machines);

		public:
			CScheduleAlgorithmSA2(std::vector<CResource*>& rResources);
			~CScheduleAlgorithmSA2();

			int init();
			void fini();
			CSchedule* compute(std::vector<CTaskCopy>* pTasks, std::vector<CTaskCopy*>* runningTasks, volatile int* interrupt, int updated);
	};

} }
#endif
