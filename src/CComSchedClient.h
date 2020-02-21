// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#ifndef __CCOMSCHEDCLIENT_H__
#define __CCOMSCHEDCLIENT_H__
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

	/// @brief Interface for communication classes representing a scheduler client.
	///
	/// The implementing class represents a scheduling client, e.g. an application.
	/// The class may be used to initiate operations such as TASK_START, TASK_SUSPEND etc.
	/// The class may react to operations initiated by the represented task.
	class CComSchedClient {

		public:
			/// @brief Start task
			/// @param resource Resource to start the task on
			/// @param targetProgress Target checkpoint to reach
			/// @param onEnd Behaviour when task reaches target checkpoint
			/// @param task Task to start
			/// @return 0 if successfull, else -1
			virtual int start(CResource& resource, int targetProgress, ETaskOnEnd onEnd, CTaskWrapper& task) = 0;

			/// @brief Suspend task
			/// @param task Task to suspend
			/// @return 0 if successfull, else -1
			virtual int suspend(CTaskWrapper& task) = 0;

			/// @brief Abort task
			/// @param task Task to abort
			/// @return 0 if successfull, else -1
			virtual int abort(CTaskWrapper& task) = 0;

			/// @brief Request task progress
			/// @param task Task to request
			/// @return 0 if successfull, else -1
			virtual int progress(CTaskWrapper& task) = 0;

			/// @brief Send taskids as response to TASKLIST
			/// @param tasks List of tasks containing reserved ids
			/// @return 0 if successfull, else -1
			virtual int taskids(std::vector<CTaskWrapper*>* tasks) = 0;

			/// @brief Send QUIT message and close socket
			virtual void closeClient() = 0;


			/// @brief React to TASKLIST message
			/// @param list Received list of new tasks
			virtual void onTasklist(std::vector<CTaskWrapper*>* list) = 0;

			/// @brief React to STARTED message
			/// @param taskid Task id
			virtual void onStarted(int taskid) = 0;

			/// @brief React to SUSPENDED message
			/// @param taskid Task id
			/// @param progress Current checkpoint
			virtual void onSuspended(int taskid, int progress) = 0;

			/// @brief React to FINISHED message
			/// @param taskid Task id
			virtual void onFinished(int taskid) = 0;

			/// @brief React to PROGRESS message
			/// @param taskid Task id
			/// @param progress Current checkpoint
			virtual void onProgress(int taskid, int progress) = 0;

			/// @brief React to QUIT message
			virtual void onQuit() = 0;

			virtual ~CComSchedClient();

	};

} }
#endif
