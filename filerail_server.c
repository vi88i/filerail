#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>

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

	extern int is_server;

	int fd, clifd, exit_status;
	bool should_resolve;
	pid_t pid, sid;
	socklen_t addrlen;
	struct sockaddr_in cliaddr;
	char *ip, *port, *key_path, *ckpt_path;
	filerail_command_header command;
	filerail_resource_header resource;
	filerail_response_header response;
	struct stat stat_path;
	char resource_path[MAX_PATH_LENGTH];

	exit_status = 0;
	should_resolve = false;
	is_server = 1;

	ip = port = key_path = ckpt_path = NULL;
	while ((opt = getopt(argc, argv, "uvqi:p:k:m:c:n")) != -1) {
		switch(opt) {
			case 'u' : {
				printf(
					"usage: -v [-i ipv4 address]"
					" [-p port] [-k key directory]"
					" [-c checkpoint directory] [-n dns resolution]\n");
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
				if (optopt == 'i' || optopt == 'p' || optopt == 'k' || optopt == 'c') {
					printf("-%c option requires value\n", optopt);
					goto parent_clean_up;
				} else {
					printf("-%c option is unknown\n", optopt);
				}
				break;
			}
			default: {
				LOG(LOG_ERR | LOG_USER, "filerail_server getopt");
				goto parent_clean_up;
			}
		}
	}

	if (ip == NULL || port == NULL || key_path == NULL || ckpt_path == NULL) {
		printf("-i, -p, -k and -c are required options\n");
		goto parent_clean_up;
	}

	if (!filerail_is_exists(key_path, &stat_path)) {
		printf("Couldn't open key directory\n");
		goto parent_clean_up;
	}

	if (!filerail_is_exists(ckpt_path, &stat_path)) {
		if (filerail_mkdir(ckpt_path) == -1) {
			printf("Failed to create checkpoints directory at %s\n", ckpt_path);
			exit_status = -1;
			goto parent_clean_up;
		}
	}

	if (should_resolve && (filerail_dns_resolve(ip) == -1)) {
		goto parent_clean_up;
	}

	pid = fork();
	if (pid < 0) {
		exit_status = -1;
		goto parent_clean_up;
	}

	if (pid > 0) {
		goto parent_clean_up;
	}

	umask(0);
	openlog("filerail", LOG_PID, LOG_USER);

	sid = setsid();
	if (sid < 0) {
		exit_status = -1;
		goto parent_clean_up;
	}

	if (chdir("/") < 0) {
		exit_status = -1;
		goto parent_clean_up;
	}

	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	close(STDERR_FILENO);

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
			LOG(LOG_ERR | LOG_USER, "filerail_server fork");
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
								if (
									filerail_recvfile_handler(
											clifd,
											resource.resource_name,
											resource.resource_dir,
											resource_path,
											key_path,
											ckpt_path
									) == -1) {
									exit_status = -1;
								}
							} else if (command.command_type == ABORT) {
								// do nothing wait for clean up
							} else {
								LOG(LOG_INFO | LOG_USER, "PROTOCOL NOT FOLLOWED");
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
							LOG(LOG_ERR | LOG_USER, "filerail_server main stat");
							if (filerail_send_response_header(clifd, NOT_FOUND) == -1) {
								exit_status = -1;
							}
						} else {
							if (filerail_is_writeable(resource.resource_dir)) {
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
								if (
									filerail_sendfile_handler(
										clifd,
										resource.resource_dir,
										resource.resource_name,
										&stat_path,
										key_path,
										ckpt_path) == -1) {
									exit_status = -1;
								}
							} else {
								LOG(LOG_INFO | LOG_USER, "PROTOCOL NOT FOLLOWED");
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
				LOG(LOG_ERR | LOG_USER, "PROTOCOL NOT FOLLOWED");
			}
			child_clean_up:
			filerail_close(clifd);
			if (exit_status == -1) {
				LOG(LOG_INFO | LOG_USER, "child process, FAILED");
			} else {
				LOG(LOG_INFO | LOG_USER, "child process, SUCCESS");
			}
			return exit_status;
		}
	}

	parent_clean_up:
	filerail_close(clifd);
	filerail_close(fd);
	if (exit_status == -1) {
		LOG(LOG_INFO | LOG_USER, "parent process, FAILED");
	} else {
		LOG(LOG_INFO | LOG_USER, "parent process, SUCCESS");
	}
	closelog();
	while (wait(NULL) > 0);
	return exit_status;
}
