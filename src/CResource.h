// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#ifndef __CRESOURCE_H__
#define __CRESOURCE_H__
#include <string>
#include <vector>
#include <mutex>
#include <map>
#include <chrono>
//#include <thread>
#include <functional>
#include "CExternalHook.h"
#include "CTimer.h"
#include "CTask.h"
namespace sched {
namespace task {
	class CTaskDatabase;
	class CTaskWrapper;
} }


namespace sched {
namespace schedule {

	using sched::task::CTaskDatabase;
	using sched::task::CTaskWrapper;
	using sched::task::ETaskState;

	class CFeedback;
	class CScheduleExecutor;
	struct STaskEntry;
	class CSchedule;

	/// @brief Resource representation and active task management
	class CResource {

		/// @brief Defines mode of task end
		enum ETaskRunUntil {
			PROGRESS_SUSPEND, ///< Wait for task suspension
			ESTIMATION_TIMER ///< Start a timer that stops the task
		};

		private:
			static ETaskRunUntil sTaskRunUntil;

		private:
			CFeedback* mpFeedback;
			CTaskDatabase& mrTaskDatabase;
			CScheduleExecutor* mpScheduleExecutor;
			std::mutex mResourceMutex;
			CTimer mProgressTimer;
			std::function<void()> mProgressTimedOutCall;
			// status
			int mExpectProgress = 0; ///< Expected task target progress
			int mSuspendOnceRunning = 0; ///< Suspend tasks once it started
			STaskEntry* mpTaskEntry = 0; ///< Current task's schedule entry
			CTaskWrapper* mpTask = 0; ///< Current task

			// hooks
			CExternalHook* mpTaskEndHook = 0;
			int mTaskEndHookLastStatus = 0; ///< Last returned status of the hook

		public:
			int mId = -1;
			std::string mName;
			std::map<std::string, void*> mAttributes;

		private:
			void execSuspendTask();
			void execTaskEndHook();

		public:
			CResource(CTaskDatabase& rTaskDatabase);
			~CResource();


			// object registration
			void setScheduleExecutor(CScheduleExecutor* pScheduleExecutor);
			void setFeedback(CFeedback* pFeedback);

			// executor
			/// @brief Start task on resource
			/// @param task Schedule entry of started task
			void startTask(STaskEntry* task);
			/// @brief Send suspend message to task
			void suspendTask();
			/// @brief Request current task progress
			/// @return Returns 1 if no progress can be returned, elsethe task was messaged and 0 is returned
			int requestProgress();
			/// @brief Update task parameters
			/// @param task
			/// @param schedule
			void updateTask(STaskEntry* task, CSchedule* schedule);
			/// @brief Handles resource actions after a executor loop iteration
			void postManage();
			/// @brief Handles resource actions in case the resource is expected to idle
			void idle();

			// task
			/// @brief Task started message
			/// @param task
			void taskStarted(CTaskWrapper& task);
			/// @brief Task suspended message
			/// @param task
			/// @param progress
			void taskSuspended(CTaskWrapper& task, int progress);
			/// @brief Task finished message
			/// @param task
			void taskFinished(CTaskWrapper& task);
			/// @brief Task was aborted message
			/// @param task
			void taskAborted(CTaskWrapper& task);
			/// @brief Task progress response message
			/// @param task
			/// @param progress Current task progress
			void taskProgress(CTaskWrapper& task, int progress);
			/// @brief Task disconnected
			/// @param task
			void clientDisconnected(CTaskWrapper& task);

			// time out
			void progressTimedOut();

			/// @brief Copies current status
			/// @param expectProgress Expected progress out parameter
			/// @param taskentry Current task's schedule entry out parameter
			/// @param task Current task out parameter
			/// @param state Current task state out parameter
			void getStatus(int* expectProgress, STaskEntry** taskentry, CTaskWrapper** task, ETaskState* state);

			/// @brief Load resources from configuration
			/// @param list Result list
			/// @param rTaskDatabase Task database
			static int loadResources(std::vector<CResource*>* list, CTaskDatabase& rTaskDatabase);
	};

} }
#endif
