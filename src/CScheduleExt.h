// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#ifndef __CSCHEDULEEXT_H__
#define __CSCHEDULEEXT_H__
#include <unordered_map>
#include "CSchedule.h"

namespace sched {
namespace schedule {

	/// @brief Extended schedule class
	/// The extended schedule class contains additional structures to track task dependencies, task parts and task ready times.
	class CScheduleExt : public CSchedule {

		public:
			std::unordered_map<int,int> mTaskMap; ///< global to local index map
			std::unordered_map<int,bool> mTaskExist; ///< does the task exist in the schedule
			std::unordered_map<int,bool> mTaskLastPart; ///< is the task's last part in the schedule ?
			std::unordered_map<int,bool> mTaskDep; ///< are the task dependencies fullfilled?
			std::unordered_map<int,double> mTaskReady; ///< max finish time of task's all predecessor tasks
			std::unordered_multimap<int,STaskEntry*> mTaskParts; ///< task's entries in the schedule

		public:
			/// @param tasks
			/// @param resources
			/// @param tasklist
			CScheduleExt(int tasks, int resources, std::vector<CResource*>& rResources, std::vector<CTaskCopy>* tasklist);

			virtual ~CScheduleExt();

			/// @brief Returns number of queued entries for the given resource
			/// @param mix
			/// @return Number of queued entries
			int resourceTasks(int mix);

			/// @brief Computes the ready task ready.
			/// The method incorporates predecessors, resource ready time and if the task is already running.
			/// Note: when using the result one has to keep in mind if the task is already running on the target resource using the taskRunningResource() method. This may influence the execution time of the task.
			/// @param tix Task index
			/// @param res Resource
			/// @param est Estimation object
			/// @return Ready time
			double taskReadyTimeResource(int tix, CResource* res, CEstimation* est);

			/// @brief Computes task execution time for a given slot on a given resource.
			/// Checks if the task is already running on the given resource.
			/// If the task is running on the resource and mapped to slot 0, then the init time can be omitted.
			/// Else the task execution time includes the init time.
			/// Finish time if task continues running on task: ready + compute + fini
			/// Finish time for other cases: ready + init + compute + fini
			/// Note: ready time has to incorporate the discussed difference. See taskReadyTimeResource().
			/// @param tix Task index
			/// @param res Target resource
			/// @param slot Target slot index
			/// @param ready Task ready time
			/// @param init Task init time
			/// @param compute Task compute time
			/// @param fini Task fini time
			double taskCompletionTime(int tix, CResource* res, int slot, double ready, double init, double compute, double fini);

			/// @brief Add entry to schedule and update data structures
			/// @param entry New entry
			/// @param res Target resource
			/// @param position Target slot index
			void addEntry(STaskEntry* entry, CResource* res, int position);

			/// @brief Returns finish time of last resource entry
			/// @param res Resource
			/// @return Finish time of last entry
			double resourceReadyTime(CResource* res);
			/// @brief Returns finish time of last resource entry
			/// @param mix Resource id
			/// @return Finish time of last entry
			double resourceReadyTime(int mix);

			/// @brief Returns task ready time.
			/// Note: This method does not incorporate if the task is already running on a resource.
			/// @param task Task
			/// @return Task ready time
			double taskReadyTime(CTask* task);
			/// @brief Returns task ready time.
			/// Note: This method does not incorporate if the task is already running on a resource.
			/// @param tix Task index
			/// @return Task ready time
			double taskReadyTime(int tix);

			/// @brief Returns if the task's finishing part is covered by the schedule
			/// @param tix Task index
			/// @return True if the task's finishing part is inside the schedule, else false
			bool taskLastPartMapped(int tix);
			/// @brief Returns if the task's finishing part is covered by the schedule
			/// @param task task
			/// @return True if the task's finishing part is inside the schedule, else false
			bool taskLastPartMapped(CTask* task);

			/// @brief Checks if the task's predecessors are completely covered by the schedule
			/// @param task Task
			/// @return True if the task is ready to be scheduled, else false
			bool taskDependencySatisfied(CTask* task);
			/// @brief Checks if the task's predecessors are completely covered by the schedule
			/// @param tix Task index
			/// @return True if the task is ready to be scheduled, else false
			bool taskDependencySatisfied(int tix);

			/// @brief Copy entries from old schedule
			/// @param old Old schedule
			/// @param updated If 1 then the task progress was updated, else the current progress will be estimated
			void copyEntries(CSchedule* old, int updated);

			/// @brief Find the next slot that can fit the task
			/// This method iterates over the queue of the given resource and checks if the break inbetween entries may fit the given task duration.
			/// @param mix Resource id
			/// @param dur Duration of the new entry
			/// @param start Ready time of the new entry, slot can't start before this time
			/// @param startSlot First slot to check
			/// @param[out] slotStartOut Computed start time of the found slot
			/// @param[out] slotStopOut Computed finish time of the found slot
			/// @return Slot index, 0 is the slot before the first entry, Length of resource queue is the slot after all entries
			int findSlot(int mix, double dur, double start, int startSlot, double* slotStartOut, double* slotStopOut);

	};

} }


#endif
