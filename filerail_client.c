#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>

#include "filerail/global.h"
#include "filerail/constants.h"
#include "filerail/socket.h"
#include "filerail/utils.h"
#include "filerail/crypto.h"
#include "filerail/operations.h"

int main(int argc, char *argv[]) {
	int opt;
	extern int verbose;
	extern char *optarg;
	extern int optopt;

	int fd, exit_status;
	bool should_resolve;
	char option, resource_name[MAX_RESOURCE_LENGTH], resource_dir[MAX_PATH_LENGTH], resource_path[MAX_PATH_LENGTH];
	char *ip, *port, *operation, *res_path, *des_path, *key_path, *ckpt_path;
	struct stat stat_path;
	filerail_AES_keys K;
	filerail_response_header response;
	filerail_resource_header resource;

	should_resolve = false;
	exit_status = 0;

	// parse command line arguement
	ip = port = operation = res_path = des_path = key_path = ckpt_path = NULL;
	while ((opt = getopt(argc, argv, "uvi:p:o:r:d:k:c:n")) != -1) {
		switch(opt) {
			case 'u' : {
				printf(
					"usage: -v [-i ipv4 address] [-p port]"
					" [-o operation] [-r resource path]"
					" [-d destination path] [-k key directory]"
					" [-c checkpoint directory] [-n dns resolution]\n"
				);
				goto clean_up;
			}
			case 'v': {
				verbose = 1;
				break;
			}
			case 'i': {
				ip = optarg;
				break;
			}
			case 'p': {
				port = optarg;
				break;
			}
			case 'o': {
				operation = optarg;
				break;
			}
			case 'r': {
				res_path = optarg;
				break;
			}
			case 'd': {
				des_path = optarg;
				break;
			}
			case 'k' : {
				key_path = optarg;
				break;
			}
			case 'c' : {
				ckpt_path = optarg;
				break;
			}
			case 'n' : {
				should_resolve = true;
				break;
			}
			case '?' : {
				if (
					optopt == 'i' || optopt == 'p' || optopt == 'o' || optopt == 'r' ||
					optopt == 'd' || optopt == 'k' || optopt == 'c'
					)
				{
					printf("-%c option requires value\n", optopt);
					goto clean_up;
				} else {
					printf("-%c option is unknown\n", optopt);
				}
				break;
			}
			default: {
				perror("filerail_client main");
				goto clean_up;
			}
		}
	}

	// check important fields
	if (ip == NULL || port == NULL || operation == NULL || key_path == NULL || ckpt_path == NULL) {
		printf("-i, -p, -o, -k and -c are required options\n");
		goto clean_up;
	}

	// check if key file exists
	// check if key file exists
	if (!filerail_is_exists(key_path, &stat_path)) {
		printf("Couldn't open key file\n");
		goto clean_up;
	}
	// read keys
	if (filerail_read_AES_keys(key_path, &K) == -1) {
		exit_status = -1;
		goto clean_up;
	}

	// create checkpoint dir, if it doesn't exist
	if (!filerail_is_exists(ckpt_path, &stat_path)) {
		if (filerail_mkdir(ckpt_path) == -1) {
			printf("Failed to create checkpoints directory at %s\n", ckpt_path);
			exit_status = -1;
			goto clean_up;
		}
	}

	// resolve the ip field, if dns resolution option provided
	if (should_resolve && (filerail_dns_resolve(ip) == -1)) {
		goto clean_up;
	}

	// connect to tcp server
	if ((fd = filerail_connect_to_tcp_server(ip, port)) == -1) {
		exit_status = -1;
		goto clean_up;
	}

	if (strcmp(operation, "ping") == 0) {
		if (filerail_send_command_header(fd, PING) == -1) {
			exit_status = -1;
			goto clean_up;
		}
		if (filerail_recv_response_header(fd, &response) == -1) {
			exit_status = -1;
			goto clean_up;
		}
		if (response.response_type == PONG) {
			printf("PONG\n");
		} else {
			printf("PROTOCOL NOT FOLLOWED\n");
		}
	} else if (strcmp(operation, "put") == 0) {
		// res path and des path is necessary
		if (res_path == NULL || des_path == NULL) {
			printf("-r and -d are required options for \"%s\"\n", operation);
			goto clean_up;
		}
		// check if resource exists
		if (filerail_is_exists(res_path, &stat_path)) {
			// check if resource is readable
			if (filerail_is_readable(res_path)) {
				// check if resource is file or directory
				if (filerail_is_file(&stat_path) || filerail_is_dir(&stat_path)) {
					// send the command
					if (filerail_send_command_header(fd, PUT) == -1) {
						exit_status = -1;
						goto clean_up;
					}
					// parse resource path
					if (filerail_parse_resource_path(res_path, resource_name, resource_dir)) {
						// send resource name, destination dir (on server) and resource size (unzipped)
						if (filerail_send_resource_header(fd, resource_name, des_path, stat_path.st_size) == -1) {
							exit_status = -1;
							goto clean_up;
						}
						// server performs checks, and sends response
						if (filerail_recv_response_header(fd, &response) == -1) {
							exit_status = -1;
							goto clean_up;
						}
						if (response.response_type == NO_ACCESS) {
							printf("You don't have write permission for %s on server\n", des_path);
							goto clean_up;
						} else if (response.response_type == DUPLICATE_RESOURCE_NAME) {
							// if the resource with same name exists
							// check if user wants to overwrite
							printf("%s already exists at %s, do you wish to re-write[Y/N]: ", resource_name, des_path);
							scanf("%c", &option);
							getchar();
							if (option == 'Y' || option == 'y') {
								if (filerail_send_response_header(fd, OVERWRITE) == -1) {
									exit_status = -1;
									goto clean_up;
								}
								// if overwrite is ok, start the sending process
								put_file:
								printf("Starting transfer process...\n");
								if (filerail_sendfile_handler(fd, resource_dir, resource_name, &stat_path, ckpt_path, &K) == -1) {
									exit_status = -1;
								}
							} else {
								// if sender doesn't want to overwrite, ABORT the process
								if (filerail_send_response_header(fd, ABORT) == -1) {
									exit_status = -1;
								}
							}
						} else if (response.response_type == NOT_FOUND) {
							printf("Unable to locate %s\n", des_path);
						} else if (response.response_type == INSUFFICIENT_SPACE) {
							printf("Insufficient space on server\n");
						} else if (response.response_type == OK) {
							goto put_file;
						} else {
							printf("PROTOCOL NOT FOLLOWED\n");
						}
					}
					printf("Done\n");
				} else {
					printf("%s is neither file or directory\n", res_path);
				}
			} else {
				printf("You don't have read permission for %s\n", res_path);
			}
		} else {
			printf("%s doesn't exists\n", res_path);
		}
	} else if(strcmp(operation, "get") == 0) {
		// res path and des path is necessary
		if (res_path == NULL || des_path == NULL) {
			printf("-r and -d are required options for \"%s\"\n", operation);
			goto clean_up;
		}
		// parse resource path
		if (filerail_parse_resource_path(res_path, resource_name, resource_dir)) {
			resource_path[0] = '\0';
			strcpy(resource_path, des_path);
			strcat(resource_path, "/");
			strcat(resource_path, resource_name);
			// check if resource name already exists in destination path on client side
			if (filerail_is_exists(resource_path, &stat_path)) {
				// ask if the user want to overwrite
				printf("%s already exists at %s, do you wish to re-write[Y/N]: ", resource_name, resource_dir);
				scanf("%c", &option);
				getchar();
				// if user wants to overwrite
				if (option == 'Y' || option == 'y') {
					// check if resource is writeable
					if (filerail_is_writeable(resource_path)) {
						// start the get process
						get_file:
						if (filerail_send_command_header(fd, GET) == -1) {
							exit_status = -1;
							goto clean_up;
						}
						// request the sender for resource
						if (filerail_send_resource_header(fd, resource_name, resource_dir, 0) == -1) {
							exit_status = -1;
							goto clean_up;
						}
						// check if sender is ready to transfer
						if (filerail_recv_response_header(fd, &response) == -1) {
							exit_status = -1;
							goto clean_up;
						}
						if (response.response_type == OK) {
							// if sender is OK, get the resource size from sender
							if (filerail_recv_resource_header(fd, &resource) == -1) {
								exit_status = -1;
								goto clean_up;
							}
							// check the storage size
							if (filerail_check_storage_size(resource.resource_size)) {
								// if feasible send OK
								if (filerail_send_response_header(fd, OK) == -1) {
									exit_status = -1;
								}
								// and start the file transfer process
								strcat(resource_path, ".zip");
								if (filerail_recvfile_handler(fd, resource_name, des_path, resource_path, ckpt_path, &K) == -1) {
									exit_status = -1;
								}
							} else {
								printf("Insufficient storage\n");
								if (filerail_send_response_header(fd, ABORT) == -1) {
									exit_status = -1;
								}
							}
						} else if (response.response_type == NOT_FOUND) {
							printf("Resource not found\n");
						} else if (response.response_type == NO_ACCESS) {
							printf("Server doesn't have read permission for %s\n", resource_dir);
						} else if (response.response_type == BAD_RESOURCE) {
							printf("Resource is neither file or directory\n");
						} else {
							printf("PROTOCOL NOT FOLLOWED\n");
						}
					} else {
						printf("You don't have write permission for %s\n", des_path);
					}
				}
			} else {
				// check if resource path is not valid, check if at least resource dir (destination dir) exists
				if (stat(des_path, &stat_path) == -1) {
					printf("%s is invalid directory\n", des_path);
					exit_status = -1;
				} else {
					// if it exists check, if des path is writeable
					if (filerail_is_writeable(des_path)) {
						// start the file transfer process
						goto get_file;
					}	else {
						printf("You don't have write permission for %s\n", des_path);
					}
				}
			}
			printf("Done\n");
		}
	} else {
		printf("Invalid command\n");
	}

	clean_up:
	filerail_close(fd);
	if (exit_status == -1) {
		PRINT(printf("[❌ ] FAILED\n"));
	}
	return exit_status;
}
