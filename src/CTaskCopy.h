// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#ifndef __CTASKCOPY_H__
#define __CTASKCOPY_H__
#include "CTask.h"
namespace sched {
namespace task {

	/// @brief Explicit class for copies of task objects
	/// The TaskCopy is a snapshot of a task object at a given time.
	class CTaskCopy : public CTask {

		protected:
			CTask* mpOriginal; ///< Pointer to original object

		public:
			/// @brief Returns original task object
			/// @return Pointer to original task object
			CTask* getOriginal();

			/// @brief Assignment constructor
			/// @param other Original task object
			CTaskCopy& operator=(CTask* other);

			CTaskCopy();
			/// @param pOriginal Original task object
			CTaskCopy(CTask* pOriginal);
			~CTaskCopy();

	};

} }
#endif
