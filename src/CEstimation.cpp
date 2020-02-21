// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#include "CEstimation.h"
#include "CEstimationLinear.h"
using namespace sched::algorithm;

CEstimation::~CEstimation(){
}

CEstimation* CEstimation::getEstimation(){

	return new CEstimationLinear();

}
