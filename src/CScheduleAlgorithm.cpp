// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#include "CScheduleAlgorithm.h"
using namespace sched::algorithm;


CScheduleAlgorithm::CScheduleAlgorithm(std::vector<CResource*>& rResources):
	mrResources(rResources)
{
}


CScheduleAlgorithm::~CScheduleAlgorithm(){
}
