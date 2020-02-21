// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#include <signal.h>
#include <cstring>
#include <unistd.h>
#include <sys/syscall.h>
#include "CComUnixWriteBuffer.h"
#include "CLogger.h"
using namespace sched::com;

CComUnixWriteBuffer::CComUnixWriteBuffer()
{

	this->mThread = std::thread(&CComUnixWriteBuffer::serve, this);

}

CComUnixWriteBuffer::~CComUnixWriteBuffer(){
}

void CComUnixWriteBuffer::stop(){
	{
		std::lock_guard<std::mutex> lg(mMutex);
		mStopThread = 1;
	}
	mCondVar.notify_one();
	if (mThread.joinable() == true) {
		mThread.join();
	}
}

void CComUnixWriteBuffer::addMessage(CComUnixWriteMessage* message) {

	{
		std::lock_guard<std::mutex> lg(mMutex);
		if (mStopThread == 1) {
			CLogger::mainlog->warn("ComUnixWriter: shut down server received message, message will be dropped");
			delete message;
			return;
		}

		mMessageQueue.push(message);
	}
	mCondVar.notify_one();

}

void CComUnixWriteBuffer::serve(){

    pid_t pid = syscall(SYS_gettid);
    CLogger::mainlog->debug("ComUnixWriter: thread %d", pid);

	// mask signals for this thread
	int ret = 0;
	sigset_t sigset = {}; 
	ret = sigemptyset(&sigset);
	ret = sigaddset(&sigset, SIGINT);
	ret = sigaddset(&sigset, SIGTERM);
	//ret = sigprocmask(SIG_BLOCK, &sigset, 0);
	ret = pthread_sigmask(SIG_BLOCK, &sigset, 0); 
	if (ret != 0) {
		CLogger::mainlog->error("ComUnixWriter: thread signal error: %s", strerror(errno));
	}

	CComUnixWriteMessage* message = 0;
	int listSize = 0;

	while (mStopThread != 1){

		{
			std::unique_lock<std::mutex> ul(mMutex);
			while (mMessageQueue.size() == 0 && mStopThread == 0) {
				mCondVar.wait(ul);
			}
		}

		// message write loop
		do {
			// check list and get next message
			{
				std::unique_lock<std::mutex> ul(mMutex);
				listSize = mMessageQueue.size();
				if (listSize > 0) {
					message = mMessageQueue.front();
					mMessageQueue.pop();
				}
			}

			// abort if there are no messages
			if (listSize == 0) {
				break;
			}

			// write message
			writeMessage(message);

		} while(listSize > 0);

	}
}

void CComUnixWriteBuffer::writeMessage(CComUnixWriteMessage* message){

	// check if client still exists
	if (message->writer->exist() == false) {
		return;
	}

	message->writer->writeMessage(message);

	delete message;
}

CComUnixWriter::CComUnixWriter(CComServer& rServer) :
	CComClient(rServer) {
}
