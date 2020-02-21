// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#include "CConfig.h"
#include <cstdlib>
#include <cstdio>
#include <cerrno>
#include <yaml.h>
#include <sys/stat.h>
#include <unistd.h>
#include <iostream>
#include "CLogger.h"
using namespace sched;


CConfig* CConfig::config = 0;

CConfig::CConfig(){
}

CConfig::~CConfig(){
	if (conf != 0) {
		delete conf;
		conf = 0;
	}
}

CConfig* CConfig::getConfig(){

	return config;

}

void CConfig::cleanConfig(){
	if (config != 0) {
		delete config;
		config = 0;
	}
}

int CConfig::loadConfig(char* pConfigFile){

	yaml_parser_t yaml_parser = {};

	// Initialize parser
	int yamlret = 0;
	yamlret = yaml_parser_initialize(&yaml_parser);
	if (yamlret != 1) { // yamlret == 1 on success, 0 on error
		return -1;
	}

	// Open file
	FILE *input = fopen(pConfigFile, "rb");
	if (input == NULL) {
		CLogger::mainlog->errorStream() << "Failed to open config file " << pConfigFile << " Error: " << errno;
		return -1;
	}

	yaml_parser_set_input_file(&yaml_parser, input);

	// Do the parsing
	CConf* conf;
	int parseResult = CConfig::parseConfig(&yaml_parser, &conf);
	yaml_parser_delete(&yaml_parser);
	fclose(input);

	if (parseResult != 0) {
		// Parsing failed
		return parseResult;
	}

	// Set new config
	CConfig* newconfig = new CConfig();
	newconfig->conf = conf;

	CConfig* oldconfig = config;
	config = newconfig;

	// Remove old config
	if (oldconfig != 0) {
		delete oldconfig;
	}

	return 0;
}

int CConfig::parseConfig(yaml_parser_t* pParser, CConf** pResult) {
	yaml_event_t event;

	CConf* root = new CConf();
//	root->mType = EConfType::Map;
//	root->mData.mpMap = new std::map<std::string, CConf*>();
	bool key = false;
	std::vector<CConf*> stack;
	stack.push_back(root);

	CLogger::mainlog->debug("Config: Start config parsing");

	bool error = false;

	do {
		if (!yaml_parser_parse(pParser, &event)) {
			char* error_type = (char*) "PARSER_ERROR";
			switch(pParser->error) {
				case YAML_NO_ERROR:
					error_type = (char*) "NO_ERROR";
					break;
				case YAML_MEMORY_ERROR:
					error_type = (char*) "MEMORY_ERROR";
					break;
				case YAML_READER_ERROR:
					error_type = (char*) "READER_ERROR";
					break;
				case YAML_SCANNER_ERROR:
					error_type = (char*) "SCANNER_ERROR";
					break;
				case YAML_COMPOSER_ERROR:
					error_type = (char*) "COMPOSER_ERROR";
					break;
				case YAML_WRITER_ERROR:
					error_type = (char*) "WRITER_ERROR";
					break;
				case YAML_EMITTER_ERROR:
					error_type = (char*) "EMITTER_ERROR";
					break;
				default:
				case YAML_PARSER_ERROR:
					;
			}
			CLogger::mainlog->error("Config: YAML Parser error: %s, Problem: %s %d", error_type, pParser->problem, pParser->problem_offset );
			CLogger::mainlog->error("Config: YAML Parser error position: index %d line %d column %d, Context: %s", pParser->problem_mark.index, pParser->problem_mark.line, pParser->problem_mark.column, pParser->context);
			error = true;
			break;
		}
		switch(event.type)
		{ 
			case YAML_NO_EVENT:
				CLogger::mainlog->debug("Config: No Event");
				break;
			/* Stream start/end */
			case YAML_STREAM_START_EVENT:
				CLogger::mainlog->debug("Config: Start Stream");
				break;
			case YAML_STREAM_END_EVENT:
				CLogger::mainlog->debug("Config: End Stream");
				break;
			/* Block delimeters */
			case YAML_DOCUMENT_START_EVENT:
				CLogger::mainlog->debug("Config: Start Document");
				break;
			case YAML_DOCUMENT_END_EVENT:
				CLogger::mainlog->debug("Config: End Document");
				break;
			case YAML_SEQUENCE_START_EVENT:
				CLogger::mainlog->debug("Config: Start Sequence");
				if (key == false) {
					switch (stack.back()->mType) {
						case EConfType::None:
							stack.back()->mType = EConfType::List;
							stack.back()->mData.mpList = new std::vector<CConf*>();
							break;
						case EConfType::List:
						{
							CConf* c = new CConf();
							c->mType = EConfType::List;
							c->mData.mpList = new std::vector<CConf*>();
							stack.back()->mData.mpList->push_back(c);
							stack.push_back(c);
							break;
						}
						case EConfType::Map:
		   					CLogger::mainlog->error("Config: Offset %d: Expected key, found sequence (%d)", pParser->offset, __LINE__);
							error = true;
							break;
						case EConfType::String:
		   					CLogger::mainlog->error("Config: Offset %d: Expected key, found sequence, right after string (%d)", pParser->offset, __LINE__);
							error = true;
							break;
						default:
							error = true;
							break;
					}
					if (error == true) {
						break;
					}
					
				} else {
		   			CLogger::mainlog->error("Config: Offset %d: Expected key, found sequence (%d)", pParser->offset, __LINE__);
					error = true;
					break;
				}
				break;
			case YAML_SEQUENCE_END_EVENT:
				CLogger::mainlog->debug("Config: End Sequence");
				if (key == false) {
					switch(stack.back()->mType) {
						case EConfType::List:
							stack.pop_back();
							switch(stack.back()->mType) {
								case EConfType::Map:
									key = true;
									break;
								case EConfType::List:
									break;
								case EConfType::None:
		   							CLogger::mainlog->error("Config: Offset %d: Found sequence end, wrong stack (%d)", pParser->offset, __LINE__);
									error = true;
									break;
								case EConfType::String:
		   							CLogger::mainlog->error("Config: Offset %d: Found sequence end, wrong stack (%d)", pParser->offset, __LINE__);
									error = true;
									break;
								default:
									error = true;
									break;
							}
							break;
						case EConfType::Map:
		   					CLogger::mainlog->error("Config: Offset %d: Found sequence end, expected.mpMap end (%d)", pParser->offset, __LINE__);
							error = true;
							break;
						case EConfType::String:
		   					CLogger::mainlog->error("Config: Offset %d: Found sequence end after string (%d)", pParser->offset, __LINE__);
							error = true;
							break;
						case EConfType::None:
		   					CLogger::mainlog->error("Config: Offset %d: Found sequence end, wrong stack (%d)", pParser->offset, __LINE__);
							error = true;
							break;
						default:
							error = true;
							break;
					}
				} else {
		   			CLogger::mainlog->error("Config: Offset %d: Found sequence end, expected key (%d)", pParser->offset, __LINE__);
					error = true;
					break;
				}
				break;
			case YAML_MAPPING_START_EVENT:
				CLogger::mainlog->debug("Config: Start Mapping");
				if (key == false) {
					switch(stack.back()->mType) {
						case EConfType::None:
							stack.back()->mType = EConfType::Map;
							stack.back()->mData.mpMap = new std::map<std::string, CConf*>();
							break;
						case EConfType::List:
						{
							CConf* c = new CConf();
							c->mType = EConfType::Map;
							c->mData.mpMap = new std::map<std::string, CConf*>();
							stack.back()->mData.mpList->push_back(c);
							stack.push_back(c);
							break;
						}
						case EConfType::Map:
		   					CLogger::mainlog->error("Config: Offset %d: Found.mpMap start, expected key or.mpMap end (%d)", pParser->offset, __LINE__);
							error = true;
							break;
						case EConfType::String:
		   					CLogger::mainlog->error("Config: Offset %d: Found.mpMap start after string (%d)", pParser->offset, __LINE__);
							error = true;
							break;
						default:
							error = true;
							break;
					}
					if (error == true) {
						break;
					}
					key = true;
				} else {
		   			CLogger::mainlog->error("Config: Offset %d: Found.mpMap start, expected key (%d)", pParser->offset, __LINE__);
					error = true;
					break;
				}
				break;
			case YAML_MAPPING_END_EVENT:
				CLogger::mainlog->debug("Config: End Mapping");
				if (key == true) {
					stack.pop_back();
					if (stack.empty() == true) {
						break;
					}
					switch (stack.back()->mType) {
						case EConfType::List:
							key = false;
							break;
						case EConfType::Map:
							break;
						case EConfType::None:
		   					CLogger::mainlog->error("Config: Offset %d: Found.mpMap end, expected value (%d)", pParser->offset, __LINE__);
							error = true;
							break;
						case EConfType::String:
		   					CLogger::mainlog->error("Config: Offset %d: Found.mpMap end after string (%d)", pParser->offset, __LINE__);
							error = true;
							break;
						default:
							error = true;
							break;
					}
				} else {
		   			CLogger::mainlog->error("Config: Offset %d: Found.mpMap end, expected value (%d)", pParser->offset, __LINE__);
					error = true;
					break;
				}
				break;
			/* Data */
			case YAML_ALIAS_EVENT:  return -1; break;
			case YAML_SCALAR_EVENT:
				CLogger::mainlog->debug("Config: Scalar value (%s) %s %s", event.data.scalar.value, event.data.scalar.anchor, event.data.scalar.tag); 
				if (key == true) {
					CConf* c = new CConf();
					std::string* s = new std::string(reinterpret_cast<const char*>(event.data.scalar.value));
					switch (stack.back()->mType) {
						case EConfType::None:
						case EConfType::List:
						case EConfType::String:
						default:
		   					CLogger::mainlog->error("Config: Offset %d: Found scalar value, expected key (%d)", pParser->offset, __LINE__);
							delete c;
							delete s;
							error = true;
							break;
						case EConfType::Map:
							(*stack.back()->mData.mpMap)[*s] = c;
							delete s;
							break;
					}
					if (error == true) {
						break;
					}
					stack.push_back(c);
					key = false;
				} else {
					std::string* s = new std::string(reinterpret_cast<const char*>(event.data.scalar.value));
					switch (stack.back()->mType) {
						case EConfType::None:
							stack.back()->mType = EConfType::String;
							stack.back()->mData.mpString = s;
							stack.pop_back();
							switch(stack.back()->mType) {
								case EConfType::Map:
									key = true;
									break;
								case EConfType::None:
		   							CLogger::mainlog->error("Config: Offset %d: Found scalar value, wrong stack (%d)", pParser->offset, __LINE__);
								case EConfType::String:
		   							CLogger::mainlog->error("Config: Offset %d: Found scalar value after string (%d)", pParser->offset, __LINE__);
								default:
									// string will be deleted on stack cleanup
									error = true;
									break;
								case EConfType::List:
									break;
							}
							break;
						case EConfType::List:
						{
							CConf* c = new CConf();
							c->mType = EConfType::String;
							c->mData.mpString = s;
							stack.back()->mData.mpList->push_back(c);
							break;
						}
						case EConfType::String:
						case EConfType::Map:
						default:
							delete s;
		   					CLogger::mainlog->error("Config: Offset %d: Found scalar value (%d)", pParser->offset, __LINE__);
							error = true;
							break;
					}
				}
				break;
		}
		if (error == true) {
			break;
		}
		if(event.type != YAML_STREAM_END_EVENT)
			yaml_event_delete(&event);
	} while(event.type != YAML_STREAM_END_EVENT);

	if (error == true) {
		// in case of error clean mData structure and return
		// recursive delete will delete all YAML entries
		delete root;
		// stack is deleted automatically
		CLogger::mainlog->debug("Config: Config parsing failed");
		return -1;
	}


	*pResult = root;

	CLogger::mainlog->debug("Config: Config parsing finished");

	return 0;

}


int CConf::getUint64(char* name, uint64_t* dest){

	if (EConfType::Map != mType) {
		return -1;
	}

	const char* str = 0;
	std::map<std::string, CConf*>::iterator it = mData.mpMap->find(std::string(name));
	if (it != mData.mpMap->end()) {
		CConf* str_obj = it->second;
		if (str_obj != 0 && str_obj->mType == EConfType::String) {
			str = str_obj->mData.mpString->c_str();
		}
	}

	if (str == 0) {
		return -1;
	}

	char* endptr = 0;
	long long int res = strtoll(str, &endptr, 10);
	if (*str != 0 && *endptr == 0) {
		*dest = (uint64_t)res;
	} else {
		return -1;
	}
	return 0;
}

int CConf::getDouble(char* name, double* dest){

	if (EConfType::Map != mType) {
		return -1;
	}

	const char* str = 0;
	std::map<std::string, CConf*>::iterator it = mData.mpMap->find(std::string(name));
	if (it != mData.mpMap->end()) {
		CConf* str_obj = it->second;
		if (str_obj != 0 && str_obj->mType == EConfType::String) {
			str = str_obj->mData.mpString->c_str();
		}
	}

	if (str == 0) {
		return -1;
	}

	char* endptr = 0;
	double res = strtod(str, &endptr);
	if (*str != 0 && *endptr == 0) {
		*dest = (double)res;
	} else {
		return -1;
	}
	return 0;
}


int CConf::getString(char* name, std::string** dest){

	if (EConfType::Map != mType) {
		return -1;
	}

	std::string* str = 0;
	std::map<std::string, CConf*>::iterator it = mData.mpMap->find(std::string(name));
	if (it != mData.mpMap->end()) {
		CConf* str_obj = it->second;
		if (str_obj != 0 && str_obj->mType == EConfType::String) {
			str = str_obj->mData.mpString;
		}
	}

	if (str == 0) {
		return -1;
	}

	*dest = str;
	return 0;
}

int CConf::getBool(char* name, bool* dest){

	if (EConfType::Map != mType) {
		return -1;
	}

	const char* str = 0;
	std::map<std::string, CConf*>::iterator it = mData.mpMap->find(std::string(name));
	if (it != mData.mpMap->end()) {
		CConf* str_obj = it->second;
		if (str_obj != 0 && str_obj->mType == EConfType::String) {
			str = str_obj->mData.mpString->c_str();
		}
	}

	if (str == 0) {
		return -1;
	}

	if (strcasecmp("true", str) == 0) {
		*dest = true;
		return 0;
	} else
	if (strcasecmp("false", str) == 0) {
		*dest = false;
		return 0;
	}

	return -1;
}

int CConf::getConf(char* name, CConf** dest){

	if (EConfType::Map != mType) {
		return -1;
	}

	CConf* conf = 0;
	std::map<std::string, CConf*>::iterator it = mData.mpMap->find(std::string(name));
	if (it != mData.mpMap->end()) {
		CConf* obj = it->second;
		if (obj != 0) {
			conf = obj;
		}
	}

	if (conf == 0) {
		return -1;
	}

	*dest = conf;
	return 0;
}

int CConf::getList(char* name, std::vector<CConf*>** dest){

	if (EConfType::Map != mType) {
		return -1;
	}

	CConf* conf = 0;
	std::map<std::string, CConf*>::iterator it = mData.mpMap->find(std::string(name));
	if (it != mData.mpMap->end()) {
		CConf* obj = it->second;
		if (obj != 0) {
			conf = obj;
		}
	}

	if (conf == 0) {
		return -1;
	}

	if (EConfType::List != conf->mType) {
		return -1;
	}

	*dest = conf->mData.mpList;
	return 0;
}


CConf::~CConf(){
	switch (mType) {
		case EConfType::Map:
			if (mData.mpMap != 0) {
				std::map<std::string, CConf*>::iterator it;
				it = mData.mpMap->begin();
				while (it != mData.mpMap->end()) {
					delete it->second;
					it++;
				}
				mData.mpMap->clear();
				delete mData.mpMap;
				mData.mpMap = 0;
			}
			break;
		case EConfType::List:
			if (mData.mpList != 0) {
				while (mData.mpList->empty() == false) {
					delete mData.mpList->back();
					mData.mpList->pop_back();
				}
				delete mData.mpList;
				mData.mpList = 0;
			}
			break;
		case EConfType::String:
			if (mData.mpString != 0) {
				delete mData.mpString;
				mData.mpString = 0;
			}
			break;
		case EConfType::None:
		default:
			break;
	}
	mType = EConfType::None;
}
