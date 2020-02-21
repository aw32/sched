// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#ifndef __CTASKDEFINITIONS_H__
#define __CTASKDEFINITIONS_H__
#include <vector>
#include "cjson/cJSON.h"
namespace sched {
namespace wrap {

	/// @brief Holds one task definition
	///
	/// The task definition defines how to start the task, the available resources and task sizes.
	class CTaskDefinition {
		public:
			char* name; ///< Name of the task
			char* wd; ///< Working directory the process is started in
			char** arg; ///< Process arguments, list of strings, last entry is 0
			char** env; ///< Process environment variables, list of strings (KEY=VALUE), last entry is 0
			char** res; ///< Possible resources, list of strings, last entry is 0
			int** sizes; ///< Possible sizes per resource, may be 0 if list is empty, each list ends with 0
			bool subtask; ///< If the task is exclusively found as a subtask

		public:
			/// @param name
			/// @param wd
			/// @param arg
			/// @param env
			/// @param res
			/// @param sizes
			/// @param subtask
			CTaskDefinition(
				char* name,
				char* wd,
				char** arg,
				char** env,
				char** res,
				int** sizes,
				bool subtask);

			~CTaskDefinition();
	};

	/// @brief Loads and holds task definitions
	///
	/// Task definitions are stored in a JSON file.
	/// The JSON file consists of an root object containing the task name as keys pointing to the definitions as objects.
	class CTaskDefinitions {

		public:
			std::vector<CTaskDefinition*> mDefinitions; ///< List of loaded task definitions
			cJSON* mpJson = 0; ///< Pointer to JSON structure
		
		protected:
			/// @brief Load task definition from JSON structure
			static CTaskDefinition* loadTaskDefinition(cJSON* json);

		public:
			/// @brief Find definition for task with given name
			/// @param name Name of the task
			/// @return Pointer to TaskDefinition if found, else 0
			CTaskDefinition* findTask(char* name);
			~CTaskDefinitions();

			/// @brief Load task definitions from JSON file
			/// @param filepath Path to JSON file
			/// @return Task definitions object
			static CTaskDefinitions* loadTaskDefinitions(char* filepath);

	};

} }
#endif
