// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#ifndef __CMEASURE_H__
#define __CMEASURE_H__
namespace sched {
namespace measure {

	/// @brief Interface for measurement classes
	class CMeasure {

		protected:
			static CMeasure* current;

		public:
			/// @brief Starts measurement
			virtual void start() = 0;
			/// @brief Stops measurement
			virtual void stop() = 0;
			/// @brief Print current measurement results
			virtual void print() = 0;
			
			CMeasure();
			virtual ~CMeasure();
			static CMeasure* getMeasure();
	};

} }
#endif
