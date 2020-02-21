// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#ifndef __CTASKDATABASE_H__
#define __CTASKDATABASE_H__
#include <map>
#include <vector>
#include <mutex>
namespace sched {
namespace schedule {
	class CScheduleComputer;
} }


namespace sched {
namespace task {

	using sched::schedule::CScheduleComputer;

	class CTaskDatabase;
	class CTaskWrapper;
	class CTaskCopy;
	class CTaskLoader;

	/// @brief Global storage of task information
	/// All tasks are registered in the database and assigned an id.
	class CTaskDatabase {

		protected:
			CScheduleComputer* mpScheduleComputer;
			std::vector<CTaskWrapper*> mTasks;
			std::map<int,CTaskWrapper*> mTaskMap;
			int mTaskNum = 0;
			int mAppNum = 0;
			std::mutex mTaskMutex;
			CTaskLoader* mpTaskLoader = 0;

		public:
			// object registration
			void setScheduleComputer(CScheduleComputer* pScheduleComputer);

			/// @brief Register given task
			/// The given task is assigned an id and added to the internal task list
			/// @param task Task to register
			/// @return 0 if successful, else -1
			int registerTask(CTaskWrapper* task);

			/// @brief Register given tasks
			/// In contrast to registerTask() the tasks in the list can have dependencies.
			/// @param tasks List of tasks
			/// @return 0 if successful, else -1
			int registerTasklist(std::vector<CTaskWrapper*>* tasks);

			/// @brief Returns TaskWrapper objects by global id
			/// @return Pointer to TaskWrapper
			CTaskWrapper* getTaskWrapper(int taskId);

			/// @brief Abort task
			int abortTask(CTaskWrapper* task);

			CTaskDatabase();

			/// @brief Initialize task database
			/// @return 0 if successful, else -1
			int initialize();

			~CTaskDatabase();

			/// @brief Lets all tasks print final state information to the event log
			void printEndTasks();

			/// @brief Creates a list of all unfinished tasks
			/// @return List with TaskCopy objects, the caller has to delete the list
			std::vector<CTaskCopy>* copyUnfinishedTasks();

			/// @brief Checks if all tasks are finished
			/// @return True if all tasks are finished, else false
			bool tasksDone();

			/// @brief Returns number of registered applications
			/// @return count of registered applications
			int getApplicationCount();

	};

} }
#endif
