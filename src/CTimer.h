// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#ifndef __CTIMER_H__
#define __CTIMER_H__
#include <mutex>
#include <thread>
#include <chrono>
#include <functional>
#include <condition_variable>
namespace sched {
namespace schedule {

	/// @brief Sets up an alarm and executes a function once it goes off
	///
	/// The timer uses a separate thread to wait and execute the function.
	/// Once the timer is set, the thread waits on a mutex.
	/// Unsetting unlocks the mutex to release the timer.
	/// If waiting for the mutex times out, the function is executed.
	class CTimer {

		private:
			std::thread mThread;
			std::mutex mMutex;
			std::condition_variable mWakeCondition;
			std::chrono::high_resolution_clock::time_point mTargetTime;
			std::chrono::steady_clock::duration mDuration;
			std::function<void()> mFunction;
			int mSet = 0;
			int mWaiting = 0;
			int mStopThread = 0;

		private:
			void timerThread();

		public:
			/// @brief Set timer duration and function to execute
			/// @param duration Duration to wait for
			/// @param fun Function that will be executed when the alarm goes off
			void set(std::chrono::steady_clock::duration duration, std::function<void()> fun);
			/// @brief Deactivate timer
			void unset();
			/// @brief Updates the target time
			/// @param update Duration that will be added to the target time
			void updateRelative(std::chrono::steady_clock::duration update);
			CTimer();
			~CTimer();

	};

} }
#endif
