// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#ifndef __CSCHEDULEEXECUTOR_H__
#define __CSCHEDULEEXECUTOR_H__

namespace sched {
namespace schedule {

	class CSchedule;
	class CScheduleComputer;

	/// @brief Applies schedule to resources
	class CScheduleExecutor {

		public:
			CScheduleExecutor();
			virtual ~CScheduleExecutor();
			
			/// @brief Start executor
			virtual int start() = 0;
			/// @brief Stop executor
			virtual void stop() = 0;
			/// @brief Suspends all tasks and executor
			virtual void suspendSchedule() = 0;
			/// @brief Replace current schedule and restart executor
			/// @param pSchedule new Schedule
			virtual void updateSchedule(CSchedule* pSchedule) = 0;
			/// @brief Set computer
			/// @param pScheduleComputer
			virtual void setScheduleComputer(CScheduleComputer* pScheduleComputer) = 0;
			/// @brief Notifies executor of task change
			virtual void operationDone() = 0;

			/// @brief Returns current schedule
			/// @return current schedule
			virtual CSchedule* getCurrentSchedule() = 0;

			/// @brief Returns current loop number
			/// @return current loop number
			virtual int getCurrentLoop() = 0;
			/// @brief Waits until the current loop number differs from the given number and return the new loop number
			/// @param current Old loop number
			/// @return new loop number
			virtual int getNextLoop(int current) = 0;

	};

} }
#endif
