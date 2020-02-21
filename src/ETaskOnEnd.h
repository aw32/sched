// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#ifndef __ETASKONEND_H__
#define __ETASKONEND_H__

namespace sched {
namespace task {


	/// @brief Describes task behaviour in case it reaches the target checkpoint
	enum ETaskOnEnd {
		TASK_ONEND_SUSPENDS, ///< The task suspends execution and sends a suspension message
		TASK_ONEND_CONTINUES ///< The task sends a progress message and continues execution
	};

	extern const char* ETaskOnEndString[];


} }

#endif
