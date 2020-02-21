// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#include "CLogger.h"
#include <cstring>
#include <sstream>
#include <iomanip>
#include <time.h>
#ifdef __GLIBC__
#include <execinfo.h>
#include <unistd.h>
#endif
#include <log4cpp/FileAppender.hh>
#include <log4cpp/OstreamAppender.hh>
#include <log4cpp/BasicLayout.hh>
#include <log4cpp/Priority.hh>
#include <log4cpp/PatternLayout.hh>
using namespace sched;


log4cpp::Category* CLogger::mainlog = 0;
log4cpp::Category* CLogger::eventlog = 0;
log4cpp::Category* CLogger::simlog = 0;
std::atomic<int> CLogger::error(0);

void CLogger::startLogging(){

	log4cpp::Layout* layout = new CLoggerLayout();

	// main log

	char const* defaultLogFile = "stdout";
	char* logFile = std::getenv("SCHED_LOG");

	if (logFile == 0) {
		logFile = (char*) defaultLogFile;
	}

	log4cpp::Appender *appender;

	if (strcmp(logFile, "stdout") == 0) {
		appender = new log4cpp::OstreamAppender("console", &std::cout);
	} else {
		appender = new log4cpp::FileAppender("default", logFile);
	}
	appender->setLayout(layout);
	mainlog = &log4cpp::Category::getInstance("main");
	CLogger::mainlog->setAdditivity(false);

	char* envPriority = std::getenv("SCHED_LOG_PRIORITY");
	log4cpp::Priority::PriorityLevel priority = log4cpp::Priority::INFO;
	if (envPriority != 0) {
		if (strcmp(envPriority, "DEBUG") == 0) {
			priority = log4cpp::Priority::DEBUG;
		}
		if (strcmp(envPriority, "NOTICE") == 0) {
			priority = log4cpp::Priority::NOTICE;
		}
		if (strcmp(envPriority, "WARN") == 0) {
			priority = log4cpp::Priority::WARN;
		}
		if (strcmp(envPriority, "ERROR") == 0) {
			priority = log4cpp::Priority::ERROR;
		}
		if (strcmp(envPriority, "CRIT") == 0) {
			priority = log4cpp::Priority::CRIT;
		}
		if (strcmp(envPriority, "ALERT") == 0) {
			priority = log4cpp::Priority::ALERT;
		}
		if (strcmp(envPriority, "FATAL") == 0) {
			priority = log4cpp::Priority::FATAL;
		}
		if (strcmp(envPriority, "EMERG") == 0) {
			priority = log4cpp::Priority::EMERG;
		}
	}

	CLogger::mainlog->setPriority(priority);
	CLogger::mainlog->addAppender(appender);
	CLoggerErrorAppender* errorAppender = new CLoggerErrorAppender("ErrorAppender");
	CLogger::mainlog->addAppender(errorAppender);

	// event log
	char const* defaultEventlogFile = "stdout";
	char* eventlogFile = std::getenv("SCHED_EVENTLOG");

	if (eventlogFile == 0) {
		eventlogFile = (char*) defaultEventlogFile;
	}

	log4cpp::Appender *eventappender;
	if (strcmp(eventlogFile, "stdout") == 0) {
		eventappender = new log4cpp::OstreamAppender("console", &std::cout);
	} else {
		eventappender = new log4cpp::FileAppender("default", eventlogFile);
	}
	log4cpp::Layout* eventlayout = new CLoggerLayoutJson(0);
	eventappender->setLayout(eventlayout);
	eventlog = &log4cpp::Category::getInstance("event");
	CLogger::eventlog->setAdditivity(false);

	log4cpp::Priority::PriorityLevel eventpriority = log4cpp::Priority::INFO;

	CLogger::eventlog->setPriority(eventpriority);
	CLogger::eventlog->addAppender(eventappender);

	// sim log
	char const* defaultSimlogFile = "stdout";
	char* simlogFile = std::getenv("SCHED_SIMLOG");

	if (simlogFile == 0) {
		simlogFile = (char*) defaultSimlogFile;
	}

	log4cpp::Appender *simappender;
	if (strcmp(simlogFile, "stdout") == 0) {
		simappender = new log4cpp::OstreamAppender("console", &std::cout);
	} else {
		simappender = new log4cpp::FileAppender("default", simlogFile);
	}
	log4cpp::Layout* simlayout = new CLoggerLayoutJson("{\"walltime\":\"");
	//log4cpp::PatternLayout *simlayout  = new log4cpp::PatternLayout();
	//simlayout->setConversionPattern("%m%n");
	simappender->setLayout(simlayout);
	simlog = &log4cpp::Category::getInstance("sim");
	CLogger::simlog->setAdditivity(false);

	log4cpp::Priority::PriorityLevel simpriority = log4cpp::Priority::INFO;

	CLogger::simlog->setPriority(simpriority);
	CLogger::simlog->addAppender(simappender);

}

void CLogger::stopLogging(){

	if (mainlog != 0) {
		mainlog = 0;
	}
	if (eventlog != 0) {
		eventlog = 0;
	}
	if (simlog != 0) {
		simlog = 0;
	}
	log4cpp::Category::shutdown();

}

void CLogger::printBacktrace(){

#ifdef __GLIBC__
	CLogger::mainlog->debug("Printing Backtrace");
// https://stackoverflow.com/a/77336
	void *array[100];
	size_t size;
	size = backtrace(array, 10);
	backtrace_symbols_fd(array, size, STDERR_FILENO);
#endif

}

void CLogger::realtime(uint64_t* sec, uint64_t* nsec) {
	
	struct timespec time = {};
	int res = clock_gettime(CLOCK_REALTIME, &time);
	if (-1 == res) {
		errno = 0;
	}
	*sec = time.tv_sec;
	*nsec = time.tv_nsec;

}

std::string CLoggerLayout::format(const log4cpp::LoggingEvent& event){

	std::ostringstream message;

	// microseconds
	//	message << event.timeStamp.getSeconds();
	//	message << ".";
	//	message << std::setfill('0') << std::setw(6);
	//	message << event.timeStamp.getMicroSeconds();

	// nanoseconds
	struct timespec time = {};
	int res = clock_gettime(CLOCK_MONOTONIC, &time);
	if (0 != res) {
		int error = errno;
		errno = 0;
		if (0 != error) {
		}
	}
	message << time.tv_sec;
	message << ".";
	message << std::setfill('0') << std::setw(9);
	message << time.tv_nsec;

	// priority and message
	message << " ";
	message << log4cpp::Priority::getPriorityName(event.priority);
	message << " : ";
	message << event.message;
	message << std::endl;

	return message.str();

}

CLoggerLayout::~CLoggerLayout(){
}

CLoggerLayoutJson::CLoggerLayoutJson(const char* prefix){
	if (prefix == 0) {
		mpPrefix = "{\"time\":\"";
	} else {
		mpPrefix = prefix;
	}
}

std::string CLoggerLayoutJson::format(const log4cpp::LoggingEvent& event){

	std::ostringstream message;

	message << mpPrefix;

	// microseconds
	//	message << event.timeStamp.getSeconds();
	//	message << ".";
	//	message << std::setfill('0') << std::setw(6);
	//	message << event.timeStamp.getMicroSeconds();

	// nanoseconds
	struct timespec time = {};
	int res = clock_gettime(CLOCK_MONOTONIC, &time);
	if (0 != res) {
		int error = errno;
		errno = 0;
		if (0 != error) {
		}
	}
	message << time.tv_sec;
	message << ".";
	message << std::setfill('0') << std::setw(9);
	message << time.tv_nsec;

	// message
	message << "\",";
	message << event.message;
	message << "}";
	message << std::endl;

	return message.str();

}

CLoggerLayoutJson::~CLoggerLayoutJson(){
}

void CLogger::setError(){
	CLogger::error = 1;
}

CLoggerErrorAppender::CLoggerErrorAppender(const std::string &name)
: Appender(name), mName(name) {
}

CLoggerErrorAppender::~CLoggerErrorAppender() {
}

void CLoggerErrorAppender::doAppend (const log4cpp::LoggingEvent &event) {
	if (event.priority <= log4cpp::Priority::ERROR) {
		CLogger::setError();
	}
}

bool CLoggerErrorAppender::reopen() {
	return true;
}

void CLoggerErrorAppender::close() {
}

bool CLoggerErrorAppender::requiresLayout() const {
	return false;
}

void CLoggerErrorAppender::setLayout(log4cpp::Layout* layout) {
}

const std::string& CLoggerErrorAppender::getName() {
	return mName;
}

void CLoggerErrorAppender::setThreshold (log4cpp::Priority::Value priority) {
	mPriority = priority;
}

log4cpp::Priority::Value CLoggerErrorAppender::getThreshold() {
	return mPriority;
}

void CLoggerErrorAppender::setFilter(log4cpp::Filter *filter) {
	mFilter = filter;
}

log4cpp::Filter* CLoggerErrorAppender::getFilter() {
	return mFilter;
}

