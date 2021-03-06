// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#define __CSCHEDULEALGORITHMMAXMINMIG2_H__
#ifdef __CSCHEDULEALGORITHMMAXMINMIG2_H__
#include "CScheduleAlgorithm.h"

namespace sched {
namespace algorithm {

	class CEstimation;

	class CScheduleAlgorithmMaxMinMig2 : public CScheduleAlgorithm {

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
		};

		private:
			CEstimation* mpEstimation;

		public:
			CScheduleAlgorithmMaxMinMig2(std::vector<CResource*>& rResources);
			~CScheduleAlgorithmMaxMinMig2();

			int init();
			void fini();
			CSchedule* compute(std::vector<CTaskCopy>* pTasks, std::vector<CTaskCopy*>* runningTasks, volatile int* interrupt, int updated);
	};

} }
#endif
