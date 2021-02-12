#ifndef _PROTOCOL_H
#define _PROTOCOL_H

#include <sys/types.h>
#include "constants.h"

// filerail commands
enum COMMAND {
	GET, // get resource from server
	PUT, // put resource on server
	PING, // ping server
	CHECK, // check feasibility of uploading file on server
	ABORT, // abort the process
	OVERWRITE, // notify user about overwriting files while uploading
	RESOURCE_SIZE, // advertise resource size
};

// filerail responses
enum RESPONSE {
	OK, // ready to proceed
	PONG, // respond to ping
	FINISH, // process finished
	NOT_FOUND, // if resource not found
	NO_ACCESS, // peer doens't have read/write access
	BAD_RESOURCE, // bad resource (not file or directory)
	INSUFFICIENT_SPACE, // insufficient storage on server
	DUPLICATE_RESOURCE_NAME, // two resource having same name
};

// command structure
typedef struct _filerail_command_header {
	int command_type;
} filerail_command_header;

// response structure
typedef struct _filerail_response_header {
	int response_type;
} filerail_response_header;

// metadata about resource to be sent
typedef struct _filerail_resource_header {
	off_t resource_size;
	char resource_name[MAX_RESOURCE_LENGTH];
	char resource_dir[MAX_PATH_LENGTH];
} filerail_resource_header;

// packet which transports encrypted data
typedef struct _filerail_data_packet {
	char data_payload[BUFFER_SIZE]; // data
	size_t data_size; // size of actual data
	size_t data_padding; // padding added by AES encryption
} filerail_data_packet;

#endif
