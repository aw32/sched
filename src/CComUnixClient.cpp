// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#include "CComUnixClient.h"
using namespace sched::com;

CComUnixClient::CComUnixClient(int socket) : mSocket(socket) {

}

CComUnixClient::CComUnixClient::~CComUnixClient() {
}
