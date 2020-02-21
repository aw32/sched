// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#ifndef __CSIMQUEUE_H__
#define __CSIMQUEUE_H__
#include <vector>
#include <list>
#include <chrono>
#include <mutex>
#include <thread>
#include <condition_variable>
#include "CComSchedClient.h"
#include "CScheduleComputer.h"
#include "CScheduleExecutor.h"
#include "CScheduleExecutorMain.h"
#include "CLogger.h"
#include "CFeedback.h"
#include "ETaskOnEnd.h"

namespace sched {
namespace task {
	class CTaskWrapper;
	class CTaskDatabase;
} }


namespace sched {
namespace schedule {
	class CResource;
	class CScheduleComputerMain;
	class CSchedule;
} }

namespace sched {
namespace algorithm {
	class CEstimation;
} }

namespace sched {
namespace sim {

	using sched::task::ETaskOnEnd;
	using sched::task::CTaskWrapper;
	using sched::com::CComSchedClient;
	using sched::schedule::CResource;
	using sched::schedule::CScheduleComputer;
	using sched::schedule::CScheduleExecutor;
	using sched::schedule::CScheduleComputerMain;
	using sched::schedule::CScheduleExecutorMain;
	using sched::schedule::CSchedule;
	using sched::schedule::CFeedback;
	using sched::task::CTaskDatabase;
	using sched::algorithm::CEstimation;

	/// @brief Status of the simulated task
	enum ESimTaskStatus {
		SIM_TASK_STATUS_IDLE,
		SIM_TASK_STATUS_STARTING,
		SIM_TASK_STATUS_WORKING,
		SIM_TASK_STATUS_SUSPENDING,
		SIM_TASK_STATUS_SUSPENDED,
		SIM_TASK_STATUS_FINISHED
	};

	/// @brief Representing the task status in the simulation
	class CSimTaskState {

		public:
			CTaskWrapper* task;
			int current_checkpoint = 0;
			int target_checkpoint = 0;
			CResource* current_res = 0;
			enum ETaskOnEnd onend = ETaskOnEnd::TASK_ONEND_SUSPENDS;
			enum ESimTaskStatus status = ESimTaskStatus::SIM_TASK_STATUS_IDLE;
			std::chrono::time_point<std::chrono::steady_clock> start_time;
			CSimTaskState(CTaskWrapper* task): task(task){}
			~CSimTaskState(){}
			const static char* statusStrings[];
	};

	/// @brief Types of simulation events
	enum ESimEventType {
		SIMEVENT_NEWTASK,
		SIMEVENT_TASK_CHANGE,
		SIMEVENT_TIMER_END,
		SIMEVENT_ALGO_END
	};

	/// @brief Base class for events occuring during simulation
	class CSimEvent {

		public:
			ESimEventType type; ///< event type
			std::chrono::time_point<std::chrono::steady_clock> time; ///< event time

			CSimEvent(ESimEventType type):
				type(type)
				{}
			virtual ~CSimEvent(){}

			const static char* eventTypeStrings[]; ///< event type strings for pretty printing
	};

	/// @brief Event that occurs when tasks register at the scheduler
	class CSimTaskRegEvent : public CSimEvent {

		public:
			std::vector<CTaskWrapper*> tasks; ///< list of tasks to be registered

			CSimTaskRegEvent():
				CSimEvent(ESimEventType::SIMEVENT_NEWTASK)
				{}
			virtual ~CSimTaskRegEvent(){}
	};

	/// @brief Event that occurs when the task state changes
	class CSimTaskChangeEvent : public CSimEvent {

		public:
			CTaskWrapper* task; ///< task that changes state
			enum ESimTaskStatus newStatus; ///< new state
			enum ESimTaskStatus oldStatus; ///< old state
			int reached_checkpoint = 0; ///< reached checkpoint in case of suspension
			int start_checkpoint = 0; ///< checkpoint the current phase started with
			CSimTaskChangeEvent():
				CSimEvent(ESimEventType::SIMEVENT_TASK_CHANGE)
				{}
			virtual ~CSimTaskChangeEvent(){}
	};

	/// @brief Event that occurs when a timer goes off (unused)
	class CSimTimerEndEvent : public CSimEvent {

		public:
			CResource* resource; ///< resource the timer belongs to

			CSimTimerEndEvent():
				CSimEvent(ESimEventType::SIMEVENT_TIMER_END)
				{}
			virtual ~CSimTimerEndEvent(){}
	};

	/// @brief Event that occurs when an algorithm finishes
	class CSimAlgorithmEndEvent : public CSimEvent {

		public:
			CSchedule* schedule; ///< resulting schedule

			CSimAlgorithmEndEvent():
				CSimEvent(ESimEventType::SIMEVENT_ALGO_END)
				{}
			virtual ~CSimAlgorithmEndEvent(){}
	};

	/// @brief Central class for simulation management
	class CSimQueue : public CComSchedClient, CScheduleComputer, CScheduleExecutor, CFeedback {

		private:
			std::vector<CResource*>& mrResources;
			CTaskDatabase& mrTaskDatabase;
			CScheduleComputerMain* mpScheduleComputer;
			CScheduleExecutorMain* mpScheduleExecutor;

			std::list<CSimEvent*> mQueue;
			std::list<CSimEvent*> mPassedQueue;
			std::chrono::time_point<std::chrono::steady_clock> mCurrentTime;
			std::vector<CSimTaskState*> mTaskStates;

			CEstimation* mpEstimation;

			std::thread mSimulationThread;
			std::mutex mSimulationMutex;
			std::condition_variable mSimulationCondVar;
			int mStartSimulation = 0;
			int mStopSimulation = 0;


			// tasks and events read from file
			std::vector<CTaskWrapper*> mInputTasks;
			std::vector<CSimEvent*> mInputEvents;

			// algorithm computation lock
			std::mutex mAlgoComputeMutex;
			std::condition_variable mAlgoComputeCondVar;
			CSchedule* mpNewSchedule;
			bool mNewScheduleInterrupt;

		private:
			void runSimulation();
			void addEvent(CSimEvent* event);
			void removeTaskChangeEvent(CSimTaskChangeEvent* taskevent);
			CSimTaskChangeEvent* findTaskChangeEvent(CTaskWrapper* task);
			void executeEvent(CSimEvent* event);
			void computeNewSchedule();
			double timeToSec(std::chrono::steady_clock::time_point time);
			void getEventsByTime(double time, std::list<CSimQueue*>* events);
			int countEventsByType(std::list<CSimEvent*>* events, ESimEventType type);
			CSimEvent* mergeEventsByType(std::list<CSimEvent*>* events, ESimEventType type);

			void removeEventsByType(std::list<CSimEvent*>* events, ESimEventType type, bool deleteEvent);
			void removeEventsByTypeAndTime(std::list<CSimEvent*>* events, ESimEventType type, double time, bool deleteEvent);

			void mergeEvents(std::list<CSimEvent*>* events, bool deleteEvent);
			void getEventsByTime(std::list<CSimEvent*>* srcEvents, std::list<CSimEvent*>* events, double time);

			void getEventsByType(std::list<CSimEvent*>* srcEvents, std::list<CSimEvent*>* events, ESimEventType type);

		public:
			/// @brief Initialize the simulation components
			int init();
			/// @brief Load task registration events from event file
			int loadTaskEvents();
			/// @brief Start processing of events
			void startSimulation();
			/// @brief Stop simulation
			void stopSimulation();
			CSimQueue(std::vector<CResource*>& rResources,
				CTaskDatabase& rTaskDatabase
			);
			~CSimQueue();

		// CComClient
			/// @brief CComClient::start
			int start(CResource& resource, int targetProgress, ETaskOnEnd onEnd, CTaskWrapper& task);
			/// @brief CComClient::suspend
			int suspend(CTaskWrapper& task);
			/// @brief CComClient::abort
			int abort(CTaskWrapper& task);
			/// @brief CComClient::progress
			int progress(CTaskWrapper& task);
			/// @brief CComClient::taskids
			int taskids(std::vector<CTaskWrapper*>* tasks) { return 0;};
			/// @brief CComClient::closeClient
			void closeClient() { }

			/// @brief CComClient::onTasklist
			void onTasklist(std::vector<CTaskWrapper*>* list) {};
			/// @brief CComClient::onStarted
			void onStarted(int taskid) {};
			/// @brief CComClient::onSuspended
			void onSuspended(int taskid, int progress) {};
			/// @brief CComClient::onFinished
			void onFinished(int taskid) {};
			/// @brief CComClient::onProgress
			void onProgress(int taskid, int progress) {};
			/// @brief CComClient::onQuit
			void onQuit() {};



		// CScheduleComputer and CScheduleExecutor
			/// @brief CScheduleComputer/CScheduleExecutor::start
			int start() { return 0;}
			/// @brief CScheduleComputer/CScheduleExecutor::stop
			void stop() { }
		// CScheduleComputer
			/// @brief CScheduleComputer::executorSuspended
			void executorSuspended();
			/// @brief CScheduleComputer::loadAlgorithm
			int loadAlgorithm() { return 0;}
			/// @brief CScheduleComputer::setScheduleExecutor
			void setScheduleExecutor(CScheduleExecutor* pScheduleExecutor) { }
			/// @brief CScheduleComputer::computeSchedule
			void computeSchedule();
			/// @brief CScheduleComputer::getRequiredApplicationCount
			int getRequiredApplicationCount();
			/// @brief CScheduleComputer::getRegisteredApplications
			int getRegisteredApplications();

		// CScheduleExecutor
			/// @brief CScheduleExecutor::suspendSchedule
			void suspendSchedule();
			/// @brief CScheduleExecutor::updateSchedule
			void updateSchedule(CSchedule* pSchedule);
			/// @brief CScheduleExecutor::setScheduleComputer
			void setScheduleComputer(CScheduleComputer* pScheduleComputer) {	}
			/// @brief CScheduleExecutor::operationDone
			void operationDone() { }
			/// @brief CScheduleExecutor::getCurrentLoop
			int getCurrentLoop() {return 0;}
			/// @brief CScheduleExecutor::getNextLoop
			int getNextLoop(int current) {return 0;}
			/// @brief CScheduleExecutor::getCurrentSchedule
			CSchedule* getCurrentSchedule() {
				return mpScheduleExecutor->getCurrentSchedule();
			}

		// CFeedback
			/// @brief CFeedback::getProgress
			int getProgress();
			/// @brief CFeedback::gotProgress
			void gotProgress(CResource& res) { };

	};

} }
#endif
