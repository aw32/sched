// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#define __CSCHEDULEALGORITHMSAMig2_H__
#ifdef __CSCHEDULEALGORITHMSAMig2_H__
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

	class CScheduleAlgorithmSAMig2 : public CScheduleAlgorithm {

		struct MigPart {
			int mix;
			int startProgress;
			int stopProgress;
			double complete;
			double exec;
		};

		struct MigOpt {
			struct MigPart parts[2];
			double complete;
			double exec;
		};

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
			CScheduleAlgorithmSAMig2(std::vector<CResource*>& rResources);
			~CScheduleAlgorithmSAMig2();

			int init();
			void fini();
			CSchedule* compute(std::vector<CTaskCopy>* pTasks, std::vector<CTaskCopy*>* runningTasks, volatile int* interrupt, int updated);
	};

} }
#endif
