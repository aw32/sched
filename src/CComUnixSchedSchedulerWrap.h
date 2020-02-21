// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#ifndef __CCOMUNIXSCHEDSCHEDULERWRAP_H__
#define __CCOMUNIXSCHEDSCHEDULERWRAP_H__
#include "CComUnixSchedScheduler.h"
#include "CTaskDatabase.h"
namespace sched {
namespace task {
	class CTaskWrapper;
	class CTaskDatabase;
} }

namespace sched {
namespace schedule {
	class CResource;
} }

namespace sched {
namespace wrap {

	using sched::task::CTaskWrapper;
	using sched::task::CTaskDatabase;
	using sched::schedule::CResource;
	using sched::com::CComUnixSchedScheduler;
	using sched::com::CComUnixServer;

	class CUnixWrapClient;

	/// @brief Wrap implementation for communicating to sched scheduler
	class CComUnixSchedSchedulerWrap : public CComUnixSchedScheduler {

		CUnixWrapClient& mrWrap;

		public:
			// messages sent from client to scheduler
			int started(int taskid);
			int suspended(int taskid, int progress);
			int finished(int taskid);
			int progress(int taskid, int progress);
			int tasklist(std::vector<CTaskWrapper*>* list);
			int closeClient();

			// messages received from scheduler
			void onStart(int taskid, CResource& resource, int targetProgress, ETaskOnEnd onEnd);
			void onSuspend(int taskid);
			void onAbort(int taskid) ;
			void onProgress(int taskid);
			void onTaskids(int* taskids);
			void onQuit();
			void onFail();


			CComUnixSchedSchedulerWrap(
				CComUnixServer& rServer,
				std::vector<CResource*>& rResources,
				CTaskDatabase& rTaskDatabase,
				CUnixWrapClient& rWrap,
				int socket
			);

			~CComUnixSchedSchedulerWrap();

	};

} }
#endif
