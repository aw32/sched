// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#ifndef __CCOMSCHEDSCHEDULER_H__
#define __CCOMSCHEDSCHEDULER_H__
#include <vector>
#include "ETaskOnEnd.h"
namespace sched {
namespace task {
	class CTaskWrapper;
} }


namespace sched {
namespace schedule {
	class CResource;
} }


namespace sched {
namespace com {

	using sched::task::CTaskWrapper;
	using sched::task::ETaskOnEnd;

	using sched::schedule::CResource;

	/// @brief Interface for communcation classes representing the scheduler.
	///
	/// The implementing class represents the scheduler.
	/// The class may be used to send messages, e.g. STARTED, SUSPENDED etc.
	/// The class my react to message sent from scheduling clients.
	class CComSchedScheduler {

		public:

			// messages sent from client to scheduler

			/// @brief Task STARTED
			/// @param taskid Task id
			/// @return 0 if successfull, else -1
			virtual int started(int taskid) = 0;

			/// @brief Task SUSPENDED
			/// @param taskid Task id
			/// @param progress Current task progress
			/// @return 0 if successfull, else -1
			virtual int suspended(int taskid, int progress) = 0;

			/// @brief Task FINISHED
			/// @param taskid Task id
			/// @return 0 if successfull, else -1
			virtual int finished(int taskid) = 0;

			/// @brief Task PROGRESS
			/// @param taskid Task id
			/// @param progress Current task progress
			/// @return 0 if successfull, else -1
			virtual int progress(int taskid, int progress) = 0;

			/// @brief TASKLIST
			/// @param list List of tasks to register
			/// @return 0 if successfull, else -1
			virtual int tasklist(std::vector<CTaskWrapper*>* list) = 0;

			/// @brief Send QUIT
			/// @return 0 if successfull, else -1
			virtual int closeClient() = 0;

			// messages received from scheduler

			/// @brief React to TASK_START
			/// @param taskid Task id
			/// @param resource Resource to start the task on
			/// @param targetProgress Target checkpoint
			/// @param onEnd Behaviour when checkpoint is reached
			virtual void onStart(int taskid, CResource& resource, int targetProgress, ETaskOnEnd onEnd) = 0;

			/// @brief React to TASK_SUSPEND
			/// @param taskid Task id
			virtual void onSuspend(int taskid) = 0;

			/// @brief React to TASK_ABORT
			/// @param taskid Task id
			virtual void onAbort(int taskid) = 0;

			/// @brief React to TASK_PROGRESS
			/// @param taskid Task id
			virtual void onProgress(int taskid) = 0;

			/// @brief React to TASKIDS
			/// @param taskids Task id
			virtual void onTaskids(int* taskids) = 0;

			/// @brief React to QUIT
			virtual void onQuit() = 0;

			/// @brief React to scheduler failure
			virtual void onFail() = 0;

			virtual ~CComSchedScheduler();

	};

} }
#endif
