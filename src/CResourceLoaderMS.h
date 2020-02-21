// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#ifndef __CRESOURCELOADERMS_H__
#define __CRESOURCELOADERMS_H__
#include "CResourceLoader.h"
namespace sched {
namespace schedule {

	/// @brief Load MS specific resource information
	class CResourceLoaderMS : public CResourceLoader {

		private:
			const char* mpIdleFile = 0; ///< Path to idle information file
			double cpu_power_avg = 0.0; ///< CPU idle power
			double gpu_power_avg = 0.0; ///< GPU idle power
			double fpga_power_avg = 0.0; ///< FPGA idle power
			double all_power_avg = 0.0; ///< Combined idle power

		public:
			CResourceLoaderMS();
			virtual ~CResourceLoaderMS();

			virtual int loadInfo();
			virtual void clearInfo();
			virtual void getInfo(CResource* resource);

	};

} }
#endif
