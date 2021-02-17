#ifndef _DESERIALIZER_H
#define _DESERIALIZER_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <msgpack.h>

#include "global.h"
#include "constants.h"
#include "protocol.h"

bool filerail_deserialize_response_header(filerail_response_header *ptr, void *buf, size_t size);
bool filerail_deserialize_command_header(filerail_command_header *ptr, void *buf, size_t size);
bool filerail_deserialize_resource_header(filerail_resource_header *ptr, void *buf, size_t size);
bool filerail_deserialize_file_offset(filerail_file_offset *ptr, void *buf, size_t size);
bool filerail_deserialize_resource_hash(filerail_resource_hash *ptr, void *buf, size_t size);
bool filerail_deserialize_data_packet(filerail_data_packet *ptr, void *buf, size_t size);

bool filerail_deserialize_response_header(filerail_response_header *ptr, void *buf, size_t size) {
	bool exit_status;
	msgpack_unpacked msg;
	msgpack_object root;

	exit_status = false;
	msgpack_unpacked_init(&msg);
	if (msgpack_unpack_next(&msg, buf, size, NULL) == MSGPACK_UNPACK_SUCCESS) {
		root = msg.data;
		ptr->response_type = root.via.u64;
		exit_status = true;
	}
	msgpack_unpacked_destroy(&msg);
	return exit_status;
}

bool filerail_deserialize_command_header(filerail_command_header *ptr, void *buf, size_t size) {
	bool exit_status;
	msgpack_unpacked msg;
	msgpack_object root;

	exit_status = false;
	msgpack_unpacked_init(&msg);
	if (msgpack_unpack_next(&msg, buf, size, NULL) == MSGPACK_UNPACK_SUCCESS) {
		root = msg.data;
		ptr->command_type = root.via.u64;
		exit_status = true;
	}
	msgpack_unpacked_destroy(&msg);
	return exit_status;
}

bool filerail_deserialize_resource_header(filerail_resource_header *ptr, void *buf, size_t size) {
	bool exit_status;
	msgpack_unpacked msg;
	msgpack_object root;

	exit_status = false;
	msgpack_unpacked_init(&msg);
	if (msgpack_unpack_next(&msg, buf, size, NULL) == MSGPACK_UNPACK_SUCCESS) {
		root = msg.data;
		ptr->resource_size = root.via.array.ptr[0].via.u64;
		memcpy(ptr->resource_name, root.via.array.ptr[1].via.str.ptr, MAX_RESOURCE_LENGTH);
		memcpy(ptr->resource_dir, root.via.array.ptr[2].via.str.ptr, MAX_PATH_LENGTH);
		exit_status = true;
	}
	msgpack_unpacked_destroy(&msg);
	return exit_status;
}

bool filerail_deserialize_resource_hash(filerail_resource_hash *ptr, void *buf, size_t size) {
	bool exit_status;
	int i;
	msgpack_unpacked msg;
	msgpack_object root;

	exit_status = false;
	msgpack_unpacked_init(&msg);
	if (msgpack_unpack_next(&msg, buf, size, NULL) == MSGPACK_UNPACK_SUCCESS) {
		root = msg.data;
		for (i = 0; i < MD5_HASH_LENGTH; i++) {
			ptr->hash[i] = root.via.array.ptr[i].via.u64;
		}
		exit_status = true;
	}
	msgpack_unpacked_destroy(&msg);
	return exit_status;
}

bool filerail_deserialize_file_offset(filerail_file_offset *ptr, void *buf, size_t size) {
	bool exit_status;
	msgpack_unpacked msg;
	msgpack_object root;

	exit_status = false;
	msgpack_unpacked_init(&msg);
	if (msgpack_unpack_next(&msg, buf, size, NULL) == MSGPACK_UNPACK_SUCCESS) {
		root = msg.data;
		ptr->offset = root.via.u64;
		exit_status = true;
	}
	msgpack_unpacked_destroy(&msg);
	return exit_status;
}

bool filerail_deserialize_data_packet(filerail_data_packet *ptr, void *buf, size_t size) {
	int i;
	bool exit_status;
	msgpack_unpacked msg;
	msgpack_object root;

	exit_status = false;
	msgpack_unpacked_init(&msg);
	if (msgpack_unpack_next(&msg, buf, size, NULL) == MSGPACK_UNPACK_SUCCESS) {
		root = msg.data;
		for (i = 0; i < BUFFER_SIZE; i++) {
			ptr->data_payload[i] = root.via.array.ptr[0].via.array.ptr[i].via.u64;
		}
		ptr->data_size = root.via.array.ptr[1].via.u64;
		exit_status = true;
	}
	msgpack_unpacked_destroy(&msg);
	return exit_status;
}

#endif
