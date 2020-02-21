// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#include "CTaskCopy.h"
#include "CLogger.h"
using namespace sched::task;

CTaskCopy::CTaskCopy(){

}

CTaskCopy::CTaskCopy(CTask* pOriginal):
	mpOriginal(pOriginal)
{

	if (pOriginal != 0) {
		*this = pOriginal;
	}

}

CTaskCopy::~CTaskCopy(){

	// remove dynamic members to avoid double free
	mpAttributes = 0;
	mpPredecessorList = 0;
	mpSuccessorList = 0;
	mpResources = 0;
	mpName = 0;

}

CTask* CTaskCopy::getOriginal(){

	return mpOriginal;

}

CTaskCopy& CTaskCopy::operator=(CTask* other){

	if (other != 0) {
		CTask* me = this;
		*me = *other;
		this->mpOriginal = other;
	}

	return *this;

}
