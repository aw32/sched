// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#ifndef __CTASKWRAPPER_H__
#define __CTASKWRAPPER_H__
#include "CTask.h"
#include "ETaskOnEnd.h"
namespace sched {
namespace com {
	class CComSchedClient;
} }


namespace sched {
namespace schedule {
	class CResource;
} }


namespace sched {
namespace task {

	using sched::com::CComSchedClient;

	using sched::schedule::CResource;

	class CTaskWrapper;
	class CTaskDatabase;

	/// @brief Extended task information
	/// This class contains the pointer to the associated client object.
	/// It also contains methods to manage task operations.
	class CTaskWrapper : public CTask {

		public:
			const static char* stateStrings[]; ///< Strings representing the task state

		private:
			CTaskDatabase& mrTaskDatabase;
			CComSchedClient* mpClient = 0;
			std::vector<CTaskWrapper*> mpDependencies;

		public:
			// Scheduler-side operations
			/// @brief Send TASK_START message
			/// @param resource
			/// @param targetProgress
			/// @param onEnd
			int start(CResource& resource, int targetProgress, ETaskOnEnd onEnd);
			/// @brief Send TASK_SUSPEND message
			int suspend();
			/// @brief Abort task
			void abort();
			/// @brief Send PROGRESS request message
			int getProgress();

			// Client-side operations
			/// @brief Reacts to STARTED message
			void started();
			/// @brief Reacts to SUSPENDED message
			void suspended(int progress);
			/// @brief Reacts to FINISHED message
			void finished();
			/// @brief Reacts to task abortion
			void aborted();
			/// @brief Reacts to PROGRESS response message
			void gotProgress(int progress);
			/// @brief Reacts to client disconnection
			void clientDisconnected();

			/// @brief Return list of task dependencies
			std::vector<CTaskWrapper*>& getDependencies();
			/// @brief Checks if task dependencies are fulfilled
			/// @return 1 if all dependencies are ready, 0 if there are unfulfilled dependencies and -1 if there are aborted dependencies
			int dependenciesReady();
			/// @brief Returns pointer to client object
			/// @return Client that is associated to this task
			CComSchedClient* getClient();

			/// @param pName
			/// @param size
			/// @param checkpoints
			/// @param pResources
			/// @param pDeplist
			/// @param depNum
			/// @param pClient
			/// @param rTaskDatabase
			CTaskWrapper(std::string* pName,
				long size,
				long checkpoints,
				std::vector<CResource*>* pResources,
				int* pDeplist,
				int depNum,
				CComSchedClient* pClient,
				CTaskDatabase& rTaskDatabase);

			~CTaskWrapper();

			/// @brief Print final task state to the event log
			void printEndTask();

	};

} }
#endif
