#ifndef _SOCKET_H
#define _SOCKET_H

#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <iostream>
#include <string>

using namespace std;

int socketConnect(const string & hostName, const string & portNum);
int buildServer(const string & portNum);
int serverAccept(const int serverFD, string & clientIp);
#endif