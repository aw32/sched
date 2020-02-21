// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#include "CMeasureNull.h"
#include "CLogger.h"
using namespace sched::measure;


void CMeasureNull::start(){
	CLogger::mainlog->info("MeasureNull: started nothing");
}

void CMeasureNull::stop(){
	CLogger::mainlog->info("MeasureNull: stopped nothing");
}

void CMeasureNull::print(){
}

CMeasureNull::CMeasureNull(){
}

CMeasureNull::~CMeasureNull(){
}
