// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#include <cstdlib>
#include <unistd.h>
#include <cstring>
#include <sys/wait.h>
#include "CLogger.h"
#include "CExternalHook.h"
using namespace sched;

CExternalHook::CExternalHook(char* program) {
	mpProgram = program;
}

int CExternalHook::execute(char** arguments){

	std::ostringstream ss;

	ss << mpProgram;

	if (arguments != 0) {
		for (int i=0; arguments[i] != 0; i++) {
			ss << " \"" << arguments[i] << "\"";
		}
	}

	std::string command_str = ss.str();
	const char* command = command_str.c_str();

	CLogger::mainlog->debug("ExternalHook: execute %s", command);


	int count = 0;
	int ix = 0;
	while (arguments != 0 && arguments[ix] != 0) {
		count++;
		ix++;
	}

	char** execarguments = new char*[count+2]();
	execarguments[0] = mpProgram;
	for (ix=0; ix<count; ix++) {
		execarguments[ix+1] = arguments[ix];
	}

	int status = 0;	
	//int status = std::system(command);

	pid_t pid = fork();
	if (pid == -1) {
		// fork failed
		int error = errno;
		errno = 0;
		CLogger::mainlog->debug("ExternalHook: execute %s failed %d %s", command, error, strerror(error));
	} else
	if (pid == 0) {
		// child

		execv(execarguments[0], execarguments);

		// execv should never return
		_exit(1);

	} else {
		// parent
		int status_info = 0;
		pid_t ret = waitpid(pid, &status_info, 0);
		if (-1 == ret) {
			int error = errno;
			errno = 0;
			CLogger::mainlog->debug("ExternalHook: execute %s ok, waitpid failed %d %s", command, error, strerror(error));
		} else {
			status = WEXITSTATUS(status_info);
		}
	}

	CLogger::mainlog->info("ExternalHook: execute %s = %d", command, status);
	delete[] execarguments;
	return status;
}
