// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#include "CComUnixSchedServerWrap.h"
#include "CComUnixSchedClientWrap.h"
#include "CLogger.h"
using namespace sched::wrap;


CComUnixSchedServerWrap::CComUnixSchedServerWrap(std::vector<CResource*>& rResources, CTaskDatabase& rTaskDatabase, CUnixWrapClient& rWrap, char* pPath) :
	CComUnixServer(pPath),
	mrWrap(rWrap),
	mrResources(rResources),
	mrTaskDatabase(rTaskDatabase)
	{

	CLogger::mainlog->info("UnixServer: socket path: %s", mpPath);
}

CComUnixSchedServerWrap::~CComUnixSchedServerWrap() {
}

CComUnixClient* CComUnixSchedServerWrap::createNewClient(int socket) {

	CComUnixClient* client = new CComUnixSchedClientWrap(*this, mrResources, mrTaskDatabase, mrWrap, socket);

	return client;
}
