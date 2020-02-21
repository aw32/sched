// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#ifndef __CMAIN_H__
#define __CMAIN_H__
#include <vector>
#include <pthread.h>

namespace sched {
namespace schedule {
	class CSchedule;
	class CScheduleExecutorMain;
	class CScheduleComputerMain;
	class CFeedbackMain;
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
	class CComUnixSchedServerMain;
} }


namespace sched {

	using sched::schedule::CSchedule;
	using sched::schedule::CScheduleExecutorMain;
	using sched::schedule::CScheduleComputerMain;
	using sched::schedule::CFeedbackMain;
	using sched::schedule::CResource;
	using sched::schedule::CResourceLoader;

	using sched::measure::CMeasure;

	using sched::task::CTaskDatabase;

	using sched::com::CComUnixSchedServerMain;

	/// @brief Application class for scheduler program
	class CMain {

		private:
			CResourceLoader* resourceLoader;
			std::vector<CResource*> resources;
			CFeedbackMain* feedback;
			CScheduleExecutorMain* scheduleExecutor;
			CScheduleComputerMain* scheduleComputer;
			CTaskDatabase* taskDatabase;
			CMeasure* measure;
			CComUnixSchedServerMain* server;
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
			int loadFeedback();
			void unloadFeedback();
			int loadScheduleComputer();
			void unloadScheduleComputer();
			int loadScheduleExecutor();
			void unloadScheduleExecutor();
			int loadMeasurement();
			void unloadMeasurement();
			int loadComUnixServer();
			void unloadComUnixServer();

		public:
			/// @brief Wakes up the main thread to proceed with shutdown
			static void shutdown();
			/// @brief Main method for application flow
			int main();
			CMeasure* getMeasure();

	};

}
#endif
