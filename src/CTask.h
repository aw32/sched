// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#ifndef __CTASK_H__
#define __CTASK_H__
#include <algorithm>
#include <map>
#include <vector>
#include <chrono>
#include <string>
namespace sched {
namespace schedule {
	class CResource;
} }


namespace sched {
namespace task {

	using sched::schedule::CResource;

	/// @brief Current task state
	enum ETaskState {
		PRE, ///< Task was never started
		STARTING, ///< Task is in init phase
		RUNNING, ///< Task is in comute phse
		STOPPING, ///< Task is in fini phase
		SUSPENDED, ///< Task is paused
		POST, ///< Task finished the computation
		ABORTED ///< Task was aborted
	};

	/// @brief Set of times
	struct STaskTimes {
		std::chrono::steady_clock::time_point Added; ///< Time the task was added
		std::chrono::steady_clock::time_point Started; ///< Time the task was started the last time
		std::chrono::steady_clock::time_point Finished; ///< Time the task finished
		std::chrono::steady_clock::time_point Aborted; ///< Time the task aborted
	};


	/// @brief Basic task state information
	class CTask {

		public:
			int mId = -1; ///< Id of the task as it was assigned by the TaskDatabase
			std::string* mpName = 0; ///< Name of the task
			long mSize = 0; ///< Size parameter
			long mCheckpoints = 0; ///< Total number of checkpoints
			long mProgress = 0; ///< Number of finished checkpoints
			ETaskState mState = ETaskState::PRE; ///< Current task state
			struct STaskTimes mTimes = {}; ///< Set of task timestamps
			CResource* mpResource = 0; ///< Pointer to lastly assigned resource
			/// @brief Map of task attributes
			/// Attributes are usually assigned by the TaskLoader
			/// The TaskLoader also keeps the data, while the map only contains pointers to the data.
			/// The TaskLoader frees the data on shutdown, afterwards no access should occur.
			std::map<std::string, void*>* mpAttributes = 0;
			std::vector<CResource*>* mpResources = 0; ///< List of compatible resources
			int* mpSuccessorList = 0; ///< List of successor ids
			int mSuccessorNum = 0; ///< Number of successors
			int* mpPredecessorList = 0; ///< List of predecessor ids
			int mPredecessorNum = 0; ///< Number of predecessors

		public:
			CTask();
			/// @brief Copy constructor
			/// @param copy Old task object
			CTask(const CTask& copy);
			/// @brief Assignment constructor
			/// @param other Old task object
			CTask& operator=(const CTask& other);
			/// @param pName
			/// @param size
			/// @param checkpoints
			/// @param pResources
			/// @param pDeplist
			/// @param depNum
			CTask(std::string* pName,
				long size,
				long checkpoints,
				std::vector<CResource*>* pResources,
				int* pDeplist,
				int depNum);

			/// @brief Check if the task is compatible to the given resource
			/// @param res Resource
			/// @return True if the task can be executed on the given resource, else false
			bool validResource(CResource* res);

			~CTask();

	};

} }
#endif
