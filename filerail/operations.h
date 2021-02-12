#ifndef _OPERATIONS_H
#define _OPERATIONS_H

#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>

#include "global.h"
#include "protocol.h"
#include "socket.h"
#include "utils.h"
#include "aes128.h"
#include "../deps/kuba__zip/zip.h"

int filerail_sendfile_handler(
	int fd,
	const char *resource_dir,
	const char *resource_name,
	struct stat *stat_resource,
	const char *key_path);

int filerail_recvfile_handler(
	int fd,
	const char *resource_name,
	const char *resource_dir,
	const char *resource_path,
	const char *key_path);

int filerail_sendfile_handler(
	int fd,
	const char *resource_dir,
	const char *resource_name,
	struct stat *stat_resource,
	const char *key_path)
{
	int exit_status;
	char current_dir[MAX_PATH_LENGTH], zip_filename[MAX_RESOURCE_LENGTH];
	struct zip_t *zip;
	clock_t start, end;
	double cpu_time_used;
	aesCryptoInfo ci;
	AES_keys K;

	exit_status = 0;

	if (filerail_getcwd(current_dir) == -1) {
		exit_status = -1;
		goto clean_up;
	}

	PRINT(printf("Generating keys...\n"));
	if (filerail_cd(key_path) == -1) {
		exit_status = -1;
		goto clean_up;
	}
	ci = (aesCryptoInfo){"key.txt", "nonce.txt"};
	if (readNonce(ci.nonce_file, &K) == -1) {
		exit_status = -1;
		goto clean_up;
	}
	if (readKey(ci.key_file, &K) == -1) {
		exit_status = -1;
		goto clean_up;
	}
	generateRoundKeys(&K);
	if (filerail_cd(current_dir) == -1) {
		exit_status = -1;
		goto clean_up;
	}
	PRINT(printf("Finished...\n"));

	if (filerail_cd(resource_dir) == -1) {
		exit_status = -1;
		goto clean_up;
	}

	zip_filename[0] = '\0';
	strcpy(zip_filename, resource_name);
	strcat(zip_filename, ".zip");

	PRINT(printf("Zipping resource...\n"));
	if (S_ISDIR(stat_resource->st_mode)) {
		if (
			(zip = zip_open(zip_filename, ZIP_DEFAULT_COMPRESSION_LEVEL, 'w')) == NULL ||
			filerail_zip_folder(zip, resource_name) == -1
		)
		{
			LOG(LOG_USER | LOG_ERR, "operations.h filerail_sendfile_handler");
			goto clean_up;
		}
	} else {
		if (
			(zip = zip_open(zip_filename, ZIP_DEFAULT_COMPRESSION_LEVEL, 'w')) == NULL ||
			filerail_zip_file(zip, resource_name)
			)
		{
			LOG(LOG_USER | LOG_ERR, "operations.h filerail_sendfile_handler");
			goto clean_up;
		}
	}
	zip_close(zip);
	zip = NULL;
	PRINT(printf("Resource zipped...\n"););

	PRINT(printf("Ready to send resource...\n"));
  start = clock();
  if (filerail_sendfile(fd, zip_filename, &K) == -1) {
  	exit_status = -1;
  	goto clean_up;
  }
  end = clock();
  cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
  PRINT(printf("File transfer complete in %f seconds...\n", cpu_time_used));

	if (filerail_rm(zip_filename) == -1) {
		exit_status = -1;
		goto clean_up;
	}

	if (filerail_cd(current_dir) == -1) {
		exit_status = -1;
		goto clean_up;
	}
	clean_up:
	zip_close(zip);
	return exit_status;
}

int filerail_recvfile_handler(
	int fd,
	const char *resource_name,
	const char *resource_dir,
	const char *resource_path,
	const char *key_path)
{
  int exit_status;
  char current_dir[MAX_PATH_LENGTH], zip_filename[MAX_RESOURCE_LENGTH];
	clock_t start, end;
	double cpu_time_used;
	aesCryptoInfo ci;
	AES_keys K;

  exit_status = 0;

  if (filerail_getcwd(current_dir) == -1) {
  	exit_status = -1;
  	goto clean_up;
  }

	PRINT(printf("Generating keys...\n"));
	if (filerail_cd(key_path) == -1) {
		exit_status = -1;
		goto clean_up;
	}
	ci = (aesCryptoInfo){"key.txt", "nonce.txt"};
	if (readNonce(ci.nonce_file, &K) == -1) {
		exit_status = -1;
		goto clean_up;
	}
	if (readKey(ci.key_file, &K) == -1) {
		exit_status = -1;
		goto clean_up;
	}
	generateRoundKeys(&K);
	if (filerail_cd(current_dir) == -1) {
		exit_status = -1;
		goto clean_up;
	}
	PRINT(printf("Finished...\n"));

  PRINT(printf("Waiting for server to respond...\n"));
  start = clock();
  if (filerail_recvfile(fd, resource_path, &K) == -1) {
  	exit_status = -1;
  	goto clean_up;
  }
  end = clock();
  cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
  PRINT(printf("File transfer complete in %f seconds...\n", cpu_time_used));

  if (filerail_cd(resource_dir) == -1) {
  	exit_status = -1;
  	goto clean_up;
  }

  zip_filename[0] = '\0';
  strcpy(zip_filename, resource_name);
  strcat(zip_filename, ".zip");

  PRINT(printf("Unzipping...\n"));
  if (zip_extract(zip_filename, resource_dir, zip_on_extract_entry, NULL) == -1) {
  	LOG(LOG_USER | LOG_ERR, "operations.h filerail_recvfile_handler");
  	exit_status = -1;
  	goto clean_up;
  }

  if (filerail_cd(current_dir) == -1) {
  	exit_status = -1;
  	goto clean_up;
  }

  if (filerail_rm(resource_path) == -1) {
  	LOG(LOG_USER | LOG_ERR, "operations.h filerail_recvfile_handler");
  	exit_status = -1;
  	goto clean_up;
  }

	clean_up:
	return exit_status;
}

#endif
