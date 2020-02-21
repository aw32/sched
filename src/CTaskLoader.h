// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#ifndef __CTASKLOADER_H__
#define __CTASKLOADER_H__
namespace sched {
namespace task {

	class CTask;

	/// @brief Loads information to different task types and parameters
	class CTaskLoader {

		public:
			/// @brief Load task information and store them
			/// @return 0 if successful, else -1
			virtual int loadInfo();
			/// @brief Clear stored task information
			virtual void clearInfo();
			/// @brief Add information to task
			/// The information is usually stored to the task attributes map as pointers.
			/// The data itself is saved by the TaskLoader.
			/// The data is freed on shutdown.
			virtual void getInfo(CTask* task);

			/// @brief Load configued TaskLoader
			static CTaskLoader* loadTaskLoader();

			virtual ~CTaskLoader();
	};

} }
#endif
