#ifndef _OPERATIONS_H
#define _OPERATIONS_H

#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <openssl/md5.h>

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
	const char *key_path,
	const char* ckpt_path);

int filerail_recvfile_handler(
	int fd,
	const char *resource_name,
	const char *resource_dir,
	const char *resource_path,
	const char *key_path,
	const char* ckpt_path);

// handles sending of files
int filerail_sendfile_handler(
	int fd,
	const char *resource_dir,
	const char *resource_name,
	struct stat *stat_resource,
	const char *key_path,
	const char* ckpt_path)
{
	int exit_status;
	off_t offset;
	char current_dir[MAX_PATH_LENGTH], zip_filename[MAX_RESOURCE_LENGTH], option;
	struct zip_t *zip;
	clock_t start, end;
	double cpu_time_used;
	unsigned char hash[MD5_DIGEST_LENGTH];
	filerail_command_header command;
	aesCryptoInfo ci;
	AES_keys K;
	filerail_response_header response;

	exit_status = 0;
	offset = 0;
	zip_filename[0] = '\0';
	strcpy(zip_filename, resource_name);
	strcat(zip_filename, ".zip");

	// store the current directory
	if (filerail_getcwd(current_dir) == -1) {
		exit_status = -1;
		goto clean_up;
	}

	// read the key file
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

	// change the directory to resource directory (because the zip lib requires the resource to be in current dir)
	if (filerail_cd(resource_dir) == -1) {
		exit_status = -1;
		goto clean_up;
	}

	// zip the resource
	PRINT(printf("Zipping resource...\n"));
	if (S_ISDIR(stat_resource->st_mode)) {
		if (
			(zip = zip_open(zip_filename, ZIP_DEFAULT_COMPRESSION_LEVEL, 'w')) == NULL ||
			!filerail_zip_folder(zip, resource_name)
		)
		{
			LOG(LOG_USER | LOG_ERR, "operations.h filerail_sendfile_handler");
			goto clean_up;
		}
	} else {
		if (
			(zip = zip_open(zip_filename, ZIP_DEFAULT_COMPRESSION_LEVEL, 'w')) == NULL ||
			!filerail_zip_file(zip, resource_name)
			)
		{
			LOG(LOG_USER | LOG_ERR, "operations.h filerail_sendfile_handler");
			goto clean_up;
		}
	}
	zip_close(zip);
	zip = NULL;
	PRINT(printf("Finished...\n"));

	// find the md5 hash of zipped file
	PRINT(printf("Generating md5 hash for zip file...\n"));
	if (filerail_md5(hash, zip_filename) == -1) {
		exit_status = -1;
		goto clean_up;
	}
	PRINT(printf("Finished...\n"));

	// advertise md5 hash to receiver (so that it can start checkpointing, and search for preivous checkpoints)
	PRINT(printf("Sending md5 hash...\n"));
	if (filerail_send(fd, (void *)hash, MD5_DIGEST_LENGTH, 0) == -1) {
		exit_status = -1;
		goto clean_up;
	}
	PRINT(printf("Finished...\n"));

	// receive response from receiver (whether it wants to resume from previous checkpoint or restart entire process)
	if (filerail_recv(fd, (void *)&command, sizeof(command), MSG_WAITALL) == -1) {
		exit_status = -1;
		goto clean_up;
	}
	// RESUME: receiver finds previous checkpoint, so it inquires if sender wants to resume
	if (command.command_type == RESUME) {
		// by deafult server agrees to resume
		if (!is_server) {
			printf("Do you wish to restart from previous checkpoint[Y/N] : ");
			scanf("%c", &option);
			getchar();
		} else {
			option = 'y';
		}
		if (option == 'Y' || option == 'y') {
			if (filerail_send_response_header(fd, OK) == -1) {
				exit_status = -1;
				goto clean_up;
			}
			// if sender agrees to resume, wait for receiver to send offset of zip file
			if (filerail_recv(fd, (void *)&offset, sizeof(offset), MSG_WAITALL) == -1) {
				exit_status = -1;
				goto clean_up;
			}
		} else {
			// if sender disagrees, abort the checkpoint resumption and restart transferring the whole file
			if (filerail_send_response_header(fd, ABORT) == -1) {
				exit_status = -1;
				goto clean_up;
			}
		}
	} else if (command.command_type == RESTART) {
		// receiver didn't find any checkpoint
		// do nothing, simply restart the whole process of sending file
	} else {
		PRINT(printf("PROTOCOL NOT FOLLOWED\n"););
	}

	// send the file
	PRINT(printf("Ready to send resource...\n"));
  start = clock();
  if (filerail_sendfile(fd, zip_filename, &K, offset) == -1) {
  	exit_status = -1;
  	goto clean_up;
  }
  end = clock();
  cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
  PRINT(printf("File transfer complete in %f seconds...\n", cpu_time_used));

  // wait for receiver to compute the hash, and verify integrity
  PRINT(printf("Verifying hash...\n"));
  if (filerail_recv(fd, (void *)&response, sizeof(response), MSG_WAITALL) == -1) {
  	exit_status = -1;
  	goto clean_up;
  }

  if (response.response_type == OK) {
  	PRINT(printf("md5 hash matched\n"));
  } else if (response.response_type == NO_INTEGRITY) {
  	PRINT(printf("md5 hash didn't match on receiver end\n"));
  	exit_status = -1;
  } else {
  	exit_status = -1;
  	PRINT(printf("PROTOCOL NOT FOLLOWED\n"));
  }
  PRINT(printf("Finished...\n"));

  // remove the zip file
	if (filerail_rm(zip_filename) == -1) {
		exit_status = -1;
		goto clean_up;
	}

	// go back to previosu directory
	if (filerail_cd(current_dir) == -1) {
		exit_status = -1;
		goto clean_up;
	}
	clean_up:
	zip_close(zip);
	return exit_status;
}

// handles receving of files
int filerail_recvfile_handler(
	int fd,
	const char *resource_name,
	const char *resource_dir,
	const char *resource_path,
	const char *key_path,
	const char* ckpt_path)
{
  int exit_status;
  off_t offset;
  char current_dir[MAX_PATH_LENGTH], zip_filename[MAX_RESOURCE_LENGTH];
	clock_t start, end;
	double cpu_time_used;
	unsigned char computed_hash[MD5_DIGEST_LENGTH], recvd_hash[MD5_DIGEST_LENGTH];
	char ckpt_resource_path[MAX_PATH_LENGTH], hex_str[2 * MD5_DIGEST_LENGTH];
	struct stat stat_path;
	filerail_checkpoint ckpt;
	filerail_response_header response;
	FILE *fp;
	aesCryptoInfo ci;
	AES_keys K;

	fp = NULL;
	offset = 0;
  exit_status = 0;

  // store current directory
  if (filerail_getcwd(current_dir) == -1) {
  	exit_status = -1;
  	goto clean_up;
  }

  // read the keys
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

	// wait for sender to advertise md5 hash
	PRINT(printf("Waiting for md5 hash...\n"););
	if (filerail_recv(fd, (void *)recvd_hash, MD5_DIGEST_LENGTH, 0) == -1) {
  	exit_status = -1;
  	goto clean_up;
  }
  PRINT(printf("Finished...\n"));

  // search for checkpoints
  PRINT(printf("Searching for checkpoints...\n"));
	ckpt_resource_path[0] = '\0';
	strcpy(ckpt_resource_path, ckpt_path);
	filerail_hash_to_string(recvd_hash, hex_str);
	strcat(ckpt_resource_path, "/");
	strcat(ckpt_resource_path, hex_str);

	if (filerail_is_exists(ckpt_resource_path, &stat_path)) {
		if (filerail_is_readable(ckpt_resource_path)) {
			// if checkpoint exists and it is readable, read the checkpoint
			if ((fp = fopen(ckpt_resource_path, "rb")) == NULL) {
				LOG(LOG_USER | LOG_ERR, "operations.h filerail_recvfile_handler fopen");
				exit_status = -1;
				goto restart;
			}
			if (fread((void *)&ckpt, 1, sizeof(ckpt), fp) != sizeof(ckpt)) {
				LOG(LOG_USER | LOG_ERR, "operations.h filerail_recvfile_handler fread");
				exit_status = -1;
				goto restart;
			}
			// check if resource path stored in checkpoint and new resource path sent by sender matches
			if (strcmp(ckpt.resource_path, resource_path) != 0) {
				PRINT(printf("Resource path in checkpoint doesn't match resource path of request...\n"));
				goto restart;
			}
			// check offset stored in checkpoint matches, the size of incompelete zip file (to make sure someone didnt modify)
			if (stat(resource_path, &stat_path) == -1) {
				LOG(LOG_USER | LOG_ERR, "operations.h filerail_recvfile_handler stat");
				exit_status = -1;
				goto restart;
			}
			if (ckpt.offset != stat_path.st_size) {
				PRINT(printf("Offset of checkpoint doesn't match resource size...\n"));
				goto restart;
			}
			// ask if sender want to resume
			if (filerail_send_command_header(fd, RESUME) == -1) {
				exit_status = -1;
				goto clean_up;
			}
			// wait for response
			if (filerail_recv(fd, (void *)&response, sizeof(response), MSG_WAITALL) == -1) {
				exit_status = -1;
				goto clean_up;
			}
			// if OK, resume from previous checkpoint
			if (response.response_type == OK) {
				// send offset to sender
				offset = ckpt.offset;
				if (filerail_send(fd, (void *)&offset, sizeof(offset), 0) == -1) {
					exit_status = -1;
					goto clean_up;
				}
				PRINT(printf("Resuming from previous checkpoint...\n"));
			} else if (response.response_type == ABORT) {
				// do nothing restart the process
			} else {
				PRINT(printf("PROTOCOL NOT FOLLOWED\n"));
				goto clean_up;
			}
		} else {
			LOG(LOG_USER | LOG_ERR, "You don't have read permission");
			exit_status = -1;
			goto clean_up;
		}
	} else {
		// restart the transfer process
		restart:
		PRINT(printf("No checkpoints found...\n"));
		if (filerail_send_command_header(fd, RESTART) == -1) {
			exit_status = -1;
			goto clean_up;
		}
	}

	// recv the file
  PRINT(printf("Waiting for server to respond...\n"));
  start = clock();
  if (filerail_recvfile(fd, resource_path, &K, offset, ckpt_resource_path, resource_path) == -1) {
  	exit_status = -1;
  	goto clean_up;
  }
  end = clock();
  cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
  PRINT(printf("File transfer complete in %f seconds...\n", cpu_time_used));

  // go back to resource directory
  if (filerail_cd(resource_dir) == -1) {
  	exit_status = -1;
  	goto clean_up;
  }

  zip_filename[0] = '\0';
  strcpy(zip_filename, resource_name);
  strcat(zip_filename, ".zip");

  // generate the md5 hash
  PRINT(printf("Generating hash...\n"));
	if (filerail_md5(computed_hash, zip_filename) == -1) {
		exit_status = -1;
		goto clean_up;
	}
	PRINT(printf("Finished...\n"));

	// compute the hash of received zip file and verify it with advertised md5 hash
  PRINT(printf("Verifying hash...\n"));
  if (memcmp(computed_hash, recvd_hash, MD5_DIGEST_LENGTH) == 0) {
  	if (filerail_send_response_header(fd, OK) == -1) {
  		exit_status = -1;
  		goto clean_up;
  	}
  } else {
  	if (filerail_send_response_header(fd, NO_INTEGRITY) == -1) {
  		exit_status = -1;
  		goto clean_up;
  	}
  	PRINT(printf("md5 hash doesn't match...\n"));
  	goto hash_failed;
  }
  PRINT(printf("Finished...\n"));

  // if hash matches unzip the resource
  PRINT(printf("Unzipping...\n"));
  if (zip_extract(zip_filename, resource_dir, zip_on_extract_entry, NULL) == -1) {
  	LOG(LOG_USER | LOG_ERR, "operations.h filerail_recvfile_handler");
  	exit_status = -1;
  	goto clean_up;
  }
  PRINT(printf("Finished...\n"));

  // oops
  hash_failed:
  // go back to previous directory
  if (filerail_cd(current_dir) == -1) {
  	exit_status = -1;
  	goto clean_up;
  }

  // remove the zip file
  if (filerail_rm(resource_path) == -1) {
  	exit_status = -1;
  	goto clean_up;
  }

  // remove the checkpoints
  if (filerail_rm(ckpt_resource_path) == -1) {
  	exit_status = -1;
  	goto clean_up;
  }

	clean_up:
	if (fp != NULL) {
		fclose(fp);
	}
	return exit_status;
}

#endif
