// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#include "CFeedbackMain.h"
#include "CLogger.h"
#include "CResource.h"
using namespace sched::schedule;


CFeedbackMain::CFeedbackMain(std::vector<CResource*>& rResources):
	mrResources(rResources)
{

	mpProgressTrack = new bool[mrResources.size()];

	for (unsigned int i=0; i<mrResources.size(); i++) {
		mrResources[i]->setFeedback(this);
	}

}

CFeedbackMain::~CFeedbackMain(){

	mStopFeedback = true;
	mProgressCondVar.notify_one();

	if (mpProgressTrack != 0) {
		delete [] mpProgressTrack;
		mpProgressTrack = 0;
	}


}

int CFeedbackMain::getProgress(){

	CLogger::eventlog->info("\"event\":\"FEEDBACK_GETPROGRESS\"");
	unsigned int resourceCount = mrResources.size();
	for (unsigned int i=0; i<resourceCount; i++) {
		CResource* resource = mrResources[i];
		mpProgressTrack[i] = false;
		int ret = resource->requestProgress();
		if (ret == 1) {
			mpProgressTrack[i] = true;
			CLogger::mainlog->debug("Feedback: res %d progress done", i);
		}
	}
	unsigned int progressCount = 0;
	for (unsigned int i=0; i<resourceCount; i++) {
		if (mpProgressTrack[i] == true) {
			progressCount++;
		}
	}

	// wait for resource progress response
	{

		std::unique_lock<std::mutex> ul(mProgressMutex);
		while (progressCount < resourceCount) {
			mProgressCondVar.wait(ul);
			if (mStopFeedback == true) {
				return 0;
			}
			progressCount = 0;
			for (unsigned int i=0; i<resourceCount; i++) {
				if (mpProgressTrack[i] == true) {
					progressCount++;
				}
			}
		}
	}
	
	CLogger::eventlog->info("\"event\":\"FEEDBACK_GOTPROGRESS\"");
	CLogger::mainlog->debug("Feedback: progress done");

	return 0;

}

void CFeedbackMain::gotProgress(CResource& res){

	{
		std::lock_guard<std::mutex> lg(mProgressMutex);
		mpProgressTrack[res.mId] = true;
		CLogger::mainlog->debug("Feedback: res %d progress done", res.mId);
	}
	mProgressCondVar.notify_one();

}
