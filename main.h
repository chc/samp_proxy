#ifndef _MAIN_H
#define _MAIN_H
#ifdef _WIN32
#include <windows.h>
#include <winsock.h>
typedef int socklen_t;
#define snprintf sprintf_s
#define strcasecmp _stricmp
#else
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <netdb.h>
#include <pthread.h>
#include <sys/times.h>
//#include <stropts.h>

#endif
#include <stdio.h>
#include <memory.h>
#include <stdint.h>

#endif
