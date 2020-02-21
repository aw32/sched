// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#include "CComUnixSchedServerMain.h"
#include "CComUnixSchedClientMain.h"
#include "CLogger.h"
using namespace sched::com;


CComUnixSchedServerMain::CComUnixSchedServerMain(std::vector<CResource*>& rResources, CTaskDatabase& rTaskDatabase, CScheduleComputer& rScheduleComputer, char* pPath) :
	CComUnixServer(pPath),
	mrResources(rResources),
	mrTaskDatabase(rTaskDatabase),
	mrScheduleComputer(rScheduleComputer)
	{

	CLogger::mainlog->info("UnixServer: socket path: %s", mpPath);
}

CComUnixSchedServerMain::~CComUnixSchedServerMain() {
}

CComUnixClient* CComUnixSchedServerMain::createNewClient(int socket) {

	CComUnixSchedClientMain* sclient = new CComUnixSchedClientMain(*this, mrResources, mrTaskDatabase, mrScheduleComputer, socket);
	//CComUnixClient* client = new CComUnixSchedClientMain(*this, mrResources, mrTaskDatabase, mrScheduleComputer, socket);
	CComUnixClient* client = dynamic_cast <CComUnixClient*> (sclient);
	

	//CComClient* cclient = dynamic_cast <CComClient*> (client);
	//CComUnixClient* uclient = dynamic_cast<CComUnixClient*> (cclient);
	//CLogger::mainlog->debug("New Client is a CComUnixClient?? %x", uclient);

	return client;
}
