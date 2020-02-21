// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#ifndef __CFEEDBACKMAIN_H__
#define __CFEEDBACKMAIN_H__
#include <mutex>
#include <condition_variable>
#include <vector>
#include "CFeedback.h"

namespace sched {
namespace schedule {

	class CResource;

	/// @brief Feedback implementation for main scheduler
	///
	/// Checks all resources.
	/// If the resource is idling, the progress is up-to-date.
	/// If the resource is busy, the progress value is requested.
	/// The call to getProgress() waits until progress for all resources is up-to-date.
	/// Resources call gotProgress() for arriving progress information or finished tasks.
	/// After all progress values were updated getProgress() returns.
	class CFeedbackMain : public CFeedback {

		private:
			bool mStopFeedback = false;
			bool* mpProgressTrack;
			std::mutex mProgressMutex;
			std::condition_variable mProgressCondVar;
			std::vector<CResource*>& mrResources;

		public:
			CFeedbackMain(std::vector<CResource*>& rResources);
			virtual ~CFeedbackMain();
			/// @brief Requests progress values and waits for updates.
			int getProgress();
			/// @brief Notes the task's progress for the given resource as up-to-date.
			void gotProgress(CResource& res);

	};

} }
#endif
