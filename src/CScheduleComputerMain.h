// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#ifndef __CSCHEDULECOMPUTERMAIN_H__
#define __CSCHEDULECOMPUTERMAIN_H__
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <vector>
#include "CScheduleComputer.h"
namespace sched {
namespace task {
	class CTaskDatabase;
} }


namespace sched {
namespace algorithm {
	class CScheduleAlgorithm;
} }


namespace sched {
namespace schedule {

	using sched::task::CTaskDatabase;

	using sched::algorithm::CScheduleAlgorithm;

	class CSchedule;
	class CScheduleExecutor;
	class CFeedback;
	class CResource;

	/// @brief Implementation for schedule computer
	class CScheduleComputerMain : public CScheduleComputer {

		/// @brief Message passed to computer thread
		enum ComputerMessage {
			EXIT = 1, ///< Stop computer
			UPDATE = 2 ///< Compute a new schedule
		};

		/// @brief Action before schedule computation
		enum EExecutorInterrupt {
			NOINTERRUPT, ///< Do nothing
			GETPROGRESS, ///< Request and wait for progress of active tasks
			SUSPENDEXECUTOR ///< Suspend executor
		};

		private:
			// object registration
			std::vector<CResource*>& mrResources;
			CFeedback& mrFeedback;
			CTaskDatabase& mrTaskDatabase;
			CScheduleExecutor* mpScheduleExecutor;

			// thread/message
			EExecutorInterrupt mExecutorInterrupt = EExecutorInterrupt::NOINTERRUPT;
			int mTaskUpdate = 0;
			std::thread mThread;
			std::mutex mMessageMutex;
			std::condition_variable mMessageCondVar;
			unsigned int mMessage = 0;
			int mStopThread = 0;

			// executor suspension
			std::mutex mExecutorSuspensionMutex;
			std::condition_variable mExecutorSuspensionCondVar;

			// algorithm
			int mAlgorithmInterrupt = 0; ///< Flag to interrupt algorithm computation
			int mScheduleNum = 0; ///< Number of computed schedules
			CScheduleAlgorithm* mpAlgorithm = 0;
			std::chrono::steady_clock::time_point mAlgorithmStart;
			std::chrono::steady_clock::time_point mAlgorithmStop;
			std::chrono::steady_clock::duration mAlgorithmDuration;

			// configuration
			int mRequiredApplicationCount = 0;
			int mRegisteredApplications = 0;

		private:
			void compute();
			int computeAlgorithm();
			int suspendExecutor();

		public:
			// object registration
			void setScheduleExecutor(CScheduleExecutor* pScheduleExecutor);

			int start();
			void stop();
			void executorSuspended();
			void computeSchedule();
			CScheduleComputerMain(std::vector<CResource*>& rResources, CFeedback& rFeedback, CTaskDatabase& rTaskDatabase);
			int loadAlgorithm();
			int getRequiredApplicationCount();
			int getRegisteredApplications();
			virtual ~CScheduleComputerMain();
	};

} }
#endif
