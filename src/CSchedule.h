// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#ifndef __CSCHEDULE_H__
#define __CSCHEDULE_H__
#include <vector>
#include <chrono>
#include <ostream>

namespace sched {
namespace algorithm {
	class CEstimation;
} }

namespace sched {
namespace task {
	class CTask;
	class CTaskCopy;
} }


namespace sched {
namespace schedule {

	using sched::task::CTask;
	using sched::task::CTaskCopy;
	using sched::algorithm::CEstimation;

	class CResource;

	/// @brief State of schedule TaskEntry
	enum ETaskEntryState {
		TODO, ///< The schedule entry is unfinished
		DONE, ///< The schedule entry is done
		ABORTED ///< The schedule entry was aborted
	};

	/// @brief Schedule entry
	struct STaskEntry {
		int taskid; ///< task id
		CTaskCopy* taskcopy; ///< Pointer to TaskCopy, the copy was created before creation of the schedule
		ETaskEntryState state; ///< Schedule entry state
		unsigned int startProgress; ///< Planned task progress at the beginning of the entry
		unsigned int stopProgress; ///< Planned task progress at the end of the entry
		// number in the order of migration
		// 0 for the first part
		int partNumber = 0; ///< Number of this entry for the associated task in this schedule
		// time until next task
		std::chrono::steady_clock::duration durBreak; ///< Planned time until next entry for the given resource
		// estimated ready time (relative to schedule)
		std::chrono::steady_clock::duration timeReady; ///< Planned start time relative to schedule start
		// estimated finish time (relative to schedule)
		std::chrono::steady_clock::duration timeFinish; ///< Planned finish time relative to schedule start
		// execution durations
		std::chrono::steady_clock::duration durInit; ///< Expected init phase duration
		std::chrono::steady_clock::duration durCompute; ///< Expected compute phase duration
		std::chrono::steady_clock::duration durFini; ///< Expected fini phase duration
		std::chrono::steady_clock::duration durTotal; ///< Expected total duration
		// task energy
		double energy; ///< Expected energy consumption (dynamic energy)
	};

	/// @brief Base schedule class
	/// The basic schedule structure used for scheduling consists of one entry queue per resource.
	/// The queued entries contain information about tasks, target checkpoints and time estimations.
	class CSchedule {

		public:
			CEstimation* mpEst;
			std::vector<CResource*>& mrResources;
			std::vector<CTaskCopy>* mpOTasks = 0;
			std::vector<std::vector<STaskEntry*>*>* mpTasks = 0;
			std::vector<CTaskCopy*> mRunningTasks; ///< previously assigned tasks
			int mTaskNum = 0;
			int mResourceNum = 0;
			int mId = -1;
			int mActiveTasks = 0;
			// schedule computation by algorithm times
			std::chrono::steady_clock::time_point mComputeStart;
			std::chrono::steady_clock::time_point mComputeStop;
			std::chrono::steady_clock::duration mComputeDuration;
			// schedule duration (makespan)
			std::chrono::steady_clock::duration mDuration;
			// schedule
			double mStaticEnergy;
			double mDynamicEnergy;
			double mTotalEnergy;

		public:
			/// @param tasks
			/// @param resources
			/// @param rResources
			CSchedule(int tasks, int resources, std::vector<CResource*>& rResources);
			virtual ~CSchedule();

			/// @brief Returns the resource the task is running on, if at all
			/// Before starting schedule creation it is checked if tasks are running on the resources.
			/// The list of running tasks is passed to the scheduling algorithm.
			/// The running tasks are also part of the task list passed to the scheduling algorithm.
			/// This method checks if the task with the given task index is listed in the list of running tasks and returns the resource id.
			/// @param tix Task index in the task list
			/// @return If the task is already running the resource id, else -1
			int taskRunningResource(int tix);

			// @brief Same as taskRunningResource() but with task id instead of task index
			// @param tid Task id
			int taskRunningResourceTid(int tid);

			/// @brief Set list of already running tasks
			/// @param runningTasks
			void setRunningTasks(std::vector<CTaskCopy*>* runningTasks);

			/// @brief Update timing information in STaskEntry structs
			void computeTimes();

			/// @brief Update execution times in given task entry
			/// @param entry TaskEntry to update
			/// @param res Resource the task is mapped to
			/// @param slot Slot index for task
			void computeExecutionTime(STaskEntry* entry, CResource* res, int slot);

			/// @brief Update execution times in given task entry without known slot
			/// In this case running tasks are ignored.
			/// @param entry TaskEntry to update
			/// @param res Resource the task is mapped to
			void computeExecutionTime(STaskEntry* entry, CResource* res);



			/// @brief Update task progress information
			/// The entry's startProgress should be consistent with the current task progress.
			/// If the task progress was updated using the PROGRESS request, then this value can be used.
			/// Else the current progress is estimated using the elapsed time since start of the task.
			/// @param entry Entry to update
			/// @param task Original task object
			/// @param res Resource
			/// @param currentTime Current time
			/// @param updated If 1 then the current progress from the task object is adopted, else the current progress is estimated using the elapsed time
			void updateEntryProgress(STaskEntry* entry, CTask* task, CResource* res, std::chrono::steady_clock::time_point currentTime, int updated);

			/// @brief Print schedule as JSON to given stream
			/// @param out Stream to print to
			void printJson(std::ostream& out);

			/// @brief Find next entry to execute on given resource
			/// The next valid entry for execution on the given resource is searched for.
			/// The method iterates over the resource entries and skips finished and aborted entries.
			/// Additional entries can be skipped to get subsequent entries.
			/// @param res Resource
			/// @param skip Number of entries to skip
			/// @return Next entry to execute
			STaskEntry* getNextTaskEntry(CResource* res, int skip);

	};

} }
#endif
