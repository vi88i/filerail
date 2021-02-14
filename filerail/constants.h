#ifndef _CONSTANTS_H
#define _CONSTANTS_H

// max path length
#define MAX_PATH_LENGTH 4097
// max name of resource (file/dir)
#define MAX_RESOURCE_LENGTH 256
// size of buffer for tcp sockets at application layer
#define BUFFER_SIZE 1024
// max number of clients connected
#define BACKLOG 8
// max width of progress bar (50 spaces)
#define PROGRESS_BAR_WIDTH 50
// time out blocking for send and recv calls
#define TIME_OUT 36000
// time out on recv during file transfer
#define MAX_IO_TIME_OUT 10

#endif
