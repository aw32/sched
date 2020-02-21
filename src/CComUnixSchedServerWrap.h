// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#ifndef __CCOMUNIXSCHEDSERVERWRAP_H__
#define __CCOMUNIXSCHEDSERVERWRAP_H__
#include "CComUnixServer.h"
namespace sched {
namespace schedule {
	class CResource;
	class CScheduleComputer;
} }


namespace sched {
namespace task {
	class CTaskDatabase;
} }

namespace sched {
namespace wrap {

	using sched::com::CComUnixServer;
	using sched::com::CComUnixClient;
	using sched::schedule::CResource;
	using sched::schedule::CScheduleComputer;

	using sched::task::CTaskDatabase;

	class CUnixWrapClient;

	/// @brief Wrap implementation of sched server
	class CComUnixSchedServerWrap : public CComUnixServer {

		
		protected:
			CUnixWrapClient& mrWrap;
			std::vector<CResource*>& mrResources;
			CTaskDatabase& mrTaskDatabase;
		// inherited by CComUnixServer
			CComUnixClient* createNewClient(int socket);

		public:
			CComUnixSchedServerWrap(std::vector<CResource*>& rResources, CTaskDatabase& rTaskDatabase, CUnixWrapClient& rWrap, char* pPath);

			virtual ~CComUnixSchedServerWrap();
	};


} }
#endif
