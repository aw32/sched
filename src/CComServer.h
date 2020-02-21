// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#ifndef __CCOMSERVER_H__
#define __CCOMSERVER_H__
#include <vector>
namespace sched {
namespace com {

	class CComClient;

	/// @brief Represents the server for arbitrary communication
	class CComServer {

		protected:
			std::vector<CComClient*> mClients; ///< list of clients connected to this server

		public:
			/// @brief Removes the client from the server
			virtual void removeClient(CComClient* client) = 0;
			/// @brief Checks if the client is still connected to the server
			virtual bool findClient(CComClient* client) = 0;
			/// @brief Starts the server
			virtual int start() = 0;
			/// @brief Stops the server
			virtual void stop() = 0;
			virtual ~CComServer() = 0;
			
	};

} }
#endif
