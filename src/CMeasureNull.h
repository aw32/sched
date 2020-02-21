// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#ifndef __CMEASURENULL_H__
#define __CMEASURENULL_H__
#include "CMeasure.h"
namespace sched {
namespace measure {

	/// @brief Measurement class that does nothing
	class CMeasureNull : public CMeasure {

		public:
			void start();
			void stop();
			void print();

			CMeasureNull();
			~CMeasureNull();

	};

} }
#endif
