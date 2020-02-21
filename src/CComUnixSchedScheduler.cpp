// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#include <typeinfo>
#include <sys/socket.h>
#include <cstring>
#include <unistd.h>
#include <cstdio>
#include <fcntl.h>

#include "CComUnixSchedScheduler.h"
#include "CLogger.h"
#include "CTaskWrapper.h"
#include "CTaskDatabase.h"
#include "CScheduleComputer.h"
#include "CResource.h"
#include "CComUnixServer.h"

using namespace sched::com;


CComUnixSchedScheduler::CComUnixSchedScheduler(
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

CComUnixSchedScheduler::~CComUnixSchedScheduler(){
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

void CComUnixSchedScheduler::incBuffer(){
	char* newbuffer = new char[mBufferMax*2 + 1];
	memset(newbuffer, 0, (mBufferMax*2 + 1)*sizeof(char));
	memcpy(newbuffer, mBuffer, mBufferPos*sizeof(char));
	delete[] mBuffer;
	mBuffer = newbuffer;
	mBufferMax *= 2;
}

void CComUnixSchedScheduler::writeStarted(int taskid) {

	CLogger::mainlog->debug("UnixClient: task started %d", taskid);

	switch(mProtocol) {
		case 1:
			cJSON* obj = cJSON_CreateObject();
			cJSON* msg = cJSON_CreateString("TASK_STARTED");
			cJSON_AddItemToObjectCS(obj, "msg", msg);
			cJSON* id = cJSON_CreateNumber(taskid);
			cJSON_AddItemToObjectCS(obj, "id", id);
			int ret = write1(obj);
			if (ret < 0) {
				//mrTaskDatabase.abortTask(&task);
				mClientClosed = 1;
			}
		break;
	}
}

void CComUnixSchedScheduler::writeSuspended(int taskid, int progress) {

	CLogger::mainlog->debug("UnixClient: task suspended %d %d", taskid, progress);

	switch(mProtocol) {
		case 1:
			cJSON* obj = cJSON_CreateObject();
			cJSON* msg = cJSON_CreateString("TASK_SUSPENDED");
			cJSON_AddItemToObjectCS(obj, "msg", msg);
			cJSON* id = cJSON_CreateNumber(taskid);
			cJSON_AddItemToObjectCS(obj, "id", id);
			cJSON* progress_obj = cJSON_CreateNumber(progress);
			cJSON_AddItemToObjectCS(obj, "progress", progress_obj);
			int ret = write1(obj);
			if (ret < 0) {
				//mrTaskDatabase.abortTask(&task);
				mClientClosed = 1;
			}
		break;
	}
}

void CComUnixSchedScheduler::writeFinished(int taskid) {

	CLogger::mainlog->debug("UnixClient: task finished %d", taskid);

	switch(mProtocol) {
		case 1:
			cJSON* obj = cJSON_CreateObject();
			cJSON* msg = cJSON_CreateString("TASK_FINISHED");
			cJSON_AddItemToObjectCS(obj, "msg", msg);
			cJSON* id = cJSON_CreateNumber(taskid);
			cJSON_AddItemToObjectCS(obj, "id", id);
			int ret = write1(obj);
			if (ret < 0) {
				//mrTaskDatabase.abortTask(&task);
				mClientClosed = 1;
			}
		break;
	}
}

void CComUnixSchedScheduler::writeProgress(int taskid, int progress) {
	
	CLogger::mainlog->debug("UnixClient: task progress %d %d", taskid, progress);

	switch(mProtocol) {
		case 1:
			cJSON* obj = cJSON_CreateObject();
			cJSON* msg = cJSON_CreateString("PROGRESS");
			cJSON_AddItemToObjectCS(obj, "msg", msg);
			cJSON* id = cJSON_CreateNumber(taskid);
			cJSON_AddItemToObjectCS(obj, "id", id);
			cJSON* progress_obj = cJSON_CreateNumber(progress);
			cJSON_AddItemToObjectCS(obj, "progress", progress_obj);
			int ret = write1(obj);
			if (ret < 0) {
				//mrTaskDatabase.abortTask(&task);
				mClientClosed = 1;
			}
		break;
	}
}

void CComUnixSchedScheduler::writeTasklist(std::vector<CTaskWrapper*>* tasks) {
	CLogger::mainlog->debug("UnixClient: tasklist");

	switch(mProtocol) {
		case 1:
			cJSON* obj = cJSON_CreateObject();
			cJSON* msg = cJSON_CreateString("TASKLIST");
			cJSON_AddItemToObjectCS(obj, "msg", msg);

			int num = tasks->size();

			cJSON* arr = cJSON_CreateArray();
			for (int ix=0; ix<num; ix++) {
				CTaskWrapper* task = (*tasks)[ix];
				cJSON* task_obj = cJSON_CreateObject();

				cJSON* name = cJSON_CreateString(task->mpName->c_str());
				cJSON_AddItemToObjectCS(task_obj, "name", name);

				cJSON* size = cJSON_CreateNumber(task->mSize);
				cJSON_AddItemToObjectCS(task_obj, "size", size);

				cJSON* checkpoints = cJSON_CreateNumber(task->mCheckpoints);
				cJSON_AddItemToObjectCS(task_obj, "checkpoints", checkpoints);

				cJSON* res_arr = cJSON_CreateArray();
				for (unsigned int resix=0; resix<task->mpResources->size(); resix++) {
					cJSON* res_str = cJSON_CreateString((*(task->mpResources))[resix]->mName.c_str());
					cJSON_AddItemToArray(res_arr, res_str);
				}
				cJSON_AddItemToObjectCS(task_obj, "resources", res_arr);

				cJSON* dep_arr = cJSON_CreateArray();
				int dep_num = 0;
				for (int dix=0; dix<task->mPredecessorNum; dix++) {
					// find local id (in task list) for dependency
					int gid = task->mpPredecessorList[dix]; // global id (in taskdb)
					int lid = -1; // local id (in task list)
					// search index (local id) of dependency in task list, dependency are always in front of current task
					for (int rid=0; rid < ix; rid++) {
						if ((*tasks)[rid]->mId == gid) {
							lid = rid;
						}
					}
					if (lid == -1) {
						// not found, skip
						continue;
					}
					cJSON* dep_id = cJSON_CreateNumber(lid);
					cJSON_AddItemToArray(dep_arr, dep_id);
					dep_num++;
				}
				if (dep_num == 0) {
					// no dependencies
					cJSON_Delete(dep_arr);
				} else {
					cJSON_AddItemToObjectCS(task_obj, "dependencies", dep_arr);
				}

				cJSON_AddItemToArray(arr, task_obj);
			}
			cJSON_AddItemToObjectCS(obj, "tasklist", arr);

			int ret = write1(obj);
			if (ret < 0) {
				for (unsigned int ix=0; ix<tasks->size(); ix++) {
					mrTaskDatabase.abortTask((*tasks)[ix]);
				}
				mClientClosed = 1;
			}
		break;
	}
}

void CComUnixSchedScheduler::writeQuit() {

	cJSON* obj = cJSON_CreateObject();
	cJSON* msg = cJSON_CreateString("QUIT");
	cJSON_AddItemToObjectCS(obj, "msg", msg);
	write1(obj);
	mClientClosed = 1;

}

int CComUnixSchedScheduler::initClient(){

	CLogger::mainlog->debug("UnixSchedScheduler initClient");

	// Send protocol version
	// "PROTOCOL=1" 0x00
	int ret = write1((char*)"PROTOCOL=1"); // write1 will add a zero byte
	if (ret == -1) {
		return -1;
	}
	mProtocol = 1;

	// set socket to non-blocking
	int flags = fcntl(mSocket, F_GETFL, 0);
	if (flags == -1) {
		CLogger::mainlog->error("UnixClient unable to set socket to non-blocking");
		return -1;
	}
	ret = fcntl(mSocket, F_SETFL, flags | O_NONBLOCK);
	if (ret == -1) {
		CLogger::mainlog->error("UnixClient unable to set socket to non-blocking");
		return -1;
	}

	CLogger::mainlog->debug("UnixClient protocol version %d", mProtocol);
	return 0;
}

int CComUnixSchedScheduler::read(){

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

int CComUnixSchedScheduler::readVer0(){
	return 0;
}
int CComUnixSchedScheduler::readVer1(){

	bool newData = true;
	int ret = 0;
	int error = 0;
	char* next = 0;
	char* currentPos = 0;

	while(newData == true) {

		if (mBufferPos == mBufferMax) {
			if (mBufferMax == 4096)	{
				// broken client
				CLogger::mainlog->error("UnixSchedScheduler buffer too large, broken client");
				return -1;
			}
			// enlarge buffer
			incBuffer();
		}

		// read new data
		newData = false;
		ret = recv(mSocket, &(mBuffer[mBufferPos]), mBufferMax-mBufferPos, 0);
		if (ret == -1) {
			error = errno;
			// other error than EWOULDBLOCK or EAGAIN
			if (error != EWOULDBLOCK && error != EAGAIN) {
				CLogger::mainlog->error("UnixSchedScheduler socket read failure %s", strerror(error));
				break;
			} else {
				CLogger::mainlog->debug("UnixSchedScheduler socket read failure %d %s", error, strerror(error));
			}
		}
		if (ret > 0) {
			mBufferPos += ret;
			newData = true;
		}
		if (ret == 0) {
			CLogger::mainlog->debug("UnixSchedScheduler read 0");
			mClientClosed = 1;
		}

		// is there something to parse?
		if (mBufferPos == 0) {
			CLogger::mainlog->debug("UnixSchedScheduler nothing to parse");
			break;
		}
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
				CLogger::mainlog->debug("UnixSchedScheduler incomplete data");
				break;
			}
			if (next > &(mBuffer[mBufferPos])) {
				// delimiter outside buffer range
				CLogger::mainlog->debug("UnixSchedScheduler delimiter outside buffer");
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
				CLogger::mainlog->error("UnixSchedScheduler json error");
				const char* errptr = cJSON_GetErrorPtr();
				if (errptr != 0) {
					CLogger::mainlog->debug("UnixSchedScheduler json error pos %d", (errptr-mBuffer)/sizeof(char));
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

void CComUnixSchedScheduler::processVer1(cJSON* json){
	if (json == 0) {
		CLogger::mainlog->error("UnixSchedScheduler: processVer1 got null pointer");
		return;
	}

	if (cJSON_IsObject(json) == 0) {
		CLogger::mainlog->error("UnixSchedScheduler: json is not an object");
		return;
	}

	cJSON* msg_obj = cJSON_GetObjectItemCaseSensitive(json, "msg");
	if (msg_obj == 0) {
		CLogger::mainlog->error("UnixSchedScheduler: \"msg\" not found");
		return;
	}
	if (msg_obj->type != cJSON_String) {
		CLogger::mainlog->error("UnixSchedScheduler: \"msg\" is not a string");
		return;
	}
	char* msg = cJSON_GetStringValue(msg_obj);
	if (strcmp(msg, "TASKIDS") == 0) {
		cJSON* ids_arr = cJSON_GetObjectItemCaseSensitive(json, "taskids");
		if (ids_arr == 0 || cJSON_IsArray(ids_arr) == 0) {
			return;
		}
		int ids_num = cJSON_GetArraySize(ids_arr);
		int* ids = new int[ids_num+1]();
		ids[ids_num] = -1;
		int error = 0;
		// Iterate tasks
		for (int ix=0; ix<ids_num; ix++) {
			cJSON* id_obj = cJSON_GetArrayItem(ids_arr, ix);
			if (id_obj == 0 || cJSON_IsNumber(id_obj) == 0) {
				error = 1;
				break;
			}
			ids[ix] = (int) id_obj->valuedouble;
		}
		if (0 != error) {
			onFail();
		} else {
			onTaskids(ids);
		}
		delete[] ids;
	} else
	if (strcmp(msg, "TASK_START") == 0) {

		// id
		cJSON* id_obj = cJSON_GetObjectItemCaseSensitive(json, "id");
		
		if (id_obj == 0 || id_obj->type != cJSON_Number) {
			CLogger::mainlog->debug("UnixSchedScheduler: TASK_START failed, invalid id");
			onFail();
			return;
		}
		int taskId = id_obj->valueint;

		// resource
		cJSON* res_obj = cJSON_GetObjectItemCaseSensitive(json, "resource");
		
		if (res_obj == 0 || res_obj->type != cJSON_String) {
			CLogger::mainlog->debug("UnixSchedScheduler: TASK_START failed, invalid resource");
			onFail();
			return;
		}

		char* res_str = cJSON_GetStringValue(res_obj);
		CResource* res = 0;
		for (unsigned int ix=0; ix<mrResources.size(); ix++) {
			if (strcmp(mrResources[ix]->mName.c_str(), res_str) == 0) {
				res = mrResources[ix];
				break;
			}
		}
		if (res == 0) {
			CLogger::mainlog->debug("UnixSchedScheduler: TASK_START failed, invalid resource");
			onFail();
			return;
		}

		// endprogress
		int endprogress = -1;
		ETaskOnEnd onend = ETaskOnEnd::TASK_ONEND_SUSPENDS;
		cJSON* end_obj = cJSON_GetObjectItemCaseSensitive(json, "endprogress");
		if (end_obj != 0 && end_obj->type == cJSON_Number) {
			endprogress = (int) end_obj->valuedouble;

			// onend
			cJSON* onend_obj = cJSON_GetObjectItemCaseSensitive(json, "onend");
			
			if (onend_obj == 0 || onend_obj->type != cJSON_String) {
				CLogger::mainlog->debug("UnixSchedScheduler: TASK_START failed, invalid onend");
				onFail();
				return;
			}
			char* onend_str = cJSON_GetStringValue(onend_obj);
			if (strcmp(onend_str, "suspend") == 0) {
				onend = ETaskOnEnd::TASK_ONEND_SUSPENDS;
			} else
			if (strcmp(onend_str, "continue") == 0) {
				onend = ETaskOnEnd::TASK_ONEND_CONTINUES;
			} else {
				CLogger::mainlog->debug("UnixSchedScheduler: TASK_START failed, invalid onend");
				onFail();
				return;
			}
		}

		onStart(taskId, *res, endprogress, onend);

	} else
	if (strcmp(msg, "TASK_SUSPEND") == 0) {

		// id
		cJSON* id_obj = cJSON_GetObjectItemCaseSensitive(json, "id");
		
		if (id_obj == 0 || id_obj->type != cJSON_Number) {
			onFail();
			return;
		}
		int taskId = id_obj->valueint;

		onSuspend(taskId);

	} else
	if (strcmp(msg, "TASK_PROGRESS") == 0) {

		// id
		cJSON* id_obj = cJSON_GetObjectItemCaseSensitive(json, "id");
		
		if (id_obj == 0 || id_obj->type != cJSON_Number) {
			onFail();
			return;
		}
		int taskId = id_obj->valueint;

		onProgress(taskId);
	
	} else 
	if (strcmp(msg, "TASK_ABORT") == 0) {

		// id
		cJSON* id_obj = cJSON_GetObjectItemCaseSensitive(json, "id");
		
		if (id_obj == 0 || id_obj->type != cJSON_Number) {
			onFail();
			return;
		}
		int taskId = id_obj->valueint;

		onAbort(taskId);

	} else
	if (strcmp(msg, "QUIT") == 0) {
		mClientQuit = 1;
		mClientClosed = 1;
		onQuit();
	}
}

int CComUnixSchedScheduler::write1(cJSON* json) {
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
			CLogger::mainlog->error("UnixClient: write error %s", strerror(errno));
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

int CComUnixSchedScheduler::write1(char* str) {
	mWriteMutex.lock();

	char* buff = str;
	CLogger::mainlog->debug("> %s", buff);
	long len = strlen(buff) + 1;
	long writtentotal = 0;
	long written = 0;
	while (writtentotal < len) {
		written = send(this->mSocket, buff, len-writtentotal, MSG_NOSIGNAL);
		CLogger::mainlog->debug("%d written", written);
		if (written < 0) {
			CLogger::mainlog->error("UnixClient: write error %s", strerror(errno));
			break;
		}
		writtentotal += written;
	}
	fsync(this->mSocket);

	mWriteMutex.unlock();
	if (written != len) {
		return -1;
	}
	return 0;
}

void CComUnixSchedScheduler::writeMessage(CComUnixWriteMessage* message) {

	CLogger::mainlog->debug("UnixSchedScheduler: Writer: writeMessage");

	CComSchedMessage* schedMsg = 0;
	if (typeid(*message) != typeid(CComSchedMessage)) {
		CLogger::mainlog->error("UnixSchedScheduler: Writer: unknown message type: %s", typeid(*message).name());
		return;
	}
	schedMsg = (CComSchedMessage*) message;

	switch (schedMsg->type) {
		case EComSchedMessageType::STARTED:
			writeStarted(schedMsg->taskid);
		break;
		case EComSchedMessageType::SUSPENDED:
			writeSuspended(schedMsg->taskid, schedMsg->progress);
		break;
		case EComSchedMessageType::TASKLIST:
			writeTasklist(schedMsg->tasks);
		break;
		case EComSchedMessageType::PROGRESS:
			writeProgress(schedMsg->taskid, schedMsg->progress);
		break;
		case EComSchedMessageType::FINISHED:
			writeFinished(schedMsg->taskid);
		break;
		default:
			CLogger::mainlog->debug("UnixSchedScheduler: Writer: unknown message type");
		break;
	}

}


