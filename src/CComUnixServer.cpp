// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#include <string>
#include <cstring>
#include <csignal>
#include <cstdio>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/syscall.h>
#include <sys/stat.h>
#include <poll.h>

#include "CComUnixServer.h"
#include "CComUnixClient.h"
#include "CComClient.h"
#include "CConfig.h"
#include "CLogger.h"

#include "CComUnixSchedClientMain.h"

using namespace sched::com;
using sched::com::CComUnixSchedClientMain;

CComUnixServer::CComUnixServer(char* unixpath):
	mrWriter()
{

	mWakeupPipe[0] = -1;
	mWakeupPipe[1] = -1;

	mpPath = unixpath;

	this->mPollList = new struct pollfd[this->mPollListMax]();

}

int CComUnixServer::start(){

	CLogger::mainlog->info("UnixServer: start");
	struct sockaddr_un addr = {};
	addr.sun_family = AF_UNIX;

	if (pipe(this->mWakeupPipe) == -1) {
		CLogger::mainlog->error("UnixServer: pipe creation failed %s", strerror(errno));
		return -1;
	}

	// checks
	if (strlen(this->mpPath) > sizeof(addr.sun_path)) {
		CLogger::mainlog->error("UnixServer: Path %s too long", this->mpPath);
		return -1;
	}
	strcpy(addr.sun_path, this->mpPath);

	this->mSocket = socket(AF_LOCAL, SOCK_STREAM, 0);

	if (this->mSocket == -1) {
		CLogger::mainlog->error("UnixServer: %s", strerror(errno));
		return -1;
	}

	if (bind(this->mSocket, (struct sockaddr*) &addr, sizeof(addr)) != 0) {
		CLogger::mainlog->error("UnixServer: %s", strerror(errno));
		close(this->mSocket);
		mSocket = -1;
		return -1;
	}

	if (listen(this->mSocket, 0)) {
		CLogger::mainlog->error("UnixServer: %s", strerror(errno));
		close(this->mSocket);
		mSocket = -1;
		return -1;
	}

	this->mStopServer = false;

	this->mThread = std::thread(&CComUnixServer::serve, this);
	CLogger::mainlog->debug("UnixServer: start %d", this->mSocket);
	return 0;
}

void CComUnixServer::serve(){
	int ret = 0;
	int error = 0;

	mPid = syscall(SYS_gettid);
	mDoneServer = false;
	CLogger::mainlog->debug("UnixServer: thread %d", mPid);


    // mask signals for this thread
    sigset_t sigset = {};
    ret = sigemptyset(&sigset);
	ret = sigaddset(&sigset, SIGINT);
	ret = sigaddset(&sigset, SIGTERM);
    //ret = sigprocmask(SIG_BLOCK, &sigset, 0);
    ret = pthread_sigmask(SIG_BLOCK, &sigset, 0);
    if (ret != 0) {
		CLogger::mainlog->error("ComUnixServer: thread signal error: %s", strerror(errno));
    }

	addPollEntry(mSocket);
	addPollEntry(mWakeupPipe[0]);

	struct sockaddr_un addr = {};
	socklen_t addrlen;
	addrlen = sizeof(addr);
	int socket;
	int wakeup;
	int pollret;
	while (mStopServer != true) {
		CLogger::mainlog->debug("UnixServer accept");
		pollret = poll(mPollList, mPollListPos, -1);
		if (pollret == -1) {
			error = errno;
			if (error == EAGAIN) {
				continue;
			}
			CLogger::mainlog->error("UnixServer poll error %s", strerror(error));
			break;
		}

		// new socket
		if (mPollList[0].revents != 0) {
			CLogger::mainlog->debug("UnixServer POLL incoming socket");
			socket = accept(mSocket, (struct sockaddr*) &addr, &addrlen);
			if (socket == -1) {
				error = errno;
				CLogger::mainlog->debug("UnixServer accept error %s %d", strerror(error), mSocket);
				if (error == EINTR || error == EBADF) {
					// thread interrupt, stop serving
					break;
				}

				//continue;
				break;
			}
			CLogger::mainlog->debug("UnixServer got socket");
			addNewClient(socket);
			mPollList[0].revents = 0;
			pollret--;
		}

		// wakeup msg
		if (mPollList[1].revents != 0) {
			CLogger::mainlog->debug("UnixServer POLL wakeup");
			wakeup = recvWakeup();
			if (wakeup == -1) {
				CLogger::mainlog->error("UnixServer wakeup fail");
				break;
			}
			// shutdown
			if (wakeup == 0x01) {
				break;
			}
			// new poll entry
			if (wakeup == 0x02) {
				// do nothing
			}

			mPollList[1].revents = 0;
			pollret--;
		}

		// read on client socket
		if (pollret > 0) {
			CLogger::mainlog->debug("UnixServer POLL other data");
			for (int i=2; i<mPollListPos; i++) {
				if (mPollList[i].events != 0 && mPollList[i].revents != 0) {
					CLogger::mainlog->debug("UnixServer %d: POLL %ld", mPollList[i].fd, mPollList[i].revents);
					if ((mPollList[i].revents & POLLIN) != 0)
 {
						// read from client socket
						CLogger::mainlog->debug("UnixServer %d: read from client", mPollList[i].fd);
						readClients(mPollList[i].fd);
					}
					if ((mPollList[i].revents & POLLERR) != 0 ||
						(mPollList[i].revents & POLLNVAL) != 0 ||
						(mPollList[i].revents & POLLHUP) != 0) {
						// socket failed, remove client
						CLogger::mainlog->debug("UnixServer %d: socket failed remove client!", mPollList[i].fd);
						CComClient* client = getClientById(mPollList[i].fd);
						if (client != 0) {
							removeClient(client);
						} else {
							CLogger::mainlog->debug("UnixServer %d: client not found", mPollList[i].fd);
						}
					}
					mPollList[i].revents = 0;
					pollret--;
					if (pollret == 0) {
						break;
					}
				}
			}
		}
		
	}
	CLogger::mainlog->debug("UnixServer thread done");
	mDoneServer = true;
}

void CComUnixServer::addNewClient(int socket){

	CComUnixClient* client = createNewClient(socket);
	CLogger::mainlog->debug("UnixServer: add new client %x socket %d", client, socket);

	{
		std::lock_guard<std::mutex> lg(mClientMutex);
		CComClient* cclient = dynamic_cast <CComClient*> (client);
		//mClients.push_back((CComClient*) client);
		mClients.push_back(cclient);
		addPollEntry(socket);
	}

}

void CComUnixServer::addNewClient(CComUnixClient* uclient, int socket){
	
	CLogger::mainlog->debug("UnixServer: add new client %x socket %d", uclient, socket);
	{
		std::lock_guard<std::mutex> lg(mClientMutex);
		CComClient* cclient = dynamic_cast <CComClient*> (uclient);
		//mClients.push_back((CComClient*) client);
		mClients.push_back(cclient);
		addPollEntry(socket);
		sendWakeup(0x02);
	}

}

void CComUnixServer::removeClient(CComClient* pClient){

	CLogger::mainlog->debug("UnixServer: removeClient");
	// find client in list and remove it
	int index = -1;
	{
		std::lock_guard<std::mutex> lg(mClientMutex);
		for (unsigned int i=0; i<mClients.size(); i++) {
			if (pClient == mClients[i]) {
				index = i;
				break;
			}
		}
		if (index == -1) {
			// not found
			return;
		}
		CComUnixClient* uclient = 0;
		int socket = -1;

		uclient = dynamic_cast <CComUnixClient*> (mClients[index]);

		if (uclient != 0) {
			socket = uclient->mSocket;
		}
		//if (typeid(CComClient) == typeid(*(mClients[index]))) {
		//	uclient = (CComUnixClient*) mClients[index];
	//		socket = uclient->mSocket;
	//	}
		CLogger::mainlog->debug("UnixServer: dyncast CComUnixClient %x", uclient);
		mClients.erase(mClients.begin() + index);
		delete pClient;
		if (socket != -1) {
			removePollEntry(socket);
		}
	}
}

CComClient* CComUnixServer::getClientById(int id) {

	{
		std::lock_guard<std::mutex> lg(mClientMutex);
		for (unsigned int i=0; i<mClients.size(); i++) {
			CLogger::mainlog->debug("UnixServer: getClientById search %d check client with address %x and type %s", id, mClients[i], typeid(*(mClients[i])).name() );

			CComClient* cclient = mClients[i];


			CComUnixClient* uclient = dynamic_cast <CComUnixClient*> (cclient);
			CLogger::mainlog->debug("UnixServer: getClientById CComUnixClient %x",uclient);

			CComUnixSchedClient* sclient = dynamic_cast <CComUnixSchedClient*> (cclient);
			CLogger::mainlog->debug("UnixServer: getClientById CComUnixSchedClient %x",sclient);

			CComUnixSchedClientMain* smclient = dynamic_cast <CComUnixSchedClientMain*> (cclient);
			CLogger::mainlog->debug("UnixServer: getClientById CComUnixSchedClientMain %x",smclient);

			CComUnixClient* client = dynamic_cast <CComUnixClient*> (cclient);
			CLogger::mainlog->debug("UnixServer: getClientById bbla %x",client);
/*
			if (client == 0) {
				CLogger::mainlog->debug("UnixServer: getClientById client is not a CComUnixClient");
				continue;
			}
*/
			//client = (CComUnixClient*) cclient;
			CLogger::mainlog->debug("UnixServer: getClientById search %d check %d", id, client->mSocket);
			if (client->mSocket == id) {
				return mClients[i];
			}
		}
	}
	return 0;
}

bool CComUnixServer::findClient(CComClient* pClient){
	if (pClient == 0) {
		return false;
	}
	{
		std::lock_guard<std::mutex> lg(mClientMutex);
		for (unsigned int i=0; i<mClients.size(); i++) {
			if (pClient == (CComClient*) mClients[i]) {
				return true;
			}
		}
	}
	return false;
}

void CComUnixServer::readClients(int fd){
	CComUnixClient* client;
	for (unsigned int i=0; i<mClients.size(); i++) {
		client = dynamic_cast<CComUnixClient*> (mClients[i]);
		if (client != 0 && client->mSocket == fd) {
			client->read();
		}
	}
}

void CComUnixServer::clearClients(){
	// in case of shutdown (!)
	// delete all clients
	CComClient* client;
	while(mClients.empty() == false) {
		client = mClients.back();
		mClients.pop_back();
		if (client != 0) {
			delete client;
		}
	}
}

void CComUnixServer::stop(){
	CLogger::mainlog->info("UnixServer: stop");
	int ret;
	this->mStopServer = true;
	// interrupt
	if (this->mThread.joinable() == true) {
		if (this->mDoneServer == false) {
			ret = sendWakeup(0x01);
			if (ret == -1) {
				CLogger::mainlog->error("UnixServer wakeup error");
			}
		}
		this->mThread.join();

		if (this->mSocket > -1) {
			CLogger::mainlog->debug("UnixServer: close");
			close(this->mSocket);
			this->mSocket = -1;
		}
	}
	if (this->mpPath != 0) {
		struct stat fstat = {};
		ret = lstat(this->mpPath, &fstat);
		if (ret != 0) {
			int error = errno;
			if (error != ENOENT) {
				CLogger::mainlog->debug("UnixServer: stat on %s failed %s", this->mpPath, strerror(error));
			}
		} else {
			ret = remove(this->mpPath);
			if (ret != 0) {
				int error = errno;
				CLogger::mainlog->debug("UnixServer: removing socket %s failed %s", this->mpPath, strerror(error));
			}
		}
	}
	if (this->mWakeupPipe[0] != -1) {
		close(this->mWakeupPipe[0]);
		this->mWakeupPipe[0] = -1;
	}
	if (this->mWakeupPipe[1] != -1) {
		close(this->mWakeupPipe[1]);
		this->mWakeupPipe[1] = -1;
	}
	// Stop writing buffer to send dangling messages
	mrWriter.stop();
	clearClients();
}

void CComUnixServer::incPollList(){

	// no poll list
	if (this->mPollList == 0) {
		this->mPollList = new struct pollfd[this->mPollListMax*2];
		this->mPollListMax *= 2;
		return;
	}
	// create larger list and copy old data
	struct pollfd* newlist = new struct pollfd[this->mPollListMax*2];
	memcpy(newlist, this->mPollList, sizeof(struct pollfd)*this->mPollListMax);
	delete this->mPollList;
	this->mPollList = newlist;
	this->mPollListMax *= 2;

}

void CComUnixServer::addPollEntry(int fd){

	int next = 0;
	for (next = 0; next<mPollListPos; next++) {
		if (mPollList[next].fd == -1) {
			break;
		}
	}
	if (next == mPollListPos && mPollListPos == mPollListMax) {
		incPollList();
	}

	mPollList[next].fd = fd;
	mPollList[next].events = POLLIN;
	if (next == mPollListPos) {
		mPollListPos++;
	}

}

void CComUnixServer::removePollEntry(int fd){

	// find entry and invalidate
	for (int i=0; i<mPollListPos; i++) {
		if (mPollList[i].fd == fd && mPollList[i].events != 0) {
			mPollList[i].fd = -1;
			mPollList[i].events = 0;
			break;
		}
	}
	// search for last valid entry
	int last = mPollListPos-1;
	for (last = mPollListPos; last > 1; last--) {
		if (mPollList[last].fd != -1) {
			break;
		}
	}
	mPollListPos = last+1;

}

int CComUnixServer::sendWakeup(int msg){

	int ret;
	ret = write(this->mWakeupPipe[1], &msg, sizeof(msg));
	if (ret == -1) {
		CLogger::mainlog->error("UnixServer sending to pipe error %s", strerror(errno));
		return -1;
	}
	return 0;

}

int CComUnixServer::recvWakeup(){

	int ret;
	int res;
	ret = read(this->mWakeupPipe[0], &res, sizeof(res));
	if (ret == -1) {
		CLogger::mainlog->error("UnixServer reading from pipe error %s", strerror(errno));
		return -1;
	}
	return res;

}

CComUnixServer::~CComUnixServer(){
	if (this->mPollList != 0) {
		delete[] this->mPollList;
		this->mPollList = 0;
	}
}

CComUnixWriteBuffer& CComUnixServer::getWriter(){
	return mrWriter;
}
