// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#ifndef __CCOMUNIXSCHEDCLIENTMAIN_H__
#define __CCOMUNIXSCHEDCLIENTMAIN_H__
#include "CComSchedClient.h"
#include "CComUnixSchedClient.h"

namespace sched {
namespace com {

	/// @brief Main implementation of communcating to sched client
	class CComUnixSchedClientMain : public CComUnixSchedClient {

		protected:
			std::vector<CTaskWrapper*> pTasks;
			CScheduleComputer& mrScheduleComputer;

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

			CComUnixSchedClientMain(
				CComUnixServer& rServer,
				std::vector<CResource*>& rResources,
				CTaskDatabase& rTaskDatabase,
				CScheduleComputer& rScheduleComputer,
				int socket
			);

			virtual ~CComUnixSchedClientMain();

	};

} }
#endif
