// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#ifndef __CWRAPTASKGROUPS_H__
#define __CWRAPTASKGROUPS_H__
#include <vector>
#include "cjson/cJSON.h"
namespace sched {
namespace task {
	class CTaskWrapper;
} }

namespace sched {
namespace wrap {

	using sched::task::CTaskWrapper;

	class CTaskDefinitions;

	/// @brief Represents the application containing a list of tasks
	///
	/// The task group consists of a list of applications each registering a list of tasks when executed.
	/// This class represents such an application.
	/// The application can be executed and stopped.
	class CWrapTaskApp {

		CTaskDefinitions& mrTaskDefinitions;
		bool started = false;
		bool stopped = false;
		bool joined = false;
		pid_t pid = 0;

		int status = -0x1337;
		int signaled = -0x1337;
		int signaled_signal = -0x1337;

		public:
			char* name; ///< Application name as defined in the task definition file
			int task_num; ///< Number of tasks
			int* tasks; ///< List of task ids, last entry is -1
			int** dependencies; ///< List of dependencies, contains one list per task
			bool exec; ///< Execute this app
			int size; ///< Tasksize parameter as passed to application
			std::vector<CTaskWrapper*> globalTasks; ///< List of registered tasks for this application
		
		public:
			/// @param defs
			/// @param name
			/// @param task_num
			/// @param tasks
			/// @param dependencies
			/// @param exec
			/// @param size
			CWrapTaskApp(
				CTaskDefinitions& defs,
				char* name,
				int task_num,
				int* tasks,
				int** dependencies,
				bool exec,
				int size);
			~CWrapTaskApp();

		public:
			/// @brief Checks if this application contains the task with the given id
			/// @param id The task id as specified in the group definition
			/// @return true if the task was found, else false
			bool containsTask(int id);

			/// @brief Return TaskWrapper object for task with given id
			/// @param id The task id as specified in the group definition
			/// @return The pointer to the TaskWrapper object if found, else 0
			CTaskWrapper* getTaskWithId(int id);

			/// @brief Checks if all tasks finished
			/// @return True if all tasks finished, else false
			bool allFinished();

			/// @brief Start the application and pass the given socket path as scheduler socket path (SCHED_SOCKET)
			/// @param number Number of started app
			/// @param logprefix Path and prefix for log files
			/// @param wrapsocketpath Path to scheduler socket
			/// @return 0 if successfull, else -1
			int start(int number, char* logprefix, char* wrapsocketpath);

			/// @brief Join the process if it already exited
			/// @return 0 if process is dead, else -1 
			int joinIfDead();

			/// @brief Stop the process
			void stop();

			/// @brief Join the process
			void join();

	};

	/// @brief Loads task group definition
	///
	/// The task group definition is specified in a JSON file and describes what applications should be combined.
	class CWrapTaskGroup {

		public:
			std::vector<CWrapTaskApp*> mTaskApps; ///< List of loaded application definitions
			cJSON* mpJson; ///< JSON structure

		public:
			~CWrapTaskGroup();
			/// @brief Load group definitions
			/// @param groupFile Path to group definition file
			/// @param defs List of task definitions, the definitions are later on necessary to start the applications
			/// @return Task group object
			static CWrapTaskGroup* loadTaskGroups(char* groupFile, CTaskDefinitions& defs);

			/// @brief Load task application definition
			/// @param json JSON structure
			/// @param defs Task definitions
			/// @return Task application object
			static CWrapTaskApp* loadTaskApp(cJSON* json, CTaskDefinitions& defs);

	};
} }
#endif
