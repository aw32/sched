// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#ifndef __CCOMUNIXSERVER_H__
#define __CCOMUNIXSERVER_H__
#include <thread>
#include <vector>
#include <mutex>
#include "CComServer.h"
#include "CComUnixWriteBuffer.h"

namespace sched {
namespace com {


	class CComUnixClient;

	/// @brief Server for Unix socket clients
	///
	/// Opens a Unix socket and listens for incoming clients.
	/// Uses poll() to listen on a list of sockets.
	/// For incoming data on client sockets the server thread calls the clients' read() method to process the data.
	class CComUnixServer : public CComServer {

		private:
			CComUnixWriteBuffer mrWriter;
			std::mutex mClientMutex;
			std::thread mThread;
			pid_t mPid;
			int mSocket;
			struct pollfd* mPollList = 0;
			long mPollListPos = 0;
			long mPollListMax = 1024;
			bool mStopServer = false;
			bool mDoneServer = true;
			int mWakeupPipe[2] = {};
			int mWakeup = 0; // 0x01 shutdown, 0x02 a new poll entry arrived

		protected:
			char* mpPath; ///< Path to Unix socket

		private:
			void serve();
			void incPollList();
			void addPollEntry(int fd);
			void removePollEntry(int fd);
			int sendWakeup(int msg);
			int recvWakeup();
			void clearClients();
			void readClients(int fd);
			CComClient* getClientById(int id);

		protected:
			/// @brief Creates a new client object from the socket id
			/// @param socket Socket id of the incoming client
			virtual CComUnixClient* createNewClient(int socket) = 0;

		// inherited by CComServer
		public:
			/// @brief Removes the given client from the client list
			/// @param pClient Client to remove
			void removeClient(CComClient* pClient);
			/// @brief Checks if the client is still in the client list
			/// @param pClient Pointer to client object
			/// @return True if the client is still present in the client, else false
			bool findClient(CComClient* pClient);

		public:
			/// @brief Add socket id to poll list
			/// @param socket Socket id
			void addNewClient(int socket);
			/// @brief Add new client to client list and socket id to the poll list
			/// @param uclient Client object to add
			/// @param socket Socket id
			void addNewClient(CComUnixClient* uclient, int socket);
			/// @brief Return the server writer object
			/// @return The server writer object
			CComUnixWriteBuffer& getWriter();
			/// @brief Starts the server: opens a Unix socket and starts the server thread
			virtual int start();
			/// @brief Stops the server thread and closes the Unix socket
			virtual void stop();
			/// @param unixpath Path to Unix socket
			CComUnixServer(char* unixpath);
			virtual ~CComUnixServer();

	};

} }
#endif
