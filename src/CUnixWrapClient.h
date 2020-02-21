// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#ifndef __CUNIXWRAPCLIENT_H__
#define __CUNIXWRAPCLIENT_H__
#include <mutex>
#include <condition_variable>
#include <vector>
#include <unordered_map>
#include "ETaskOnEnd.h"

namespace sched {
namespace com {
	class CComUnixServer;
} }

namespace sched {
namespace task {
	class CTaskDatabase;
	class CTaskWrapper;
} }

namespace sched {
namespace schedule {
	class CResource;
} }

namespace sched {
namespace wrap {

	using sched::task::ETaskOnEnd;
	using sched::com::CComUnixServer;

	using sched::task::CTaskDatabase;
	using sched::task::CTaskWrapper;

	using sched::schedule::CResource;

	class CTaskDefinitions;
	class CWrapTaskGroup;
	class CComUnixSchedSchedulerWrap;
	class CComUnixSchedClientWrap;

	/// @brief Manages starting and stopping of task application, scheduler registration and message forwarding
	///
	/// The wrapper program combines several task applications to one application.
	/// The applications are started and register to the wrapper.
	/// Once all applications registered the wrapper registers as one application to the scheduler.
	/// Afterwards messages between applications and scheduler are being forwarded, while the task ids are corrected.
	class CUnixWrapClient {

		std::vector<CResource*>& mrResources;
		CTaskDefinitions& mrDefinitions;
		CTaskDatabase& mrTaskDatabase;
		CWrapTaskGroup& mrGroup;
		CComUnixServer* mpServer;

		CComUnixSchedSchedulerWrap* schedulerClient;

		std::vector<CTaskWrapper*> allTasks;

		std::unordered_map<int, int> taskSchedToGlobal; // sched id -> taskdb id
		std::unordered_map<int, int> taskGlobalToSched; // taskdb id -> sched id

		char* mpSocketPath = 0;
		char* mpLogPrefix = 0;

		std::mutex mWrapMutex;
		std::condition_variable mWrapCondVar;

		int mTotalApps = 0;
		int mRegisteredApps = 0;
		int mErrorApps = 0;
		bool registered = false;

		protected:
			/// @brief Connect to scheduler and create SchedulerWrap object
			CComUnixSchedSchedulerWrap* connectToScheduler();

		public:
			/// @param definitions
			/// @param group
			/// @param rResources
			/// @param rTaskDatabase
			CUnixWrapClient(CTaskDefinitions& definitions, CWrapTaskGroup& group, std::vector<CResource*>& rResources, CTaskDatabase& rTaskDatabase);
			/// @brief Sets UnixServer
			void setServer(CComUnixServer* pServer);

			~CUnixWrapClient();

			/// @brief Initializes the applications
			///
			/// Starts all applications one by one.
			/// The applications register to the wrapper.
			/// If one applications fails the wrapper shuts down.
			/// @return 0 if successfull, else -1
			int initialize();

			/// @brief Registers to the scheduler
			///
			/// The wrapper registers all tasks to the scheduler according to the group definition.
			/// @return 0 if successfull, else -1
			int registerGroup();


			// hooks for messages from apps
			/// @brief Reacts to STARTED message by client
			void clientStarted(CComUnixSchedClientWrap& client, int taskid);
			/// @brief Reacts to SUSPENDED message by client
			void clientSuspended(CComUnixSchedClientWrap& client, int taskid, int progress);
			/// @brief Reacts to ABORTED message by client
			void clientAborted(CComUnixSchedClientWrap& client);
			/// @brief Reacts to PROGRESS message by client
			void clientProgress(CComUnixSchedClientWrap& client, int taskid, int progress);
			/// @brief Reacts to TASKLIST message by client
			void clientTasklist(CComUnixSchedClientWrap& client, std::vector<CTaskWrapper*>* newtasks);
			/// @brief Reacts to QUIT message by client
			void clientQuit(CComUnixSchedClientWrap& client);
			/// @brief Reacts to FINISHED message by client
			void clientFinished(CComUnixSchedClientWrap& client, int taskid);

			/// @brief Reacts to client failure
			void clientFail(CComUnixSchedClientWrap& client);

			// hooks for messages from scheduler
			/// @brief Reacts to TASK_START message by scheduler
			void serverTaskStart(CComUnixSchedSchedulerWrap& scheduler, int taskid, CResource& resource, int targetProgress, ETaskOnEnd onEnd);
			/// @brief Reacts to TASK_SUSPEND message by scheduler
			void serverTaskSuspend(CComUnixSchedSchedulerWrap& scheduler, int taskid);
			/// @brief Reacts to TASK_ABORT message by scheduler
			void serverTaskAbort(CComUnixSchedSchedulerWrap& scheduler, int taskid);
			/// @brief Reacts to TASK_PROGRESS message by scheduler
			void serverTaskProgress(CComUnixSchedSchedulerWrap& scheduler, int taskid);
			/// @brief Reacts to TASKIDS message by scheduler
			void serverTaskIds(CComUnixSchedSchedulerWrap& scheduler, int* taskids);
			/// @brief Reacts to QUIT message by scheduler
			void serverQuit(CComUnixSchedSchedulerWrap& scheduler);

			/// @brief Reacts to schedule failure
			void serverFail(CComUnixSchedSchedulerWrap& scheduler);


	};

} }
#endif
