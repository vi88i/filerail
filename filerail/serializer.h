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
		return 0;											\
	}

size_t filerail_serialize_response_header(filerail_response_header *h, void *buf);
size_t filerail_serialize_command_header(filerail_command_header *h, void *buf);
size_t filerail_serialize_resource_header(filerail_resource_header *h, void *buf);
size_t filerail_serialize_data_packet(filerail_resource_header *h, void *buf);

size_t filerail_serialize_response_header(filerail_response_header *data, void *buf) {
	int ret;
	msgpack_sbuffer sbuf;
	msgpack_packer pk;

	msgpack_sbuffer_init(&sbuf);
	msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);

	ERR_CHECK(msgpack_pack_uint8(&pk, data->response_type), "serializer.h filerail_serialize_response_header");

	buf = malloc(sbuf.size);
	if (buf == NULL) {
		return 0;
	}
	ret = sbuf.size;
	memcpy(buf, sbuf.data, sbuf.size);

	msgpack_sbuffer_destroy(&sbuf);
	return ret;
}

size_t filerail_serialize_command_header(filerail_command_header *data, void *buf) {
	int ret;
	msgpack_sbuffer sbuf;
	msgpack_packer pk;

	msgpack_sbuffer_init(&sbuf);
	msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);

	ERR_CHECK(msgpack_pack_uint8(&pk, data->command_type), "serializer.h filerail_serialize_command_header");

	buf = malloc(sbuf.size);
	if (buf == NULL) {
		return 0;
	}
	ret = sbuf.size;
	memcpy(buf, sbuf.data, sbuf.size);

	msgpack_sbuffer_destroy(&sbuf);
	return ret;
}

size_t filerail_serialize_resource_header(filerail_resource_header *data, void *buf) {
	int ret;
	msgpack_sbuffer sbuf;
	msgpack_packer pk;

	msgpack_sbuffer_init(&sbuf);
	msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);

	ERR_CHECK(
		msgpack_pack_array(&pk, NUM_ATTRS_FOR_RESOURCE_HEADER),
		"serializer.h filerail_serialize_resource_header"
	);
	ERR_CHECK(
		msgpack_pack_uint64(&pk, data->resource_size),
		"serializer.h filerail_serialize_resource_header"
	);
	ERR_CHECK(
		msgpack_pack_str(&pk, MAX_RESOURCE_LENGTH),
		"serializer.h filerail_serialize_resource_header"
	);
	ERR_CHECK(
		msgpack_pack_str_body(&pk, data->resource_name, MAX_RESOURCE_LENGTH),
		"serializer.h filerail_serialize_resource_header"
	);
	ERR_CHECK(
		msgpack_pack_str(&pk, MAX_PATH_LENGTH),
		"serializer.h filerail_serialize_resource_header"
	);
	ERR_CHECK(
		msgpack_pack_str_body(&pk, data->resource_dir),
		"serializer.h filerail_serialize_resource_header"
	);

	buf = malloc(sbuf.size);
	if (buf == NULL) {
		return 0;
	}
	ret = sbuf.size;
	memcpy(buf, sbuf.data, sbuf.size);

	msgpack_sbuffer_destroy(&sbuf);
	return ret;
}

size_t filerail_serialize_data_packet(filerail_data_packet *data, void *buf) {
	int i, ret;
	msgpack_sbuffer sbuf;
	msgpack_packer pk;

	msgpack_sbuffer_init(&sbuf);
	msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);

	ERR_CHECK(
		msgpack_pack_array(&pk, NUM_ATTRS_FOR_DATA_PACKET),
		"serializer.h filerail_serialize_data_packet"
	);
	ERR_CHECK(
		msgpack_pack_array(&pk, BUFFER_SIZE),
		"serializer.h filerail_serialize_data_packet"
	);
	for (i = 0; i < BUFFER_SIZE; i++) {
		ERR_CHECK(
			msgpack_pack_uint8(&pk, data->data_payload[i]),
			"serializer.h filerail_serialize_data_packet"
		);
	}
	ERR_CHECK(
		msgpack_pack_uint64(&pk, data->data_size),
		"serializer.h filerail_serialize_data_packet"
	);
	ERR_CHECK(
		msgpack_pack_uint64(&pk, data->data_padding),
		"serializer.h filerail_serialize_data_packet"
	);

	buf = malloc(sbuf.size);
	if (buf == NULL) {
		return 0;
	}
	ret = sbuf.size;
	memcpy(buf, sbuf.data, sbuf.size);

	msgpack_sbuffer_destroy(&sbuf);
	return ret;
}

#endif
