#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "filerail/constants.h"
#include "filerail/socket.h"
#include "filerail/utils.h"
#include "filerail/operations.h"

int main(int argc, char *argv[]) {
	int fd, exit_status;
	char c, option, resource_name[MAX_RESOURCE_LENGTH], resource_dir[MAX_PATH_LENGTH], resource_path[MAX_PATH_LENGTH];
	struct stat stat_path;
	filerail_command_header command;
	filerail_response_header response;
	filerail_resource_header resource;

	exit_status = 0;
	if (argc < 4) {
		printf("syntax: ./filerail_client <ipv4 address> <port number> <get/put/ping> <...>\n");
		goto clean_up;
	}

	if ((fd = filerail_connect_to_tcp_server(argv[1], argv[2])) == -1) {
		exit_status = -1;
		goto clean_up;
	}

	if (strcmp(argv[3], "ping") == 0) {
		if (filerail_send_response_header(fd, PING) == -1) {
			exit_status = -1;
		}
		if (filerail_recv(fd, (void *)&response, sizeof(response), MSG_WAITALL) == -1) {
			exit_status = -1;
			goto clean_up;
		}
		printf("PONG\n");
	} else if (strcmp(argv[3], "put") == 0) {
		if (argc < 6) {
			printf("Not enough arguements\n");
			goto clean_up;
		}
		if (filerail_is_exists(argv[4], &stat_path)) {
			if (filerail_is_readable(argv[4])) {
				if (filerail_is_file(&stat_path) || filerail_is_dir(&stat_path)) {
					if (filerail_send_command_header(fd, PUT) == -1) {
						exit_status = -1;
						goto clean_up;
					}
					if (filerail_parse_resource_path(argv[4], resource_name, resource_dir)) {
						if (filerail_send_resource_header(fd, resource_name, argv[5], stat_path.st_size) == -1) {
							exit_status = -1;
							goto clean_up;
						}
						if (filerail_recv(fd, (void*)&response, sizeof(response), MSG_WAITALL) == -1) {
							exit_status = -1;
							goto clean_up;
						}
						if (response.response_type == NO_ACCESS) {
							printf("You don't have write permission for %s on server\n", argv[5]);
							goto clean_up;
						} else if (response.response_type == DUPLICATE_RESOURCE_NAME) {
							printf("%s already exists at %s, do you wish to re-write[Y/N]: ", resource_name, argv[5]);
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
							printf("%s not able to locate\n", argv[5]);
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
					printf("%s is neither file or directory\n", argv[4]);
				}
			} else {
				printf("You don't have read permission for %s\n", argv[4]);
			}
		} else {
			printf("%s doesn't exists\n", argv[4]);
		}
	} else if(strcmp(argv[3], "get") == 0) {
		if (filerail_parse_resource_path(argv[4], resource_name, resource_dir)) {
			resource_path[0] = '\0';
			strcpy(resource_path, argv[5]);
			strcat(resource_path, "/");
			strcat(resource_path, resource_name);
			if (filerail_is_exists(resource_path, &stat_path)) {
				printf("%s already exists at %s, do you wish to re-write[Y/N]: \n", resource_name, resource_dir);
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
								if (filerail_recvfile_handler(fd, resource_name, argv[5], resource_path) == -1) {
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
						printf("You don't have write permission for %s\n", argv[5]);
					}
				}
			} else {
				// check if directory is valid
				if (stat(argv[5], &stat_path) == -1) {
					printf("%s is invalid directory\n", argv[5]);
					exit_status = -1;
				} else {
					if (access(argv[5], W_OK) == 0) {
						goto get_file;
					}	else {
						printf("You don't have write permission for %s\n", argv[5]);
					}
				}
			}
			printf("Done\n");
		}
	} else {
		printf("Invalid command\n");
	}

	clean_up:
	printf("\u263A , Bye\n");
	filerail_close(fd);
	return exit_status;
}
