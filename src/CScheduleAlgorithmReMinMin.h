// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#define __CSCHEDULEALGORITHMREMINMIN_H__
#ifdef __CSCHEDULEALGORITHMREMINMIN_H__
#include "CScheduleAlgorithm.h"

namespace sched {
namespace algorithm {

	class CEstimation;

	class CScheduleAlgorithmReMinMin : public CScheduleAlgorithm {

		private:
			CEstimation* mpEstimation;

		public:
			CScheduleAlgorithmReMinMin(std::vector<CResource*>& rResources);
			~CScheduleAlgorithmReMinMin();

			int init();
			void fini();
			CSchedule* compute(std::vector<CTaskCopy>* pTasks, std::vector<CTaskCopy*>* runningTasks, volatile int* interrupt, int updated);
	};

} }
#endif
