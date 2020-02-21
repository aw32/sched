// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#include "CMeasure.h"
using namespace sched::measure;

CMeasure* CMeasure::current = 0;

CMeasure::CMeasure(){
	current = this;
}

CMeasure::~CMeasure(){
	current = 0;
}

CMeasure* CMeasure::getMeasure() {
	return current;
}
