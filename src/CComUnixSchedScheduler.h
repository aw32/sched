// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#ifndef __CCOMUNIXSCHEDSCHEDULER_H__
#define __CCOMUNIXSCHEDSCHEDULER_H__
#include <mutex>
#include <vector>
#include "cjson/cJSON.h"
#include "CComClient.h"
#include "CComSchedScheduler.h"
#include "CComUnixClient.h"
#include "CComUnixWriteBuffer.h"
#include "ETaskOnEnd.h"

// Maximal size of read buffer
#define __CCOMUNIXCLIENT_MAX_BUFFER 1024

using sched::task::ETaskOnEnd;

namespace sched {
namespace schedule {
	class CResource;
	class CScheduleComputer;
} }


namespace sched {
namespace task {
	class CTaskWrapper;
	class CTaskDatabase;
} }


namespace sched {
namespace com {

	using sched::schedule::CResource;
	using sched::schedule::CScheduleComputer;

	using sched::task::CTaskWrapper;
	using sched::task::CTaskDatabase;

	class CComUnixServer;


	/// @brief Communication to sched scheduler over unix sockets
	class CComUnixSchedScheduler : 
		public CComUnixClient,
		public CComUnixWriter,
		public CComSchedScheduler
	{

		public: 
			enum EComSchedMessageType {
				STARTED,
				SUSPENDED,
				PROGRESS,
				TASKLIST,
				FINISHED,
				QUIT
			};

			class CComSchedMessage : public CComUnixWriteMessage {
				public:
					EComSchedMessageType type;
					int taskid;
					int progress;
					std::vector<CTaskWrapper*>* tasks;

					virtual ~CComSchedMessage() {
						if (tasks != 0) {
							delete tasks;
							tasks = 0;
						}
					};
			};


		private:
			int const MAX_BUFFER = __CCOMUNIXCLIENT_MAX_BUFFER;

		protected:
			CComUnixServer& mrServer;
			char* mBuffer = 0;
			int mBufferPos = 0;
			int mBufferMax = 1024;
			long mProtocol = -1;
			std::mutex mWriteMutex;
			int mClientQuit = 0;
			int mClientClosed = 0;
			std::vector<CResource*>& mrResources;
			CTaskDatabase& mrTaskDatabase;

		private:
			int readVer0();
			int readVer1();
			void processVer1(cJSON* json);
			void incBuffer();
			int write1(cJSON* json);
			int write1(char* str);

		public:
			int initClient();

			void writeStarted(int taskid);
			void writeSuspended(int taskid, int progress);
			void writeFinished(int taskid);
			void writeProgress(int taskid, int progress);
			void writeTasklist(std::vector<CTaskWrapper*>* tasks);
			void writeQuit();


		// inherited by CComUnixClient
			int read();

		// inherited by CComUnixWriter
			void writeMessage(CComUnixWriteMessage* message);

			CComUnixSchedScheduler(
					CComUnixServer& rServer,
					std::vector<CResource*>& rResources,
					CTaskDatabase& rTaskDatabase,
					int socket);
			~CComUnixSchedScheduler();

	};

} }
#endif
