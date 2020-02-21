// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#ifndef __CMEASUREAMPEHRE_H__
#define __CMEASUREAMPEHRE_H__
#include "ms_measurement.h"
#include "CMeasure.h"
namespace sched {
namespace measure {

	/// @brief Measurment class that uses the Ampehre framework
	///
	/// The Ampehre framework is used to regularly sample measurement values.
	/// Print() is used to log the current values.
	class CMeasureAmpehre : public CMeasure {

		private:
			MS_VERSION version;
			MS_SYSTEM* ms = 0;
			MS_LIST* ml = 0;
			MS_MEASUREMENT_CPU *cpu = 0;
			MS_MEASUREMENT_GPU *gpu = 0;
			MS_MEASUREMENT_FPGA *fpga = 0;
			MS_MEASUREMENT_MIC *mic = 0;
			MS_MEASUREMENT_SYS *sys = 0;
			bool mMeasure = false;
			uint64_t cpu_s = 10;
			uint64_t cpu_ns = 0;
			uint64_t gpu_s = 10;
			uint64_t gpu_ns = 0;
			uint64_t fpga_s = 10;
			uint64_t fpga_ns = 0;
			uint64_t system_s = 10;
			uint64_t system_ns = 0;
			uint64_t mic_s = 10;
			uint64_t mic_ns = 0;

		public:
			/// @brief Start Ampehre measurement
			virtual void start();
			/// @brief Stop Ampehre measurement
			virtual void stop();
			/// @brief Print current values
			virtual void print();
			
			CMeasureAmpehre();
			~CMeasureAmpehre();

	};

} }
#endif
