// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#include <signal.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <cerrno>
#include <cstring>
#include "CTimer.h"
#include "CLogger.h"
using namespace sched::schedule;


CTimer::CTimer(){
	this->mThread = std::thread(&CTimer::timerThread, this);
}

CTimer::~CTimer(){
	mStopThread = 1;
	mWakeCondition.notify_one();
	this->mThread.join();
}

void CTimer::set(std::chrono::steady_clock::duration duration, std::function<void()> fun){

	{
		std::lock_guard<std::mutex> lg(mMutex);

		std::chrono::high_resolution_clock::time_point now = std::chrono::high_resolution_clock::now();
		mTargetTime = now + duration;
		mDuration = duration;
		mFunction = fun;
		mSet = 1;

		mWakeCondition.notify_one();
	}
}

void CTimer::unset(){

	{
		std::lock_guard<std::mutex> lg(mMutex);
		mDuration = {};
		mFunction = 0;
		mSet = 0;
		if (mWaiting == 1) {
			mWakeCondition.notify_one();
		}
	}
}

void CTimer::updateRelative(std::chrono::steady_clock::duration update) {

	{
		std::lock_guard<std::mutex> lg(mMutex);
		mTargetTime += update;
		mDuration += update;
		mSet = 1;
		// reset and wake up timer, it will go to sleep right away
		mWakeCondition.notify_one();
	}
}

void CTimer::timerThread(){

    // mask signals for this thread
    int ret = 0;
    sigset_t sigset = {};
    ret = sigemptyset(&sigset);
	ret = sigaddset(&sigset, SIGINT);
	ret = sigaddset(&sigset, SIGTERM);
    //ret = sigprocmask(SIG_BLOCK, &sigset, 0);
    ret = pthread_sigmask(SIG_BLOCK, &sigset, 0);
    if (ret != 0) {
		CLogger::mainlog->error("Timer: thread signal error: %s", strerror(errno));
    }

	pid_t pid = syscall(SYS_gettid);
	CLogger::mainlog->debug("Timer: thread %d", pid);

	{
		std::unique_lock<std::mutex> lg(mMutex);
		while (mStopThread == 0) {

			// wait for new time
			while (mSet == 0 && mStopThread == 0) {
				// wait for new setting
				mWakeCondition.wait(lg);
			} 

			if (mStopThread == 1) {
				break;
			}

			// set waiting
			mWaiting = 1;
			mSet = 0;

			std::cv_status status = mWakeCondition.wait_until(lg, mTargetTime);
			if (status == std::cv_status::no_timeout) {
				// aborted

				// unset waiting
				mWaiting = 0;
			} else {
				// time out, call function
				
				// unset waiting and call function
				mWaiting = 0;
				mFunction();
			}
		}
	}
}
