#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>

#include "filerail/global.h"
#include "filerail/constants.h"
#include "filerail/socket.h"
#include "filerail/utils.h"
#include "filerail/operations.h"

int main(int argc, char *argv[]) {
	int opt;
	extern int verbose;
	extern char *optarg;
	extern int optopt;

	int fd, exit_status;
	char c, option, resource_name[MAX_RESOURCE_LENGTH], resource_dir[MAX_PATH_LENGTH], resource_path[MAX_PATH_LENGTH];
	char *ip, *port, *operation, *res_path, *des_path;
	struct stat stat_path;
	filerail_command_header command;
	filerail_response_header response;
	filerail_resource_header resource;

	exit_status = 0;

	ip = port = operation = res_path = des_path = NULL;
	while ((opt = getopt(argc, argv, "uvi:p:o:r:d:")) != -1) {
		switch(opt) {
			case 'u' : {
				printf("usage: -v [-i ipv4 address] [-p port] [-o operation] [-r resource path] [-d destination path]\n");
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
			case '?' : {
				if (optopt == 'i' || optopt == 'p' || optopt == 'o' || optopt == 'r' || optopt == 'd') {
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

	if (ip == NULL || port == NULL || operation == NULL) {
		printf("-i, -p and -a are required options\n");
		goto clean_up;
	}

	if ((fd = filerail_connect_to_tcp_server(ip, port)) == -1) {
		exit_status = -1;
		goto clean_up;
	}

	if (strcmp(operation, "ping") == 0) {
		if (filerail_send_response_header(fd, PING) == -1) {
			exit_status = -1;
		}
		if (filerail_recv(fd, (void *)&response, sizeof(response), MSG_WAITALL) == -1) {
			exit_status = -1;
			goto clean_up;
		}
		printf("PONG\n");
	} else if (strcmp(operation, "put") == 0) {
		if (res_path == NULL || des_path == NULL) {
			printf("-r and -d are required options for \"%s\"\n", operation);
			goto clean_up;
		}
		if (filerail_is_exists(res_path, &stat_path)) {
			if (filerail_is_readable(res_path)) {
				if (filerail_is_file(&stat_path) || filerail_is_dir(&stat_path)) {
					if (filerail_send_command_header(fd, PUT) == -1) {
						exit_status = -1;
						goto clean_up;
					}
					if (filerail_parse_resource_path(res_path, resource_name, resource_dir)) {
						if (filerail_send_resource_header(fd, resource_name, des_path, stat_path.st_size) == -1) {
							exit_status = -1;
							goto clean_up;
						}
						if (filerail_recv(fd, (void*)&response, sizeof(response), MSG_WAITALL) == -1) {
							exit_status = -1;
							goto clean_up;
						}
						if (response.response_type == NO_ACCESS) {
							printf("You don't have write permission for %s on server\n", des_path);
							goto clean_up;
						} else if (response.response_type == DUPLICATE_RESOURCE_NAME) {
							printf("%s already exists at %s, do you wish to re-write[Y/N]: ", resource_name, des_path);
							scanf("%c", &option);
							getchar();
							if (option == 'Y' || option == 'y') {
								if (filerail_send_response_header(fd, OVERWRITE) == -1) {
									exit_status = -1;
									goto clean_up;
								}
								put_file:
								printf("Starting transfer process...\n");
								if (filerail_sendfile_handler(fd, resource_dir, resource_name, &stat_path) == -1) {
									exit_status = -1;
								}
							} else {
								if (filerail_send_response_header(fd, ABORT) == -1) {
									exit_status = -1;
								}
							}
						} else if (response.response_type == NOT_FOUND) {
							printf("%s not able to locate\n", des_path);
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
		if (res_path == NULL || des_path == NULL) {
			printf("-r and -d are required options for \"%s\"\n", operation);
			goto clean_up;
		}
		if (filerail_parse_resource_path(res_path, resource_name, resource_dir)) {
			resource_path[0] = '\0';
			strcpy(resource_path, des_path);
			strcat(resource_path, "/");
			strcat(resource_path, resource_name);
			if (filerail_is_exists(resource_path, &stat_path)) {
				printf("%s already exists at %s, do you wish to re-write[Y/N]: ", resource_name, resource_dir);
				scanf("%c", &option);
				getchar();
				if (option == 'Y' || option == 'y') {
					if (filerail_is_writeable(resource_path)) {
						get_file:
						if (filerail_send_command_header(fd, GET) == -1) {
							exit_status = -1;
							goto clean_up;
						}
						if (filerail_send_resource_header(fd, resource_name, resource_dir, 0) == -1) {
							exit_status = -1;
							goto clean_up;
						}
						if (filerail_recv(fd, (void *)&response, sizeof(response), MSG_WAITALL) == -1) {
							exit_status = -1;
							goto clean_up;
						}
						if (response.response_type == OK) {
							if (filerail_recv(fd, (void *)&resource, sizeof(resource), MSG_WAITALL) == -1) {
								exit_status = -1;
								goto clean_up;
							}
							if (filerail_check_storage_size(resource.resource_size)) {
								if (filerail_send_response_header(fd, OK) == -1) {
									exit_status = -1;
								}
								strcat(resource_path, ".zip");
								if (filerail_recvfile_handler(fd, resource_name, des_path, resource_path) == -1) {
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
				// check if directory is valid
				if (stat(des_path, &stat_path) == -1) {
					printf("%s is invalid directory\n", des_path);
					exit_status = -1;
				} else {
					if (access(des_path, W_OK) == 0) {
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
