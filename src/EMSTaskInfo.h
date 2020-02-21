// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#ifndef __EMSTaskInfo_H__
#define __EMSTaskInfo_H__
namespace sched {
namespace task {

	/// @brief Fields of the egysched measurement results
	///
	/// Egysched tasks can measure their time and energy consumption.
	/// The results are stored as CSV file.
	/// This enumeration contains the fields stored in the CSV file.
	///
	/// There are two different types of measurement: energy and time.
	///
	/// For the energy measurement the task is executed multiple times until the minimal measurement time is reached.
	/// Therefore, at least one task execution is done.
	/// ETotal and TTotal are total energy and time for the full measurement run.
	/// ETask is the dynamic energy (measured energy - idle energy) for one task iteration.
	/// TTask is the time for one task iteration.
	/// For energy measurement only ETotal, TTotal, ETask and TTask are stored.
	///
	/// For the time measurement the task is executed once.
	/// TTask is the total task execution time.
	/// TInit is the duration of the init phase.
	/// TComp is the duration of the whole compute phase comprising all checkpoints.
	/// TFini is the duration of the fini phase.
	/// TTask might be larger than the sum of TInit, TComp and TFini.
	/// For time measurement only TTask, TInit, TComp and TFini are stored.
	enum EMSTaskInfo {
		ETotal, ///< Total energy for measurement run
		TTotal, ///< Total time for measurement run
		ETask, ///< Dynamic energy for one task iteration
		TTask, ///< Time for one task iteration
		TInit, ///< Time for task's init phase
		TComp, ///< Time for task's compute phase (all checkpoints)
		TFini ///< Time for task's fini phase
	};

} }
#endif
