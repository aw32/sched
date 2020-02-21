// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#ifndef __CCOMCLIENT_H__
#define __CCOMCLIENT_H__

namespace sched {
namespace com {

	class CComServer;

	/// @brief Represents a connected client
	class CComClient {

		protected:
			CComServer& mrServer; ///< Server object the client connected to

		public:
			/// @param rServer Server that accepted the client
			CComClient(CComServer& rServer);
			/// @brief Checks if the client is still registered at the server
			bool exist();
			virtual ~CComClient();

	};

} }
#endif
