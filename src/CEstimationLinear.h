// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#ifndef __CESTIMATIONLINEAR_H__
#define __CESTIMATIONLINEAR_H__
#include "CEstimation.h"
namespace sched {
namespace algorithm {

	/// @brief Estimation class using linear interpolation
	///
	/// This class uses egysched measurements to estimate the runtime or energy consumption of given tasks.
	/// For partial execution of tasks linear interpolation is used to calculate the fraction of time or energy.
	class CEstimationLinear : public CEstimation {

		public:
			~CEstimationLinear();

			double taskTimeInit(CTask* task, CResource* res);
			double taskTimeCompute(CTask* task, CResource* res, int startCheckpoint, int stopCheckpoint);
			double taskTimeFini(CTask* task, CResource* res);
			int taskTimeComputeCheckpoint(CTask* task, CResource* res, int startCheckpoint, double sec);

			double taskEnergyInit(CTask* task, CResource* res);
			double taskEnergyCompute(CTask* task, CResource* res, int startCheckpoint, int stopCheckpoint);
			double taskEnergyFini(CTask* task, CResource* res);
			int taskEnergyComputeCheckpoint(CTask* task, CResource* res, int startCheckpoint, double energy);

			double resourceIdleEnergy(CResource* res, double seconds);
			double resourceIdlePower(CResource* res);
	};

} }
#endif
