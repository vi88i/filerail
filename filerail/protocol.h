#ifndef _PROTOCOL_H
#define _PROTOCOL_H

#include <sys/types.h>
#include "constants.h"

#define NUM_ATTRS_FOR_RESOURCE_HEADER 3
#define NUM_ATTRS_FOR_DATA_PACKET 3

// filerail commands
enum COMMAND {
	GET, // get resource from server
	PUT, // put resource on server
	PING, // ping server
	CHECK, // check feasibility of uploading file on server
	ABORT, // abort the process
	OVERWRITE, // notify user about overwriting files while uploading
	RESOURCE_SIZE, // advertise resource size
	RESUME, // prompt client whether it wants to resume from previous checkpoint
	RESTART // tell client there are no checkpoints
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
	NO_INTEGRITY // to indicate md5 hash calculated at receiver is not same as advertised md5 hash
};

// command structure
typedef struct _filerail_command_header {
	uint8_t command_type; // self-explanatory
} filerail_command_header;

// response structure
typedef struct _filerail_response_header {
	uint8_t response_type; // self-explanatory
} filerail_response_header;

// metadata about resource to be sent
typedef struct _filerail_resource_header {
	uint64_t resource_size; // stores resource size
	char resource_name[MAX_RESOURCE_LENGTH]; // self-explanatory
	char resource_dir[MAX_PATH_LENGTH]; // self-explanatory
} filerail_resource_header;

// packet which transports encrypted data
typedef struct _filerail_data_packet {
	uint8_t data_payload[BUFFER_SIZE]; // data
	uint64_t data_size; // size of actual data
	uint64_t data_padding; // padding added by AES encryption
} filerail_data_packet;

// serializes checkpoint
typedef struct _filerail_checkpoint {
	uint64_t offset; // stores offset
	char resource_path[MAX_PATH_LENGTH]; // self-explanatory
} filerail_checkpoint;

#endif
