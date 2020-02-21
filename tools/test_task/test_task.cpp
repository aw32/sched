// Copyright 2019, Alex Wiens <awiens@mail.upb.de>, Achim Lösch <achim.loesch@upb.de>
// SPDX-License-Identifier: BSD-2-Clause

#include <vector>
#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <cstdlib>
#include <cstdio>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
#include <cstring>
#include <ctime>
#include <cmath>
#include <cerrno>

enum ECommand {
	CMD_WAIT,
	CMD_SEND,
	CMD_RECV
};

const char* ECommandStrings[3] = {
	"WAIT",
	"SEND",
	"RECV"
};

struct SMessage {
	ECommand command;
	
	double wait;
	std::string* message;

	~SMessage() {
		if (message != 0) {
			delete message;
			message = 0;
		}
	};

};

std::vector<SMessage*>* read_messages(char* messages_filepath) {

	std::vector<SMessage*>* out = new std::vector<SMessage*>();

	std::ifstream infile( messages_filepath );
	std::string line;
	while (std::getline(infile, line))
	{

		if (line[0] == '#') {
			continue;
		}

		std::string::size_type split_index = line.find(" ");
		if (split_index == std::string::npos) {
			continue;
		}


		std::string command = line.substr(0, split_index);
		std::string message = line.substr(split_index+1);
		double wait = 0.0;
		ECommand cmd = CMD_WAIT;

		if (command == "wait") {
			cmd = CMD_WAIT;
			const char* str = message.c_str();
			char* end = 0;
			wait = std::strtod(str, &end);
			if (end == str) {
				std::cout << "Invalid waiting time" << message << std::endl;
				continue;
			}
		} else if (command == "recv") {
			cmd = CMD_RECV;
		} else if (command == "send") {
			cmd = CMD_SEND;
		} else {
			std::cout << "Unknown command " << command << std::endl;
			continue;
		}

		SMessage* msg = new SMessage();
		msg->wait = wait;
		msg->command = cmd;
		msg->message = new std::string(message);
		//printf("%p \n", message.c_str());
		//printf("%p \n", msg->message->c_str());
		out->push_back(msg);
	}

	if (out->size() == 0) {
		delete out;
		out = 0;
	}
	return out;
}

int read_message(int socket, char* readbuffer, unsigned long buffersize, unsigned long* bufferpos, char** readmsg) {

	// Remove previous message
	if ((*readmsg) != 0) {
		size_t len = strlen(*readmsg) + 1;
		size_t overlap = (*bufferpos) - len;
		if (overlap > 0) {
			bcopy(readbuffer+len, readbuffer, overlap);
		}
		*bufferpos = overlap;
		(*readmsg) = 0;
	}

	// Check if there is a dangling message in buffer
	if (*bufferpos > 0) {
		size_t msglen = strnlen(readbuffer, *bufferpos);
		if (msglen < *bufferpos) {
			(*readmsg) = readbuffer;
			return 1;
		}
	}

	// Check if buffer is full
	if (*bufferpos == buffersize) {
		std::cout << "Recv failure: buffer not large enough (buffer full without message end)" << std::endl;
		return -1;
	}

	// Read new data
	long maxread = buffersize - *bufferpos;
	ssize_t read = 0;
	read = recv(socket, readbuffer+(*bufferpos), maxread, 0);
	if (read == -1) {
		std::cout << "Recv failure: " << strerror(errno) << std::endl;
		return -1;
	}
	if (read == 0) {
		std::cout << "Recv: client shutdown " << std::endl;
		return -1;
	}
	(*bufferpos) += read;

	// Check if there is a new message
	if (*bufferpos > 0) {
		size_t msglen = strnlen(readbuffer, *bufferpos);
		if (msglen < *bufferpos) {
			(*readmsg) = readbuffer;
			return 1;
		}
	}

	return 0;
}

int main(int argc, char* argv[]) {

	if (argc < 2) {
		std::cout << "test_task messages.txt" << std::endl;
		exit(1);
	}

	// Read messages
	std::vector<SMessage*>* messages = 0;
	messages = read_messages(argv[1]);

	if (messages == 0) {
		std::cout << "No messages" << std::endl;
		exit(1);
	}

	//for (unsigned int i=0; i<messages->size(); i++) {
	//	std::cout << (*((*messages)[i]->message)) << std::endl;
	//}

	// Open server socket
	const char* socket_name = "/tmp/test.socket";
	struct sockaddr_un addr = {};
	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, socket_name);
	int server_socket = -1;
	int client_socket = -1;
	struct sockaddr_un client_addr = {};
	socklen_t addrlen;
	addrlen = sizeof(client_addr);
	int status = 0;
	unsigned int msg_index = 0;

	char* readbuffer = new char[1024];
	unsigned long buffersize = 1024;
	unsigned long bufferpos = 0;
	char* readmsg = 0;
	int newmsg = 0;

	server_socket = socket(AF_LOCAL, SOCK_STREAM, 0);
	if (bind(server_socket, (struct sockaddr*) &addr, sizeof(addr)) != 0) {
		std::cout << "Socket bind failure: " << strerror(errno) << std::endl;
		status = 1;
		goto error;
	}
	if (listen(server_socket, 0) != 0) {
		std::cout << "Socket listen failure: " << strerror(errno) << std::endl;
		status = 1;
		goto error;
	}

	// Wait for client
	client_socket = accept(server_socket, (struct sockaddr*) &addr, &addrlen);
	if (client_socket == -1) {
		std::cout << "Socket accept failure: " << strerror(errno) << std::endl;
		status = 1;
		goto error;
	}


	// Go through messages
	for (msg_index = 0; msg_index < messages->size(); msg_index++) {

		SMessage* msg = (*messages)[msg_index];

		std::cout << "# " << ECommandStrings[msg->command] << std::endl;

		struct timespec time = {};

		char* buff = 0;
		size_t buff_len = 0;
		size_t tosend = 0;
		ssize_t sent = 0;
		

		switch (msg->command) {

			case CMD_WAIT:
				time.tv_sec = floor(msg->wait);
				time.tv_nsec = (long) ((msg->wait - time.tv_sec) * 1000000000);
				nanosleep(&time, 0);
			break;

			case CMD_SEND:
				buff = (char*) msg->message->c_str();
				buff_len = strlen(buff) + 1;
				tosend = buff_len;
				sent = 0;
				std::cout << "> " << buff << std::endl;
				//printf("%p %p\n", buff, buff+(buff_len-tosend));
				//std::cout << "> " << buff+(buff_len-tosend) << std::endl;
				while ( tosend > 0 && status == 0 ) {
					sent = send(client_socket, buff+(buff_len-tosend), buff_len, 0);
					if (sent == -1) {
						std::cout << "Send failure: " << strerror(errno) << std::endl;
						status = 1;
						goto error;
					} else {
						tosend -= sent;
					}
				}
			break;

			case CMD_RECV:
				newmsg = 0;
				while (newmsg == 0 && status == 0) {
					// Read message
					newmsg = read_message(client_socket, readbuffer, buffersize, &bufferpos,  &readmsg);
					//std::cout << newmsg << std::endl;
					if (newmsg == -1) {
						status = 1;
						goto error;
					}
					// Print new message and compare with expected one
					if (newmsg == 1) {
						std::cout << "< " << readmsg << std::endl;
						if (strcmp(msg->message->c_str(), readmsg) == 0) {
							// message found
							break;
						}
						newmsg = 0;
					}
				}
			break;
			
		}

		if (status != 0) {
			break;
		}
	}





	// Clean up
	error:

		// Close client socket
		if (client_socket != -1) {
			close(client_socket);
			client_socket = -1;
		}

		// Close server socket
		if (server_socket != -1) {
			close(server_socket);
			server_socket = -1;
		}

		// Remove socket file
		remove(socket_name);

		if (readbuffer != 0) {
			delete[] readbuffer;
			readbuffer = 0;
		}

		if (messages != 0) {
			while (messages->empty() == false) {
				delete (*messages)[messages->size()-1];
				messages->pop_back();
			}
			delete messages;
		}

		return status;

}
