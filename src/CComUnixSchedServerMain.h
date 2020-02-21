// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#ifndef __CCOMUNIXSCHEDSERVERMAIN_H__
#define __CCOMUNIXSCHEDSERVERMAIN_H__
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
namespace com {

	using sched::schedule::CResource;
	using sched::schedule::CScheduleComputer;

	using sched::task::CTaskDatabase;


	/// @brief Main implementation for scheduler server
	class CComUnixSchedServerMain : public CComUnixServer {

		
		protected:
			std::vector<CResource*>& mrResources;
			CTaskDatabase& mrTaskDatabase;
			CScheduleComputer& mrScheduleComputer;
		// inherited by CComUnixServer
			CComUnixClient* createNewClient(int socket);

		public:
			CComUnixSchedServerMain(std::vector<CResource*>& rResources, CTaskDatabase& rTaskDatabase, CScheduleComputer& rScheduleComputer, char* pPath);

			virtual ~CComUnixSchedServerMain();
	};


} }
#endif
