#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include<syslog.h>

#include "filerail/global.h"
#include "filerail/constants.h"
#include "filerail/protocol.h"
#include "filerail/socket.h"
#include "filerail/utils.h"
#include "filerail/operations.h"

int main(int argc, char *argv[]) {
	int opt;
	extern int verbose;
	extern char *optarg;
	extern int optopt;

	int fd, clifd, exit_status;
	pid_t pid;
	socklen_t addrlen;
	struct sockaddr_in cliaddr;
	char *ip, *port;
	filerail_command_header command;
	filerail_resource_header resource;
	filerail_response_header response;
	struct stat stat_path;
	char resource_name[MAX_RESOURCE_LENGTH], resource_dir[MAX_PATH_LENGTH], resource_path[MAX_PATH_LENGTH];

	exit_status = 0;

	ip = port = NULL;
	while ((opt = getopt(argc, argv, "uvqi:p:")) != -1) {
		switch(opt) {
			case 'u' : {
				printf("usage: -v [-i ipv4 address] [-p port]\n");
				goto parent_clean_up;
			}
			case 'v': {
				verbose = 1;
				break;
			}
			case 'q': {
				verbose = 0;
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
			case '?' : {
				if (optopt == 'i' || optopt == 'p') {
					printf("-%c option requires value\n", optopt);
					goto parent_clean_up;
				} else {
					printf("-%c option is unknown\n", optopt);
				}
				break;
			}
			default: {
				perror("filerail_server main");
				goto parent_clean_up;
			}
		}
	}

	if (ip == NULL || port == NULL) {
		printf("-i and -p are required options\n");
		goto parent_clean_up;
	}

	if ((fd = filerail_create_tcp_server(ip, port)) == -1) {
		exit_status = -1;
		goto parent_clean_up;
	}

	while (true) {
		memset(&cliaddr, 0, sizeof(cliaddr));
		clifd = filerail_accept(fd, (struct sockaddr*)&cliaddr, &addrlen);
		if (clifd == -1) {
			exit_status = -1;
			goto parent_clean_up;
		}
		if (filerail_who(clifd, "joined") == -1) {
			exit_status = -1;
			goto parent_clean_up;
		}

		pid = fork();
		if (pid == -1) {
			perror("filerail_server.c fork");
			exit_status = -1;
			goto parent_clean_up;
		} else if (pid == 0) {
			close(fd);

			if (filerail_recv(clifd, (void *)&command, sizeof(command), MSG_WAITALL) == -1) {
				exit_status = -1;
				goto child_clean_up;
			}

			if (command.command_type == PUT) {
				if (filerail_recv(clifd, (void *)&resource, sizeof(resource), MSG_WAITALL) == -1) {
					exit_status = -1;
					goto child_clean_up;
				}
				resource_path[0] = '\0';
				strcpy(resource_path, resource.resource_dir);
				strcat(resource_path, "/");
				strcat(resource_path, resource.resource_name);
				if (filerail_check_storage_size(resource.resource_size)) {
					if (filerail_is_exists(resource_path, &stat_path)) {
						if (filerail_is_writeable(resource_path)) {
							if (filerail_send_response_header(clifd, DUPLICATE_RESOURCE_NAME) == -1) {
								exit_status = -1;
								goto child_clean_up;
							}
							if (filerail_recv(clifd, (void*)&command, sizeof(command), MSG_WAITALL) == -1) {
								exit_status = -1;
								goto child_clean_up;
							}
							if (command.command_type == OVERWRITE) {
								if (filerail_rm(resource_path) == -1) {
									exit_status = -1;
									goto child_clean_up;
								}
								put_file:
								strcat(resource_path, ".zip");
								if (filerail_recvfile_handler(clifd, resource.resource_name, resource.resource_dir, resource_path) == -1) {
									exit_status = -1;
								}
							} else if (command.command_type == ABORT) {
								// do nothing wait for clean up
							} else {
								printf("PROTOCOL NOT FOLLOWED\n");
							}
						} else {
							if (filerail_send_response_header(clifd, NO_ACCESS) == -1) {
								exit_status = -1;
							}
						}
					} else {
						// if file doesnt exist, check if dir is accessible
						if (stat(resource.resource_dir, &stat_path) == -1) {
							exit_status = -1;
							perror("filerail_server main");
							if (filerail_send_response_header(clifd, NOT_FOUND) == -1) {
								exit_status = -1;
							}
						} else {
							if (access(resource.resource_dir, W_OK) == 0) {
								if (filerail_send_response_header(clifd, OK) == -1) {
									exit_status = -1;
									goto child_clean_up;
								}
								goto put_file;
							} else {
								if (filerail_send_response_header(clifd, NO_ACCESS) == -1) {
									exit_status = -1;
								}
							}
						}
					}
				} else {
					if (filerail_send_response_header(clifd, INSUFFICIENT_SPACE) == -1) {
						exit_status = -1;
					}
				}
			} else if (command.command_type == PING) {
				if (filerail_send_response_header(clifd, PONG) == -1) {
					exit_status = -1;
				}
			} else if(command.command_type == GET) {
				if (filerail_recv(clifd, (void *)&resource, sizeof(resource), MSG_WAITALL) == -1) {
					exit_status = -1;
					goto child_clean_up;
				}
				resource_path[0] = '\0';
				strcpy(resource_path, resource.resource_dir);
				strcat(resource_path, "/");
				strcat(resource_path, resource.resource_name);
				if (filerail_is_exists(resource_path, &stat_path)) {
					if (filerail_is_readable(resource_path)) {
						if (filerail_is_file(&stat_path) || filerail_is_dir(&stat_path)) {
							if (filerail_send_response_header(clifd, OK) == -1) {
								exit_status = -1;
							}
							if (filerail_send_resource_header(clifd, "\0", "\0", stat_path.st_size) == -1) {
								exit_status = -1;
								goto child_clean_up;
							}
							if (filerail_recv(clifd, (void*)&response, sizeof(response), MSG_WAITALL) == -1) {
								exit_status = -1;
								goto child_clean_up;
							}
							if (response.response_type == ABORT) {
								// do nothing wait for clean up
							} else if (response.response_type == OK) {
								if (filerail_sendfile_handler(clifd, resource.resource_dir, resource.resource_name, &stat_path) == -1) {
									exit_status = -1;
								}
							} else {
								printf("PROTOCOL NOT FOLLOWED\n");
							}
						} else {
							if (filerail_send_response_header(clifd, BAD_RESOURCE) == -1) {
								exit_status = -1;
							}
						}
					} else {
						if (filerail_send_response_header(clifd, NO_ACCESS) == -1) {
							exit_status = -1;
						}
					}
				} else {
					if (filerail_send_response_header(clifd, NOT_FOUND) == -1) {
						exit_status = -1;
					}
				}
			} else {
				printf("PROTOCOL NOT FOLLOWED\n");
			}
			goto child_clean_up;
		}
	}

	child_clean_up:
	filerail_close(clifd);
	PRINT(printf("Client connection closed\n"));
	return exit_status;

	parent_clean_up:
	filerail_close(clifd);
	filerail_close(fd);
	return exit_status;
}
