#ifndef _OPERATIONS_H
#define _OPERATIONS_H

#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "protocol.h"
#include "socket.h"
#include "utils.h"
#include "../deps/kuba__zip/zip.h"

int filerail_sendfile_handler(int fd, const char *resource_dir, const char *resource_name, struct stat *stat_resource);
int filerail_recvfile_handler(int fd, const char *resource_name, const char *resource_dir, const char *resource_path);

int filerail_sendfile_handler(int fd, const char *resource_dir, const char *resource_name, struct stat *stat_resource) {
	int exit_status;
	char current_dir[MAX_PATH_LENGTH], zip_filename[MAX_RESOURCE_LENGTH];
	struct zip_t *zip;

	exit_status = 0;

	if (filerail_getcwd(current_dir) == -1) {
		exit_status = -1;
		goto clean_up;
	}

	if (filerail_cd(resource_dir) == -1) {
		exit_status = -1;
		goto clean_up;
	}

	zip_filename[0] = '\0';
	strcpy(zip_filename, resource_name);
	strcat(zip_filename, ".zip");

	printf("Zipping resource...\n");
	if (S_ISDIR(stat_resource->st_mode)) {
		if (
			(zip = zip_open(zip_filename, ZIP_DEFAULT_COMPRESSION_LEVEL, 'w')) == NULL ||
			filerail_zip_folder(zip, resource_name) == -1
		)
		{
			perror("operations.h filerail_sendfile_handler");
			goto clean_up;
		}
	} else {
		if (
			(zip = zip_open(zip_filename, ZIP_DEFAULT_COMPRESSION_LEVEL, 'w')) == NULL ||
			filerail_zip_file(zip, resource_name)
			)
		{
			perror("operations.h filerail_sendfile_handler");
			goto clean_up;
		}
	}
	zip_close(zip);
	zip = NULL;
	printf("Resource zipped...\n");

	printf("Ready to send resource...\n");
  if (filerail_sendfile(fd, zip_filename) == -1) {
  	exit_status = -1;
  	goto clean_up;
  }
  printf("File transfer complete...\n");

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

int filerail_recvfile_handler(int fd, const char *resource_name, const char *resource_dir, const char *resource_path) {
  int exit_status;
  char current_dir[MAX_PATH_LENGTH], zip_filename[MAX_RESOURCE_LENGTH];

  exit_status = 0;

  if (filerail_getcwd(current_dir) == -1) {
  	exit_status = -1;
  	goto clean_up;
  }

  printf("Ready to receive file...\n");
  if (filerail_recvfile(fd, resource_path) == -1) {
  	exit_status = -1;
  	goto clean_up;
  }
  printf("File transfer complete...\n");

  if (filerail_cd(resource_dir) == -1) {
  	exit_status = -1;
  	goto clean_up;
  }

  zip_filename[0] = '\0';
  strcpy(zip_filename, resource_name);
  strcat(zip_filename, ".zip");

  printf("Unzipping...\n");
  if (zip_extract(zip_filename, resource_dir, zip_on_extract_entry, NULL) == -1) {
  	perror("operations.h filerail_recvfile_handler");
  	exit_status = -1;
  	goto clean_up;
  }

  if (filerail_cd(current_dir) == -1) {
  	exit_status = -1;
  	goto clean_up;
  }

  if (filerail_rm(resource_path) == -1) {
  	perror("operations.h filerail_recvfile_handler");
  	exit_status = -1;
  	goto clean_up;
  }

	clean_up:
	return exit_status;
}

#endif
