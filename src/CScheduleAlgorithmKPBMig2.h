// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#define __CSCHEDULEALGORITHMKPBMIG2_H__
#ifdef __CSCHEDULEALGORITHMKPBMIG2_H__
#include <unordered_map>
#include "CScheduleAlgorithm.h"

namespace sched {
namespace algorithm {

	class CEstimation;

	class CScheduleAlgorithmKPBMig2 : public CScheduleAlgorithm {

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

		private:
			CEstimation* mpEstimation;
			std::unordered_map<int,int> mTaskMap;
			CSchedule* mpPreviousSchedule = 0;
			double mPercentage = 0.0;

		public:
			CScheduleAlgorithmKPBMig2(std::vector<CResource*>& rResources);
			~CScheduleAlgorithmKPBMig2();

			int init();
			void fini();
			CSchedule* compute(std::vector<CTaskCopy>* pTasks, std::vector<CTaskCopy*>* runningTasks, volatile int* interrupt, int updated);

			/// @brief Find index for next spot in array
			///
			/// Searches for next place in array that should be filled.
			/// If the array is not full this is an empty spot.
			/// Else it is the option with worst execution time.
			/// @param options Array of options
			/// @param num Size of array
			/// @return Index to use next
			int nextMETOption(struct MigOpt* options, int num);
	};

} }
#endif
