#ifndef _GLOBAL_H
#define _GLOBAL_H

#include <stdio.h>
#include <syslog.h>

#define PRINT(x) 							 \
	if (verbose && !is_server) { \
		x;					 							 \
	}

#define LOG(p, m) 				 \
	if (is_server) { 				 \
		syslog(p, #m " : %m"); \
	} else { 								 \
		perror(m);						 \
	}

int verbose = 0;
int is_server = 0;

#endif
