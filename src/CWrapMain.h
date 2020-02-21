// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#ifndef __CWRAPMAIN_H__
#define __CWRAPMAIN_H__
#include <vector>
#include <pthread.h>
#include "CTaskDefinitions.h"

namespace sched {
namespace schedule {
	class CResource;
	class CResourceLoader;
} }


namespace sched {
namespace task {
	class CTaskDatabase;
} }


namespace sched {
namespace com {
	class CComUnixServer;
	
} }

namespace sched {
namespace wrap {

	using sched::schedule::CResource;
	using sched::schedule::CResourceLoader;


	using sched::task::CTaskDatabase;

	using sched::com::CComUnixServer;

	class CUnixWrapClient;
	class CWrapTaskGroup;

	/// @brief Application class for wrap program
	class CWrapMain {

		private:
			CResourceLoader* resourceLoader;
			std::vector<CResource*> resources;
			CTaskDatabase* taskDatabase;
			CComUnixServer* server;
			CTaskDefinitions* taskDefinitions;
			CUnixWrapClient* wrap;
			CWrapTaskGroup* group;

			static pid_t mPid;
			static pthread_t mPthread;

		private:
			static void serveSigaction(int sig);
			void waitForSignal();
			int loadConfiguration();
			void unloadConfiguration();
			int loadTaskDatabase();
			void unloadTaskDatabase();
			int loadResourceLoader();
			void unloadResourceLoader();
			int loadResources();
			void unloadResources();
			int loadWrap();
			void unloadWrap();
			int loadComUnixServer();
			void unloadComUnixServer();

			int loadTaskDefinitions();
			void unloadTaskDefinitions();

			int loadGroup();
			void unloadGroup();

		public:
			/// @brief Wakes up the main thread to proceed with shutdown
			static void shutdown();
			/// @brief Main method for application flow
			int main();

	};

} }
#endif
