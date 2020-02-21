// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#include <typeinfo>
#include <sys/socket.h>
#include <cstring>
#include <unistd.h>
#include <cstdio>
#include <fcntl.h>

#include "CComUnixSchedClient.h"
#include "CLogger.h"
#include "CTaskWrapper.h"
#include "CTaskDatabase.h"
#include "CScheduleComputer.h"
#include "CResource.h"
#include "CComUnixServer.h"

using namespace sched::com;


CComUnixSchedClient::CComUnixSchedClient(
		CComUnixServer& rServer,
		std::vector<CResource*>& rResources,
		CTaskDatabase& rTaskDatabase,
		int socket) :
	CComUnixClient(socket),
	CComUnixWriter(rServer),
	mrServer(rServer),
	mrResources(rResources),
	mrTaskDatabase(rTaskDatabase)
{
	mBuffer = new char[1025];
	memset(mBuffer, 0, 1025*sizeof(char));
}

CComUnixSchedClient::~CComUnixSchedClient(){
	if (mClientClosed == 0) {
		writeQuit();
	}
	mWriteMutex.lock();
	if (mSocket != -1) {
		close(mSocket);
		mSocket = -1;
	}
	if (mBuffer != 0) {
		delete[] mBuffer;
		mBuffer = 0;
	}
	mWriteMutex.unlock();
}

void CComUnixSchedClient::incBuffer(){
	char* newbuffer = new char[mBufferMax*2 + 1];
	memset(newbuffer, 0, (mBufferMax*2 + 1)*sizeof(char));
	memcpy(newbuffer, mBuffer, mBufferPos*sizeof(char));
	delete[] mBuffer;
	mBuffer = newbuffer;
	mBufferMax *= 2;
}


void CComUnixSchedClient::writeStart(CResource& resource, int targetProgress, ETaskOnEnd onEnd, CTaskWrapper& task) {

	const char* resName = resource.mName.c_str();
	int taskId = task.mId;
	CLogger::mainlog->debug("UnixClient: start task %d on resource %s", taskId, resName);

	switch (mProtocol) {
		case 1:
			cJSON* obj = cJSON_CreateObject();
			cJSON* msg = cJSON_CreateString("TASK_START");
			cJSON_AddItemToObjectCS(obj, "msg", msg);
			cJSON* id = cJSON_CreateNumber(taskId);
			cJSON_AddItemToObjectCS(obj, "id", id);
			cJSON* res = cJSON_CreateString(resName);
			cJSON_AddItemToObjectCS(obj, "resource", res);
			if (targetProgress > 0) {
				cJSON* tprog = cJSON_CreateNumber(targetProgress);
				cJSON_AddItemToObjectCS(obj, "endprogress", tprog);
				cJSON* onend;
				switch (onEnd) {
					case ETaskOnEnd::TASK_ONEND_SUSPENDS:
						onend = cJSON_CreateString("suspend");
					break;
					case ETaskOnEnd::TASK_ONEND_CONTINUES:
						onend = cJSON_CreateString("continue");
					break;
				}
				cJSON_AddItemToObjectCS(obj, "onend", onend);
			}
			int ret = write1(obj, false);
			if (ret < 0) {
				mrTaskDatabase.abortTask(&task);
				mClientClosed = 1;
			}
		break;
	}
}


void CComUnixSchedClient::writeAbort(CTaskWrapper& task) {
	int taskId = task.mId;
	CLogger::mainlog->debug("UnixClient: abort task %d", taskId);

	switch(mProtocol) {
		case 1:
			cJSON* obj = cJSON_CreateObject();
			cJSON* msg = cJSON_CreateString("TASK_ABORT");
			cJSON_AddItemToObjectCS(obj, "msg", msg);
			cJSON* id = cJSON_CreateNumber(taskId);
			cJSON_AddItemToObjectCS(obj, "id", id);
			int ret = write1(obj, false);
			if (ret < 0) {
				mrTaskDatabase.abortTask(&task);
				// invalid write in case the object is already deleted
				mClientClosed = 1;
			}
		break;
	}
}


void CComUnixSchedClient::writeProgress(CTaskWrapper& task) {
	int taskId = task.mId;
	CLogger::mainlog->debug("UnixClient: progress task %d", taskId);

	switch(mProtocol) {
		case 1:
			cJSON* obj = cJSON_CreateObject();
			cJSON* msg = cJSON_CreateString("TASK_PROGRESS");
			cJSON_AddItemToObjectCS(obj, "msg", msg);
			cJSON* id = cJSON_CreateNumber(taskId);
			cJSON_AddItemToObjectCS(obj, "id", id);
			int ret = write1(obj, false);
			if (ret < 0) {
				mrTaskDatabase.abortTask(&task);
				mClientClosed = 1;
			}
		break;
	}
}


void CComUnixSchedClient::writeSuspend(CTaskWrapper& task){
	int taskId = task.mId;
	CLogger::mainlog->debug("UnixClient: suspend task %d", taskId);

	switch(mProtocol) {
		case 1:
			cJSON* obj = cJSON_CreateObject();
			cJSON* msg = cJSON_CreateString("TASK_SUSPEND");
			cJSON_AddItemToObjectCS(obj, "msg", msg);
			cJSON* id = cJSON_CreateNumber(taskId);
			cJSON_AddItemToObjectCS(obj, "id", id);
			int ret = write1(obj, false);
			if (ret < 0) {
				mrTaskDatabase.abortTask(&task);
				mClientClosed = 1;
			}
		break;
	}
}


void CComUnixSchedClient::writeTaskids(int* taskid_list) {

	int num = 0;
	for (int ix=0; taskid_list[ix] != -1; ix++) {
		num = ix+1;
	}

	cJSON* taskids = cJSON_CreateObject();
	cJSON* msg = cJSON_CreateString("TASKIDS");
	cJSON_AddItemToObjectCS(taskids, "msg", msg);
	cJSON* arr = cJSON_CreateIntArray(taskid_list, num);
	cJSON_AddItemToObjectCS(taskids, "taskids", arr);
	int ret = write1(taskids, false); // json deleted by write
	if (ret < 0) {
		// next poll should clean this mess up
		for (unsigned int i=0; taskid_list[i]!=-1; i++) {

			CTaskWrapper* task = mrTaskDatabase.getTaskWrapper(taskid_list[i]);
			if (task != 0) {
				task->aborted();
			}

		}
		mClientClosed = 1;
	}

}

void CComUnixSchedClient::writeQuit() {

	cJSON* obj = cJSON_CreateObject();
	cJSON* msg = cJSON_CreateString("QUIT");
	cJSON_AddItemToObjectCS(obj, "msg", msg);
	write1(obj, true);
	mClientClosed = 1;

}

int CComUnixSchedClient::initClient(){

	CLogger::mainlog->debug("UnixClient initClient");

	int count;
	count = recv(this->mSocket, this->mBuffer, CComUnixSchedClient::MAX_BUFFER, 0);

	if (count == -1) {
		int error = errno;
		CLogger::mainlog->debug("CComUnixClient read error from socket %d %s", this->mSocket, strerror(error));
		mClientClosed = 1;
		return -1;
	}
	if (count == 0) {
		mClientQuit = 1;
	}
	mBufferPos += count;

	// Check for first protocol version
	// "SET_PID:%u;SET_TASK:%u;SET_SIZE:%u;"
	if (mBuffer[0] == 'S') {
		mProtocol = 0;
		return 0;
	}

	// Check protocol version
	// "PROTOCOL=%u" 0x00
	char* end = index(mBuffer, 0x00);
	// Search for null byte
	if (end == 0 || end >= &(mBuffer[CComUnixSchedClient::MAX_BUFFER])) {
		// null byte not found, aborting
		CLogger::mainlog->debug("UnixClient null byte not found");
		return -1;
	}
	char* endptr = 0;
	long int version = strtol(&(mBuffer[9]), &endptr, 10);
	if (endptr != 0 && *endptr != 0) {
		// invalid string, no number
		CLogger::mainlog->debug("UnixClient invalid string %x %x", mBuffer, endptr);
		int error = errno;
		CLogger::mainlog->debug("UnixClient error %s", strerror(error));
		CLogger::mainlog->debug("UnixClient %d",version);
		return -1;
	}
	if (version < 1) {
		// invalid version
		CLogger::mainlog->debug("UnixClient invalid version");
		return -1;
	}
	this->mProtocol = version;

	// calculate additional data
	mBufferPos -= (&end[1]-mBuffer)/sizeof(char);
	if (mBufferPos > 0) {
		// advance buffer, data could overlap so do two copies
		char* buff = new char[mBufferPos];
		memcpy(buff, &end[1], mBufferPos);
		memcpy(mBuffer, buff, mBufferPos);
		delete[] buff;
	}

	// set socket to non-blocking
	int flags = fcntl(mSocket, F_GETFL, 0);
	if (flags == -1) {
		CLogger::mainlog->error("UnixClient unable to set socket to non-blocking");
		return -1;
	}
	int ret = fcntl(mSocket, F_SETFL, flags | O_NONBLOCK);
	if (ret == -1) {
		CLogger::mainlog->error("UnixClient unable to set socket to non-blocking");
		return -1;
	}

	CLogger::mainlog->debug("UnixClient protocol version %d", mProtocol);

	return 0;
}

int CComUnixSchedClient::read(){

	if (mProtocol < 0) {
		if (initClient() == -1) {
			return -1;
		}
	}

	int ret = 0;
	switch(this->mProtocol) {
		case 0:
			ret = readVer0();
			break;
		case 1:
			ret = readVer1();
			break;
		default:
			ret = initClient();
	}
	if (mClientQuit == 1) {
		mrServer.removeClient(this);
	}
	return ret;
}

int CComUnixSchedClient::readVer0(){
	return 0;
}
int CComUnixSchedClient::readVer1(){

	bool newData = true;
	int ret = 0;
	int error = 0;
	char* next = 0;
	char* currentPos = 0;

	while(newData == true) {

		CLogger::mainlog->debug("UnixClient start");
		if (mBufferPos == mBufferMax) {
			CLogger::mainlog->debug("UnixClient buffer reached max");
			if (mBufferMax == 4096)	{
				// broken client
				CLogger::mainlog->error("UnixClient buffer too large, broken client");
				return -1;
			}
			// enlarge buffer
			incBuffer();
		}

		CLogger::mainlog->debug("UnixClient read new data");
		// read new data
		newData = false;
		ret = recv(mSocket, &(mBuffer[mBufferPos]), mBufferMax-mBufferPos, 0);
		CLogger::mainlog->debug("UnixClient recv %d", ret);
		if (ret == -1) {
			error = errno;
			// other error than EWOULDBLOCK or EAGAIN
			if (error != EWOULDBLOCK && error != EAGAIN) {
				CLogger::mainlog->error("UnixClient socket read failure %s", strerror(error));
				break;
			} else {
				CLogger::mainlog->debug("UnixClient socket read failure %d %s", error, strerror(error));
			}
		}
		if (ret > 0) {
			mBufferPos += ret;
			newData = true;
		}
		if (ret == 0) {
			CLogger::mainlog->debug("UnixClient read 0");
			mClientClosed = 1;
		}

		// is there something to parse?
		if (mBufferPos == 0) {
			CLogger::mainlog->debug("UnixClient nothing to parse");
			break;
		}
		CLogger::mainlog->debug("UnixClient going to parse");
		// parse messages in buffer
		currentPos = mBuffer;
		do {
			next = index(currentPos, 0x00);
			if (next == 0 || next == &(mBuffer[mBufferMax])) {
				if (mBufferPos == mBufferMax) {
					// not enough buffer
					break;
				}
				// data incomplete, wait for more
				CLogger::mainlog->debug("UnixClient incomplete data");
				break;
			}
			if (next > &(mBuffer[mBufferPos])) {
				// delimiter outside buffer range
				CLogger::mainlog->debug("UnixClient delimiter outside buffer");
				break;
			}
			// new message
			CLogger::mainlog->debug("< %s", currentPos);
			cJSON* json = cJSON_Parse(currentPos);
			if (json != 0) {
				// debug output
				//char* string = cJSON_Print(json);
				//CLogger::mainlog->debug("UnixClient Message: %s", string);
				//free(string);
				// process message
				processVer1(json);
				cJSON_Delete(json);
			} else {
				CLogger::mainlog->error("UnixClient json error");
				const char* errptr = cJSON_GetErrorPtr();
				if (errptr != 0) {
					CLogger::mainlog->debug("UnixClient json error pos %d", (errptr-mBuffer)/sizeof(char));
				}
				break;
			}
			currentPos = &next[1];
		} while(currentPos < &(mBuffer[mBufferPos]));

		// remove read data
		int unread = (&mBuffer[mBufferPos]-currentPos)/sizeof(char);
		if (unread == 0) {
			mBufferPos = 0;
		} else
		if (unread != mBufferPos) {
			// copy lingering data
			char* unreadData = new char[unread];
			memcpy(unreadData, currentPos, unread);
			memcpy(mBuffer, unreadData, unread);
			mBufferPos = unread;
			delete [] unreadData;
		} // else do nothing
	}

	return 0;
}

void CComUnixSchedClient::processVer1(cJSON* json){
	if (json == 0) {
		CLogger::mainlog->error("UnixClient: processVer1 got null pointer");
		return;
	}

	if (cJSON_IsObject(json) == 0) {
		CLogger::mainlog->error("UnixClient: json is not an object");
		return;
	}

	cJSON* msg_obj = cJSON_GetObjectItemCaseSensitive(json, "msg");
	if (msg_obj == 0) {
		CLogger::mainlog->error("UnixClient: \"msg\" not found");
		return;
	}
	if (msg_obj->type != cJSON_String) {
		CLogger::mainlog->error("UnixClient: \"msg\" is not a string");
		return;
	}
	char* msg = msg_obj->valuestring;
	if (strcmp(msg, "TASKLIST") == 0) {

		cJSON* list_obj = cJSON_GetObjectItemCaseSensitive(json, "tasklist");
		if (list_obj == 0) {
			return;
		}
		// Iterate tasks
		cJSON* chain = list_obj->child;
		std::vector<CTaskWrapper*> list;
		int error = 0;
		int index = -1;
		while (chain != 0) {
			
			index++;

			// name			
			cJSON* name_obj = cJSON_GetObjectItemCaseSensitive(chain, "name");
			if (name_obj == 0 || name_obj->type != cJSON_String) {
				CLogger::mainlog->error("UnixClient: tasklist obj: task misses name");
				error = 1;
				break;
			}
			std::string* name = new std::string(name_obj->valuestring);

			// size
			cJSON* size_obj = cJSON_GetObjectItemCaseSensitive(chain, "size");
			if (size_obj == 0 || size_obj->type != cJSON_Number) {
				CLogger::mainlog->error("UnixClient: tasklist obj: task misses size");
				delete name;
				name = 0;
				error = 1;
				break;
			}
			unsigned long size = size_obj->valueint;

			// checkpoints
			cJSON* checkp_obj = cJSON_GetObjectItemCaseSensitive(chain, "checkpoints");
			if (checkp_obj == 0 || checkp_obj->type != cJSON_Number) {
				CLogger::mainlog->error("UnixClient: tasklist obj: task misses checkpoints");
				delete name;
				name = 0;
				error = 1;
				break;
			}
			unsigned long checkpoints = checkp_obj->valueint;

			// resources
			cJSON* res_arr = cJSON_GetObjectItemCaseSensitive(chain, "resources");
			if (res_arr == 0 || res_arr->type != cJSON_Array) {
				CLogger::mainlog->error("UnixClient: tasklist obj: task misses resources");
				delete name;
				name = 0;
				error = 1;
				break;
			}
			int res_num = cJSON_GetArraySize(res_arr);
			if (res_num < 1) {
				CLogger::mainlog->error("UnixClient: tasklist obj: task has empty resources array");
				delete name;
				name = 0;
				error = 1;
				break;
			}
			std::vector<CResource*>* resources = new std::vector<CResource*>();
			for (int ri=0; ri<res_num; ri++) {
				cJSON* res_obj = cJSON_GetArrayItem(res_arr, ri);
				if (res_obj == 0 || res_obj->type != cJSON_String) {
					CLogger::mainlog->error("UnixClient: tasklist obj: task has invalid resource array entry");
					error = 1;
					break;
					
				}
				char* res_name = res_obj->valuestring;
				int res_id = -1;
				for (unsigned int rj=0; rj<mrResources.size(); rj++) {
					if (strcmp(mrResources[rj]->mName.c_str(), res_name) == 0) {
						res_id = rj;
						break;
					}
				}
				if (res_id == -1) {
					CLogger::mainlog->warn("UnixClient: tasklist obj: task has unknown resource array entry %s", res_name);
					// Ignore unknown resource, use resources that are known or
					// if there are no resources then abort later
					continue;
				}
				resources->push_back(mrResources[res_id]);
			}
			if (error == 1 || resources->size() == 0) {
				CLogger::mainlog->error("UnixClient: tasklist obj: task has no valid resources");
				delete name;
				name = 0;
				delete resources;
				resources = 0;
				break;
			}

			// dependencies (optional)
			int dep_num = 0;
			int* deplist = 0;
			cJSON* dep_obj = cJSON_GetObjectItemCaseSensitive(chain, "dependencies");
			if (dep_obj != 0 && dep_obj->type == cJSON_Array) {
				dep_num = cJSON_GetArraySize(dep_obj);
				if (dep_num != 0) { // ignore empty list
					deplist = new int[dep_num]; 
					cJSON* dep_val = dep_obj->child;
					int depindex = 0;
					while(dep_val != 0){
						if (dep_val->type != cJSON_Number) {
							CLogger::mainlog->error("UnixClient: tasklist obj: dependencies list entry is not a number");
							error = 1;
							break;
						}
						if (dep_val->valueint < 0 ||
							dep_val->valueint >= index) {
							CLogger::mainlog->error("UnixClient: tasklist obj: invalid dependency: index %d for task with index %d", dep_val->valueint, index);
							error = 1;
							break;
						}
						deplist[depindex] = dep_val->valueint;
						depindex++;
						dep_val = dep_val->next;
					}
					if (error == 1) {
						break;
					}
				}
			}
			if (error == 1) {
				if (deplist != 0) {
					delete [] deplist;
					deplist = 0;
					dep_num = 0;
				}
				delete name;
				name = 0;
				delete resources;
				resources = 0;
				break;
			}
		
			CTaskWrapper* taskwrap = new CTaskWrapper(name, size, checkpoints, resources, deplist, dep_num, this, mrTaskDatabase);
			list.push_back(taskwrap);
			chain = chain->next;
		}
		
		if (error == 1) {
			while (list.empty() == false) {
				delete list.back();
				list.pop_back();
			}
			return;
		}

		onTasklist(&list);
	}

	if (strcmp(msg, "TASK_STARTED") == 0) {
		
		cJSON* id_obj = cJSON_GetObjectItemCaseSensitive(json, "id");
		
		if (id_obj == 0 || id_obj->type != cJSON_Number) {
			return;
		}
		int taskId = id_obj->valueint;

		onStarted(taskId);
	}

	if (strcmp(msg, "TASK_SUSPENDED") == 0) {
		
		cJSON* id_obj = cJSON_GetObjectItemCaseSensitive(json, "id");
		
		if (id_obj == 0 || id_obj->type != cJSON_Number) {
			return;
		}
		int taskId = id_obj->valueint;

		cJSON* pr_obj = cJSON_GetObjectItemCaseSensitive(json, "progress");
		
		if (pr_obj == 0 || pr_obj->type != cJSON_Number) {
			return;
		}
		int progress = pr_obj->valueint;

		onSuspended(taskId, progress);
	}


	if (strcmp(msg, "TASK_FINISHED") == 0) {
		
		cJSON* id_obj = cJSON_GetObjectItemCaseSensitive(json, "id");
		
		if (id_obj == 0 || id_obj->type != cJSON_Number) {
			return;
		}
		int taskId = id_obj->valueint;

		onFinished(taskId);
	}

	if (strcmp(msg, "PROGRESS") == 0) {
		cJSON* id_obj = cJSON_GetObjectItemCaseSensitive(json, "id");
		
		if (id_obj == 0 || id_obj->type != cJSON_Number) {
			return;
		}
		int taskId = id_obj->valueint;

		cJSON* pr_obj = cJSON_GetObjectItemCaseSensitive(json, "progress");
		
		if (pr_obj == 0 || pr_obj->type != cJSON_Number) {
			return;
		}
		int progress = pr_obj->valueint;

		onProgress(taskId, progress);
	}
	
	if (strcmp(msg, "QUIT") == 0) {
		mClientQuit = 1;
		mClientClosed = 1;
		onQuit();
	}
}

int CComUnixSchedClient::write1(cJSON* json, bool quit) {
	mWriteMutex.lock();

	char* buff = cJSON_PrintUnformatted(json);
	CLogger::mainlog->debug("> %s", buff);
	long len = strlen(buff) + 1;
	long writtentotal = 0;
	long written = 0;
	while (writtentotal < len) {
		written = send(this->mSocket, buff, len-writtentotal, MSG_NOSIGNAL);
		CLogger::mainlog->debug("%d written", written);
		if (written < 0) {
			if (quit == false) {
				CLogger::mainlog->error("UnixClient: write error %s", strerror(errno));
			} else {
				CLogger::mainlog->debug("UnixClient: write error %s", strerror(errno));
			}
			break;
		}
		writtentotal += written;
	}
	fsync(this->mSocket);
	cJSON_free(buff);
	cJSON_Delete(json);	

	mWriteMutex.unlock();
	if (written != len) {
		return -1;
	}
	return 0;
}

void CComUnixSchedClient::writeMessage(CComUnixWriteMessage* message) {

	if (message == 0) {
		
		CLogger::mainlog->debug("UnixSchedClient: Writer: message is 0");
		return;
	}

	CComSchedMessage* schedMsg = 0;

	schedMsg = dynamic_cast<CComSchedMessage*> (message);
	//schedMsg = (CComSchedMessage*) message;
	if (schedMsg == 0) {
		CLogger::mainlog->debug("UnixSchedClient: Writer: unknown message type: %s", typeid(*message).name());
		return;
	}
/*
	if (typeid(*message) != typeid(CComSchedMessage)) {
		CLogger::mainlog->error("UnixSchedClient: Writer: unknown message type: %s", typeid(*message).name());
		return;
	}
	schedMsg = (CComSchedMessage*) message;
*/
	if (schedMsg == 0) {
		CLogger::mainlog->debug("UnixSchedClient: Writer: unknown message type: %s", typeid(*message).name());
		return;
	}

	switch (schedMsg->type) {
		case EComSchedMessageType::START:
			writeStart(*schedMsg->resource, schedMsg->targetProgress, schedMsg->onEnd, *schedMsg->task);
		break;
		case EComSchedMessageType::SUSPEND:
			writeSuspend(*schedMsg->task);
		break;
		case EComSchedMessageType::ABORT:
			writeAbort(*schedMsg->task);
		break;
		case EComSchedMessageType::PROGRESS:
			writeProgress(*schedMsg->task);
		break;
		case EComSchedMessageType::TASKIDS:
			writeTaskids(schedMsg->taskids);
		break;
		default:
			CLogger::mainlog->debug("UnixSchedClient: Writer: unknown message");
		break;
	}

}


