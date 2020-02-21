// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#ifndef __CSCHEDULEEXECUTORMAIN_H__
#define __CSCHEDULEEXECUTORMAIN_H__
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include "CScheduleExecutor.h"
namespace sched {
namespace task {
	class CTaskDatabase;
} }


namespace sched {
namespace schedule {

	using sched::task::CTaskDatabase;

	class CResource;
	class CScheduleComputer;
	class CSchedule;

	/// @brief Implementation for schedule executor
	class CScheduleExecutorMain : public CScheduleExecutor {

		/// @brief Message to schedule executor
		enum ExecutorMessage {
			EXIT = 1, ///< Stop the executor
			SCHEDULE = 2, ///< A new schedule was passed
			RESOURCE = 4 ///< A task change occured
		};

		/// @brief Executor state
		enum EScheduleState {
			ACTIVE, ///< Executor is active
			INACTIVE ///< Executor is inactive
		};


		private:
			// object registration
			std::vector<CResource*>& mrResources;
			CTaskDatabase& mrTaskDatabase;
			CScheduleComputer* mpScheduleComputer;

			std::thread mThread;
			std::mutex mMessageMutex;
			std::condition_variable mMessageCondVar;
			int mStopThread = 0;
			unsigned int mMessage = 0;
			CSchedule* mpNewSchedule = 0;
			CSchedule* mpCurrentSchedule = 0;
			std::vector<CSchedule*> mLastSchedules; ///< List of old schedules, saved because of dangling pointers
			EScheduleState mState = EScheduleState::ACTIVE;
			std::mutex mLoopMutex;
			std::condition_variable mLoopCondVar;
			int mLoopCounter = 0;
			bool mReschedule = false; ///< Trigger reschedule if all resource are idling, but not all tasks are done
			
		private:
			/// @brief Executor thread main function
			void execute();
			void setupSchedule(CSchedule* pSchedule);
			/// @brief Compare schedule and resource state and react accordingly
			/// @return Number of active resources
			int manageResources();

		public:
			// object registration
			void setScheduleComputer(CScheduleComputer* pScheduleComputer);

			// schedule computer operations
			void updateSchedule(CSchedule* pSchedule);
			void suspendSchedule();

			// resource operations
			void operationDone();
			CSchedule* getCurrentSchedule();

			int getCurrentLoop();
			int getNextLoop(int current);
			int start();
			void stop();
			CScheduleExecutorMain(std::vector<CResource*>& rResources, CTaskDatabase& rTaskDatabase);
			virtual ~CScheduleExecutorMain();
	};

} }
#endif
