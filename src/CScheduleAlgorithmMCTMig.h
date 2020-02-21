// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#define __CSCHEDULEALGORITHMMCTMIG_H__
#ifdef __CSCHEDULEALGORITHMMCTMIG_H__
#include <unordered_map>
#include "CScheduleAlgorithm.h"

namespace sched {
namespace algorithm {

	class CEstimation;


	class CScheduleAlgorithmMCTMig : public CScheduleAlgorithm {

		struct MigPart {
			int mix;
			int startProgress;
			int stopProgress;
			double complete;
		};

		struct MigOpt {
			struct MigPart parts[2];
			double complete;
		};

		struct MCTOpt {
			int mix;
			double ready;
			double complete;
			double init;
			double compute;
			double fini;
			int startProgress;
			int stopProgress;
		};

		private:
			CEstimation* mpEstimation;
			std::unordered_map<int,int> mTaskMap;
			CSchedule* mpPreviousSchedule = 0;

		public:
			CScheduleAlgorithmMCTMig(std::vector<CResource*>& rResources);
			~CScheduleAlgorithmMCTMig();

			int init();
			void fini();
			CSchedule* compute(std::vector<CTaskCopy>* pTasks, std::vector<CTaskCopy*>* runningTasks, volatile int* interrupt, int updated);
	};

} }
#endif
