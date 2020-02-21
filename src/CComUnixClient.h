// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#ifndef __CCOMUNIXCLIENT_H__
#define __CCOMUNIXCLIENT_H__

namespace sched {
namespace com {

	class CComUnixServer;

	/// @brief Client class for Unix socket clients
	class CComUnixClient {

		protected:
			int mSocket; ///< Socket id

		public:
			/// @param socket Socket id of incoming client
			CComUnixClient(int socket);
			/// @brief Reads from the socket and processes the data
			virtual int read() = 0;
			virtual ~CComUnixClient();

		friend class CComUnixServer;
	};

} }
#endif
