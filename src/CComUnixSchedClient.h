// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#ifndef __CCOMUNIXSCHEDCLIENT_H__
#define __CCOMUNIXSCHEDCLIENT_H__
#include <mutex>
#include <vector>
#include "cjson/cJSON.h"
#include "CComClient.h"
#include "CComSchedClient.h"
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


	/// @brief Communication to sched client over unix sockets
	class CComUnixSchedClient : 
		public CComUnixClient,
		public CComUnixWriter,
		public CComSchedClient
	{

		public:
			enum EComSchedMessageType {
				START,
				SUSPEND,
				ABORT,
				PROGRESS,
				TASKIDS,
				QUIT
			};

			class CComSchedMessage : public CComUnixWriteMessage {
				public:
					EComSchedMessageType type;
					CTaskWrapper* task;
					CResource* resource;
					int targetProgress;
					ETaskOnEnd onEnd;
					int progress;
					int* taskids = 0;

					virtual ~CComSchedMessage() {
						if (taskids != 0) {
							delete[] taskids;
							taskids = 0;
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
			int initClient();
			int readVer0();
			int readVer1();
			void processVer1(cJSON* json);
			void incBuffer();
			int write1(cJSON* json, bool quit);

		public:
			void writeStart(CResource& resource, int targetProgress, ETaskOnEnd onEnd, CTaskWrapper& task);
			void writeSuspend(CTaskWrapper& task);
			void writeAbort(CTaskWrapper& task);
			void writeProgress(CTaskWrapper& task);
			void writeTaskids(int* taskid_list);
			void writeQuit();

		// inherited by CComUnixClient
			int read();

		// inherited by CComUnixWriter
			void writeMessage(CComUnixWriteMessage* message);

			CComUnixSchedClient(
					CComUnixServer& rServer,
					std::vector<CResource*>& rResources,
					CTaskDatabase& rTaskDatabase,
					int socket);
			virtual ~CComUnixSchedClient();

	};

} }
#endif
