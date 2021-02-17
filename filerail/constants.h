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
// length of md5 hash
#define MD5_HASH_LENGTH 16
// number of attributes in filerail_resource_header
#define NUM_ATTRS_FOR_RESOURCE_HEADER 3
// number of attributes in filerail_data_packet
#define NUM_ATTRS_FOR_DATA_PACKET 2
// key file size
#define KEY_FILE_SIZE 96
// size of AES key (AES-128-CBC => 16 byte keys)
#define AES_KEY_SIZE 16

#endif
