#ifndef _SERIALIZER_H
#define _SERIALIZER_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <msgpack.h>

#include "global.h"
#include "constants.h"
#include "protocol.h"

#define ERR_CHECK(x, msg)         \
	if (x != 0) {									  \
		LOG(LOG_USER | LOG_ERR, msg); \
		return 0;										  \
	}

size_t filerail_serialize_response_header(filerail_response_header *ptr, void **buf);
size_t filerail_serialize_command_header(filerail_command_header *ptr, void **buf);
size_t filerail_serialize_resource_header(filerail_resource_header *ptr, void **buf);
size_t filerail_serialize_file_offset(filerail_file_offset *ptr, void **buf);
size_t filerail_serialize_resource_hash(filerail_resource_hash *ptr, void **buf);
size_t filerail_serialize_data_packet(filerail_data_packet *ptr, void **buf);

size_t filerail_serialize_response_header(filerail_response_header *ptr, void **buf) {
	size_t ret;
	msgpack_sbuffer sbuf;
	msgpack_packer pk;

	msgpack_sbuffer_init(&sbuf);
	msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);

	ERR_CHECK(msgpack_pack_uint8(&pk, ptr->response_type), "serializer.h filerail_serialize_response_header\n");

	*buf = malloc(sbuf.size);
	if (*buf == NULL) {
		LOG(LOG_USER | LOG_ERR, "serializer.h filerail_serialize_response_header\n");
		return 0;
	}
	ret = sbuf.size;
	memcpy(*buf, sbuf.data, sbuf.size);

	msgpack_sbuffer_destroy(&sbuf);
	return ret;
}

size_t filerail_serialize_command_header(filerail_command_header *ptr, void **buf) {
	size_t ret;
	msgpack_sbuffer sbuf;
	msgpack_packer pk;

	msgpack_sbuffer_init(&sbuf);
	msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);

	ERR_CHECK(msgpack_pack_uint8(&pk, ptr->command_type), "serializer.h filerail_serialize_command_header\n");

	*buf = malloc(sbuf.size);
	if (*buf == NULL) {
		LOG(LOG_USER | LOG_ERR, "serializer.h filerail_serialize_command_header\n");
		return 0;
	}
	ret = sbuf.size;
	memcpy(*buf, sbuf.data, sbuf.size);

	msgpack_sbuffer_destroy(&sbuf);
	return ret;
}

size_t filerail_serialize_resource_header(filerail_resource_header *ptr, void **buf) {
	size_t ret;
	msgpack_sbuffer sbuf;
	msgpack_packer pk;

	msgpack_sbuffer_init(&sbuf);
	msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);

	ERR_CHECK(
		msgpack_pack_array(&pk, NUM_ATTRS_FOR_RESOURCE_HEADER),
		"serializer.h filerail_serialize_resource_header\n"
	);
	ERR_CHECK(
		msgpack_pack_uint64(&pk, ptr->resource_size),
		"serializer.h filerail_serialize_resource_header\n"
	);
	ERR_CHECK(
		msgpack_pack_str(&pk, MAX_RESOURCE_LENGTH),
		"serializer.h filerail_serialize_resource_header\n"
	);
	ERR_CHECK(
		msgpack_pack_str_body(&pk, ptr->resource_name, MAX_RESOURCE_LENGTH),
		"serializer.h filerail_serialize_resource_header\n"
	);
	ERR_CHECK(
		msgpack_pack_str(&pk, MAX_PATH_LENGTH),
		"serializer.h filerail_serialize_resource_header\n"
	);
	ERR_CHECK(
		msgpack_pack_str_body(&pk, ptr->resource_dir, MAX_PATH_LENGTH),
		"serializer.h filerail_serialize_resource_header\n"
	);

	*buf = malloc(sbuf.size);
	if (*buf == NULL) {
		LOG(LOG_USER | LOG_ERR, "serializer.h filerail_serialize_resource_header\n");
		return 0;
	}
	ret = sbuf.size;
	memcpy(*buf, sbuf.data, sbuf.size);

	msgpack_sbuffer_destroy(&sbuf);
	return ret;
}

size_t filerail_serialize_file_offset(filerail_file_offset *ptr, void **buf) {
	size_t ret;
	msgpack_sbuffer sbuf;
	msgpack_packer pk;

	msgpack_sbuffer_init(&sbuf);
	msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);

	ERR_CHECK(msgpack_pack_uint64(&pk, ptr->offset), "serializer.h filerail_serialize_file_offset\n");

	*buf = malloc(sbuf.size);
	if (*buf == NULL) {
		LOG(LOG_USER | LOG_ERR, "serializer.h filerail_serialize_file_offset\n");
		return 0;
	}
	ret = sbuf.size;
	memcpy(*buf, sbuf.data, sbuf.size);

	msgpack_sbuffer_destroy(&sbuf);
	return ret;
}

size_t filerail_serialize_resource_hash(filerail_resource_hash *ptr, void **buf) {
	int i;
	size_t ret;
	msgpack_sbuffer sbuf;
	msgpack_packer pk;

	msgpack_sbuffer_init(&sbuf);
	msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);

	ERR_CHECK(msgpack_pack_array(&pk, MD5_HASH_LENGTH), "serializer.h filerail_serialize_resource_hash\n");
	for (i = 0; i < MD5_HASH_LENGTH; i++) {
		ERR_CHECK(msgpack_pack_uint8(&pk, ptr->hash[i]), "serializer.h filerail_serialize_resource_hash\n");
	}

	*buf = malloc(sbuf.size);
	if (*buf == NULL) {
		LOG(LOG_USER | LOG_ERR, "serializer.h filerail_serialize_resource_hash\n");
		return 0;
	}
	ret = sbuf.size;
	memcpy(*buf, sbuf.data, sbuf.size);

	msgpack_sbuffer_destroy(&sbuf);
	return ret;
}

size_t filerail_serialize_data_packet(filerail_data_packet *ptr, void **buf) {
	int i;
	size_t ret;
	msgpack_sbuffer sbuf;
	msgpack_packer pk;

	msgpack_sbuffer_init(&sbuf);
	msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);

	ERR_CHECK(
		msgpack_pack_array(&pk, NUM_ATTRS_FOR_DATA_PACKET),
		"serializer.h filerail_serialize_data_packet\n"
	);
	ERR_CHECK(
		msgpack_pack_array(&pk, BUFFER_SIZE),
		"serializer.h filerail_serialize_data_packet\n"
	);
	for (i = 0; i < BUFFER_SIZE; i++) {
		ERR_CHECK(
			msgpack_pack_uint8(&pk, ptr->data_payload[i]),
			"serializer.h filerail_serialize_data_packet\n"
		);
	}
	ERR_CHECK(
		msgpack_pack_uint64(&pk, ptr->data_size),
		"serializer.h filerail_serialize_data_packet\n"
	);

	*buf = malloc(sbuf.size);
	if (*buf == NULL) {
		LOG(LOG_USER | LOG_ERR, "serializer.h filerail_serialize_data_packet\n");
		return 0;
	}
	ret = sbuf.size;
	memcpy(*buf, sbuf.data, sbuf.size);

	msgpack_sbuffer_destroy(&sbuf);
	return ret;
}

#endif
