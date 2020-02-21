// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#ifndef __CCONFIG_H__
#define __CCONFIG_H__

#include <map>
#include <vector>
#include <yaml.h>

namespace sched {

	/// @brief Type of configuration node
	enum EConfType {
		None,
		Map,
		List,
		String
	};

	/// @brief Node in configuration hierarchy
	class CConf {
		public:
			enum EConfType mType; ///< Type of the current node
			union {
				std::map<std::string, CConf*>* mpMap;
				std::vector<CConf*>* mpList;
				std::string* mpString;
			} mData; ///< Data of the current node
		
		public:
			/// @brief Returns the configuration data named name as uint64
			int getUint64(char* name, uint64_t* dest);
			/// @brief Returns the configuration data named name as double
			int getDouble(char* name, double* dest);
			/// @brief Returns the configuration data named name as string
			int getString(char* name, std::string** dest);
			/// @brief Returns the configuration data named name as bool
			int getBool(char* name, bool* dest);
			/// @brief Returns the configuration node named name
			int getConf(char* name, CConf** conf);
			/// @brief Returns the list of configuration nodes named name
			int getList(char* name, std::vector<CConf*>** mpList);
			~CConf();
	};

	/// @brief Configuration base class
	///
	/// The class loads the configuration from a YAML file and stores it.
	/// The configuration can be accessed by including CConfig.h and calling getConfig().
	class CConfig {

		private:
			static int parseConfig(yaml_parser_t* pParser, CConf** pResult);
			static CConfig* config;
		public:
			/// @brief Root configuration node
			CConf* conf;

		public:
			CConfig();
			~CConfig();
			/// @brief Loads configuration from the given path to a YAML file
			static int loadConfig(char* pConfigFile);
			/// @brief Cleans up the loaded configuration
			static void cleanConfig();
			/// @brief Returns the previously loaded configuration
			static CConfig* getConfig();

	};

}
#endif
