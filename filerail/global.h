#ifndef _GLOBAL_H
#define _GLOBAL_H

/*
	Because client and server (which daemonized) share the same libs,
	while printing or logging we have to differentiate b/w client and server.
*/
#include <stdio.h>
#include <syslog.h>

#define PRINT(x) 							 \
	if (verbose && !is_server) { \
		x;					 	             \
	}

// client ignores p
#define LOG(p, m) 				 \
	if (is_server) { 				 \
		syslog(p, #m " : %m"); \
	} else { 								 \
		perror(m);						 \
	}

int verbose = 0; // flag for setting verbose mode
int is_server = 0; // flag to check if host is client/server

#define min(a, b) ((a) > (b) ? (b) : (a))

#endif
