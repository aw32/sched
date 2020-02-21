// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#ifndef __CSIMMAIN_H__
#define __CSIMMAIN_H__
#include <vector>
#include <thread>
#include <pthread.h>

namespace sched {
namespace schedule {
	class CSchedule;
	class CScheduleExecutorMain;
	class CScheduleComputerMain;
	class CFeedback;
	class CResource;
	class CResourceLoader;
} }


namespace sched {
namespace measure {
	class CMeasure;
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
namespace sim {
	class CSimQueue;
} }

namespace sched {
namespace sim {

	using sched::schedule::CSchedule;
	using sched::schedule::CScheduleExecutorMain;
	using sched::schedule::CScheduleComputerMain;
	using sched::schedule::CFeedback;
	using sched::schedule::CResource;
	using sched::schedule::CResourceLoader;


	using sched::task::CTaskDatabase;

	using sched::sim::CSimQueue;

	/// @brief Application class for scheduler simulation program
	class CSimMain {

		private:
			CResourceLoader* resourceLoader;
			std::vector<CResource*> resources;
			CTaskDatabase* taskDatabase;
			CSimQueue* simulation;
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
			int loadSimulation();
			void unloadSimulation();

		public:
			/// @brief Wakes up the main thread to proceed with shutdown
			static void shutdown();
			/// @brief Main method for application flow
			int main();

	};

} }
#endif
