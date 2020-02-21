// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#include "CTask.h"
#include "CLogger.h"
using namespace sched::task;

CTask::CTask(){
}

CTask::CTask(std::string* pName,
			long size,
			long checkpoints,
			std::vector<CResource*>* pResources,
			int* pDeplist,
			int depNum){

	mpName = pName;
	mSize = size;
	mCheckpoints = checkpoints;
	mpResources = pResources;
	mpPredecessorList = pDeplist;
	mPredecessorNum = depNum;

	mpAttributes = new std::map<std::string, void*>();

}

CTask::CTask(const CTask& copy) {
	*this = copy;
}

CTask& CTask::operator=(const CTask& other) {

	mId = other.mId;
	mpName = other.mpName;
	mSize = other.mSize;
	mCheckpoints = other.mCheckpoints;
	mProgress = other.mProgress;
	mState = other.mState;
	mTimes = other.mTimes;
	mpPredecessorList = other.mpPredecessorList;
	mPredecessorNum = other.mPredecessorNum;
	mpSuccessorList = other.mpSuccessorList;
	mSuccessorNum = other.mSuccessorNum;
	mpResources = other.mpResources;
	mpAttributes = other.mpAttributes;
	return *this;
}

CTask::~CTask(){
	
	if (mpAttributes != 0) {

		for(std::map<std::string, void*>::iterator it = mpAttributes->begin(); it != mpAttributes->end(); ++it) {
			//delete it->second;
		}

		delete mpAttributes;
		mpAttributes = 0;
	}
	if (mpPredecessorList != 0) {
		delete[] mpPredecessorList;
		mpPredecessorList = 0;
		mPredecessorNum = 0;
	}
	if (mpSuccessorList != 0) {
		delete[] mpSuccessorList;
		mpSuccessorList = 0;
		mSuccessorNum = 0;
	}
	if (mpResources != 0) {
		delete mpResources;
		mpResources = 0;
	}
	if (mpName != 0) {
		delete mpName;
		mpName = 0;
	}
}

bool CTask::validResource(CResource* res){
	if (std::find(
			mpResources->begin(),
			mpResources->end(),
			res)
		 == mpResources->end()) {
	// task is not compatible with resource 
		return false;
	}
	return true;
}
