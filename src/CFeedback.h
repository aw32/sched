// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#ifndef __CFEEDBACK_H__
#define __CFEEDBACK_H__
#include <mutex>
#include <condition_variable>
#include <vector>
namespace sched {
namespace schedule {

	class CResource;

	/// @brief Used to request progress values from the tasks currently running.
	///
	/// An interested object calls getProgress().
	/// getProgress() requests progress values and waits for responses.
	/// Calls to gotProgress() suggest that the responses arrived and the progress values were updated.
	class CFeedback {

		public:
			CFeedback();
			virtual ~CFeedback();
			/// @brief Request progress
			virtual int getProgress() = 0;
			/// @brief Called in case requested progress values arrive
			virtual void gotProgress(CResource& res) = 0;

	};

} }
#endif
