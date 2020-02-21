// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#ifndef __CSCHEDULEALGORITHM_H__
#define __CSCHEDULEALGORITHM_H__
#include <vector>
#include <mutex>
namespace sched {
namespace schedule {
	class CSchedule;
	class CResource;
} }


namespace sched {
namespace task {
	class CTaskCopy;
} }


namespace sched {
namespace algorithm {

	using sched::schedule::CSchedule;
	using sched::schedule::CResource;

	using sched::task::CTaskCopy;

	/// @brief Algorithm that computes a new schedule
	class CScheduleAlgorithm {

		protected:
			std::vector<CResource*>& mrResources; ///< List of resources

		public:
			CScheduleAlgorithm(std::vector<CResource*>& rResources);
			/// @brief Initialize algorithm
			virtual int init() = 0;
			/// @brief Compute new schedule
			/// @param pTasks List of current tasks
			/// @param runningTasks List of currently active tasks per resource, may contain null if resource is idling
			/// @param interrupt Pointer to interrupt variable to abort algorithm
			/// @param updated 1 if task progress was updated before schedule computation, else 0
			/// @return Return new schedule or null if algorithm was aborted
			virtual CSchedule* compute(std::vector<CTaskCopy>* pTasks, std::vector<CTaskCopy*>* runningTasks, volatile int* interrupt, int updated) = 0;
			/// @brief Clean up algorithm
			virtual void fini() = 0;
			virtual ~CScheduleAlgorithm();
	};

} }
#endif
