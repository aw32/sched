// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#ifndef __CCOMUNIXWRITEBUFFER_H__
#define __CCOMUNIXWRITEBUFFER_H__
#include <queue>
#include <mutex>
#include <thread>
#include <condition_variable>
#include "CComClient.h"

namespace sched {
namespace com {

	class CComUnixWriteMessage;

	/// @brief Base class for clients that need to buffer messages
	class CComUnixWriter : public CComClient {
		public:
			/// @param Server this writer object belongs to
			CComUnixWriter(CComServer& rServer);
			/// @brief Writes message to internal socket using custom method
			///
			/// The client's implementation of writeMessage() processes the message and writes it to the outgoing socket.
			/// This happens using the buffer's thread.
			virtual void writeMessage(CComUnixWriteMessage* message) = 0;
	};

	/// @brief Base class for buffered messages
	class CComUnixWriteMessage {
		public:
			CComUnixWriter* writer; ///< Client that writes messages
			virtual ~CComUnixWriteMessage(){};
	};

	/// @brief Buffers outgoing messages for UnixClients
	///
	/// To desynchronize writing to unix sockets and freeing of internal threads clients may delegate writing to this writer thread.
	/// Clients can store messages as custom objects and pass them to this buffer.
	/// The buffer consists of a queue containing the messages and a thread processing the queue.
	/// The thread dequeues messages and passes them to the client's writing method.
	///
	/// Messages need to inherit from CComUnixWriteMessage to contain a pointer to the writer object.
	///
	/// Client need to inherit from CComUnixWriter so the buffer can call the writeMessage() method.
	class CComUnixWriteBuffer {

		private:
			// list
			std::queue<CComUnixWriteMessage*> mMessageQueue;
			// list and thread
			std::mutex mMutex;
			std::condition_variable mCondVar;
			std::thread mThread;
			int mStopThread = 0;

		private:
			void serve();
			void writeMessage(CComUnixWriteMessage* message);

		public:
			CComUnixWriteBuffer();
			~CComUnixWriteBuffer();
			/// @brief Add message to buffer
			/// @param message Message containing pointer to writer
			void addMessage(CComUnixWriteMessage* message);
			void stop();
	};

} }
#endif
