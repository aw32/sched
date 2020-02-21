// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#define __CSCHEDULEALGORITHMHEFT2DYN_H__
#ifdef __CSCHEDULEALGORITHMHEFT2DYN_H__
#include "CScheduleAlgorithm.h"

namespace sched {
namespace algorithm {

	class CEstimation;


	class CScheduleAlgorithmHEFT2Dyn : public CScheduleAlgorithm {

		struct HEFTEntry {
			int taskix;
			int taskid;
			double start;
			double stop;
		};

		private:
			CEstimation* mpEstimation;

		public:
			CScheduleAlgorithmHEFT2Dyn(std::vector<CResource*>& rResources);
			~CScheduleAlgorithmHEFT2Dyn();

			int init();
			void fini();
			CSchedule* compute(std::vector<CTaskCopy>* pTasks, std::vector<CTaskCopy*>* runningTasks, volatile int* interrupt, int updated);
	};

} }
#endif
