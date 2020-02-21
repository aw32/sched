// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#ifndef __CLOGGER_H__
#define __CLOGGER_H__
#include <atomic>
#include <log4cpp/Category.hh>
#include <log4cpp/Layout.hh>
#include <log4cpp/LoggingEvent.hh>
namespace sched {

	/// @brief Logging class for access by all other classes
	///
	/// The logging objects should be prepared at application startup and deleted at application shutdown.
	/// In between all classes may use the objects to log messages.
	/// The logging objects have to be deleted as the last step in the application lifecycle to prevent usage of deleted objects.
	/// The logging objects can be used by simply including the CLogger.h file and accessing the static Category objects.
	class CLogger {

		public:
			static log4cpp::Category* mainlog; ///< Logging of application events
			static log4cpp::Category* eventlog; ///< Logging of events concerning scheduling
			static log4cpp::Category* simlog; ///< Logging of events concerning simulation
			static std::atomic<int> error; ///< Tracks if an error message was logged


		public:
			/// @brief Creates the logging objects
			static void startLogging();
			/// @brief Deletes the logging objects
			static void stopLogging();
			/// @brief Allows to print a trace of the current stack (on glibc)
			static void printBacktrace();
			/// @brief Sets the error status 
			static void setError();

			static void realtime(uint64_t* sec, uint64_t* nsec);

	};

	/// @brief Layout class to create messages with nanosecond timestamps
	///
	/// The normal log4cpp layouts do not allow timestamps with higher precision.
	/// The message layout is as follows:
	/// 
	/// SECONDS.NANOSECONDS PRIORITY : MESSAGE\n
	class CLoggerLayout : public log4cpp::Layout {

		public:
			std::string format(const log4cpp::LoggingEvent& event);
			~CLoggerLayout();

	};

	/// @brief Layout class to create json messages with nanosecond timestamps
	///
	/// Certain logs should contain json messages, that can be analyzed later.
	/// The message can contain json attributes without newlines.
	/// When inserted in the layout this results in a json object.
	/// The final log contains one json object per line.
	/// The message layout is as follows:
	///
	/// {"time":"SECONDS.NANOSECONDS",MESSAGE}\n
	class CLoggerLayoutJson : public log4cpp::Layout {

		private:
			const char* mpPrefix;

		public:
			CLoggerLayoutJson(const char* prefix);
			std::string format(const log4cpp::LoggingEvent& event);
			~CLoggerLayoutJson();

	};

	/// @brief Appender that tracks error messages and discards everything
	///
	/// This appender is used to track if an error message was logged.
	/// It is used additionally to normal appenders and therefore the message content is ignored.
	class CLoggerErrorAppender : public log4cpp::Appender {

		public:
			const std::string& mName;
			log4cpp::Priority::Value mPriority;
			log4cpp::Filter* mFilter;

		public:
			CLoggerErrorAppender(const std::string &name);
			virtual ~CLoggerErrorAppender();

			virtual void doAppend(const log4cpp::LoggingEvent &event);
			virtual bool reopen();
			virtual void close ();
			virtual bool requiresLayout() const;
			virtual void setLayout(log4cpp::Layout* layout);
			const std::string& getName();
			virtual void setThreshold (log4cpp::Priority::Value priority);
			virtual log4cpp::Priority::Value getThreshold();
			virtual void setFilter(log4cpp::Filter *filter);
			virtual log4cpp::Filter* getFilter();
	};

}
#endif
