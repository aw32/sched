// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#ifndef __CESTIMATION_H__
#define __CESTIMATION_H__
namespace sched {
namespace task {
	class CTask;
} }


namespace sched {
namespace schedule {
	class CResource;
} }


namespace sched {
namespace algorithm {

	using sched::task::CTask;

	using sched::schedule::CResource;

	/// @brief Estimates time and energy consumption of a task execution
	///
	/// To control task execution the scheduler has to estimate a task's resource consumption.
	/// This interface declares the generic questions for this estimation.
	class CEstimation {

		public:
			/// @brief Estimation of the task's init phase duration
			/// @param task The given task
			/// @param res The resource the task will run on
			virtual double taskTimeInit(CTask* task, CResource* res) = 0;

			/// @brief Estimation of duration when executing given checkpoints of the task
			/// @param task The given task
			/// @param res The resource the task will run on
			/// @param startCheckpoint Start checkpoint for the execution
			/// @param stopCheckpoint Final checkpoint for the execution
			virtual double taskTimeCompute(CTask* task, CResource* res, int startCheckpoint, int stopCheckpoint) = 0;

			/// @brief Estimation of the task's fini phase duration
			/// @param task The given task
			/// @param res The resource the task will run on
			virtual double taskTimeFini(CTask* task, CResource* res) = 0;

			/// @brief Estimation of final checkpoint when executing task for a given duration
			/// @param task The given task
			/// @param res The resource the task will run on
			/// @param startCheckpoint Start checkpoint for the execution
			/// @param sec Maximal duration
			virtual int taskTimeComputeCheckpoint(CTask* task, CResource* res, int startCheckpoint, double sec) = 0;



			/// @brief Estimation of the task's init phase energy consumption
			/// @param task The given task
			/// @param res The resource the task will run on
			virtual double taskEnergyInit(CTask* task, CResource* res) = 0;

			/// @brief Estimation of energy consumption when executing given checkpoints of the task
			/// @param task The given task
			/// @param res The resource the task will run on
			/// @param startCheckpoint Start checkpoint for the execution
			/// @param stopCheckpoint Final checkpoint for the execution
			virtual double taskEnergyCompute(CTask* task, CResource* res, int startCheckpoint, int stopCheckpoint) = 0;

			/// @brief Estimation of the task's fini phase energy consumption
			/// @param task The given task
			/// @param res The resource the task will run on
			virtual double taskEnergyFini(CTask* task, CResource* res) = 0;

			/// @brief Estimation of final checkpoint when executing task for a given energy consumption
			/// @param task The given task
			/// @param res The resource the task will run on
			/// @param startCheckpoint Start checkpoint for the execution
			/// @param energy Maximal duration
			virtual int taskEnergyComputeCheckpoint(CTask* task, CResource* res, int startCheckpoint, double energy) = 0;

			/// @brief Estimation of resource idle energy consumption
			/// @param res The given resource
			/// @param seconds Duration
			virtual double resourceIdleEnergy(CResource* res, double seconds) = 0;

			/// @brief Estimation of resource idle power consumption per second
			/// @param res The given resource
			virtual double resourceIdlePower(CResource* res) = 0;

			virtual ~CEstimation();

			/// @brief Returns configured estimation
			static CEstimation* getEstimation();

	};

} }
#endif
