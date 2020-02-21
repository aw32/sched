// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#ifndef __CTASKLOADERMS_H__
#define __CTASKLOADERMS_H__
#include <map>
#include "CTaskLoader.h"
namespace sched {
namespace task {

	/// @brief Loads task information from CSV files stored by egysched tasks
	class CTaskLoaderMS : public CTaskLoader {

		private:
			//[name][size][resource]
			std::map<std::string, std::map<int, std::map<std::string, double*>*>*> mInfo;
			const char* mpInfoDir = 0;

		private:
			double* loadTaskInfo(char* fname);
			void addTaskInfo(double* info, char* name, int size, char* resource, char* type);

		public:
			CTaskLoaderMS();
			virtual ~CTaskLoaderMS();

			virtual int loadInfo();
			virtual void clearInfo();
			virtual void getInfo(CTask* task);

	};

} }
#endif
