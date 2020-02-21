// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#include "CComClient.h"
#include "CComServer.h"
using namespace sched::com;

CComClient::CComClient(CComServer& rServer) :
	mrServer(rServer) {
}

CComClient::~CComClient() {
}

bool CComClient::exist() {

	return mrServer.findClient(this);
}
