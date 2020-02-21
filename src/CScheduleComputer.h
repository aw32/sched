// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#ifndef __CSCHEDULECOMPUTER_H__
#define __CSCHEDULECOMPUTER_H__

namespace sched {
namespace schedule {

	class CScheduleExecutor;

	/// @brief Manages the algorithm for schedule computation
	class CScheduleComputer {
		public:
			CScheduleComputer();
			virtual ~CScheduleComputer();

			/// @brief Executor stopped
			virtual void executorSuspended() = 0;
			/// @brief Load algorithm from configuration
			virtual int loadAlgorithm() = 0;
			/// @brief Set executor
			/// @param pScheduleExecutor
			virtual void setScheduleExecutor(CScheduleExecutor* pScheduleExecutor) = 0;
			/// @brief Starts computer
			virtual int start() = 0;
			/// @brief Stops computer
			virtual void stop() = 0;
			/// @brief Compute a new schedule
			virtual void computeSchedule() = 0;
			/// @brief Get number of required applications for schedule computation
			virtual int getRequiredApplicationCount() = 0;
			/// @brief Get number of already registered applications
			virtual int getRegisteredApplications() = 0;
	};

} }
#endif
