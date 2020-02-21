// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#ifndef __CCOMUNIXSCHEDCLIENTWRAP_H__
#define __CCOMUNIXSCHEDCLIENTWRAP_H__
#include "CComUnixSchedClient.h"
//#include "CTaskDatabase.h"
//#include "CUnixWrapClient.h"

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

	using sched::com::CComUnixSchedClient;
	using sched::schedule::CResource;
	using sched::task::CTaskWrapper;
	using sched::task::CTaskDatabase;
	using sched::com::CComUnixServer;

	class CUnixWrapClient;

	/// @brief Wrap implementation for communicating to clients
	class CComUnixSchedClientWrap : public CComUnixSchedClient {

		CUnixWrapClient& mrWrap;

		public:
			int start(CResource& resource, int targetProgress, ETaskOnEnd onEnd, CTaskWrapper& task);
			int suspend(CTaskWrapper& task);
			int abort(CTaskWrapper& task);
			int progress(CTaskWrapper& task);
			int taskids(std::vector<CTaskWrapper*>* tasks);
			void closeClient();

			void onTasklist(std::vector<CTaskWrapper*>* list);
			void onStarted(int taskid);
			void onSuspended(int taskid, int progress);
			void onFinished(int taskid);
			void onProgress(int taskid, int progress);
			void onQuit();

			CComUnixSchedClientWrap(
				CComUnixServer& rServer,
				std::vector<CResource*>& rResources,
				CTaskDatabase& rTaskDatabase,
				CUnixWrapClient& rWrap,
				int socket
			);

			~CComUnixSchedClientWrap();

	};

} }
#endif
