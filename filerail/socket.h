#ifndef _SOCKET_H
#define _SOCKET_H

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>

#include "protocol.h"
#include "constants.h"

#define min(a, b) ((a) > (b) ? (b) : (a))

static int filerail_socket(int domain, int type, int protocol);
static int filerail_bind(int fd, const struct sockaddr *addr, socklen_t addrlen);
static int filerail_connect(int fd, const struct sockaddr *addr, socklen_t addrlen);
static int filerail_listen(int fd, int backlog);
static int filerail_setsockopt(int fd, int level, int option, const void *optval, socklen_t optlen);
static int filerail_is_fd_valid(int fd);
int firerail_accept(int fd, struct sockaddr *addr, socklen_t *addrlen);
int filerail_close(int fd);
int filerail_create_tcp_server(char *ip, char *port);
int filerail_connect_to_tcp_server(char *ip, char *port);
int filerail_who(int fd, const char *action);
int filerail_send_command_header(int fd, int type);
int filerail_send_response_header(int fd, int type);
int filerail_send_resource_header(int fd, char *name, char *dir, off_t size);
int filerail_sendfile(int fd, const char *zip_filename);
int filerail_recvfile(int fd, const char *zip_filename);

static int filerail_socket(int domain, int type, int protocol) {
	int fd;

	fd = socket(domain, type, protocol);
	if (fd == -1) {
		perror("socket.h filerail_socket");
	}
	return fd;
}

static int filerail_bind(int fd, const struct sockaddr *addr, socklen_t addrlen) {
	int ret;

	ret = bind(fd, addr, addrlen);
	if (ret == -1) {
		perror("socket.h filerail_bind");
	}
	return ret;
}

static int filerail_connect(int fd, const struct sockaddr *addr, socklen_t addrlen) {
	int ret;

	ret = connect(fd, addr, addrlen);
	if (ret == -1) {
		perror("socket.h filerail_connect");
	}
	return ret;
}

static int filerail_listen(int fd, int backlog) {
	int ret;

	ret = listen(fd, backlog);
	if (ret == -1) {
		perror("socket.h filerail_listen");
	}
	return ret;
}

static int filerail_setsockopt(int fd, int level, int option, const void *optval, socklen_t optlen) {
	int ret;

	ret = setsockopt(fd, level, option, optval, optlen);
	if (ret == -1) {
		perror("socket.h filerail_setsockopt");
	}
	return ret;
}

static int filerail_is_fd_valid(int fd) {
	return fcntl(fd, F_GETFD) != -1 || errno == EBADFD;
}

int filerail_accept(int fd, struct sockaddr *addr, socklen_t *addrlen) {
	int clifd;

	clifd = accept(fd, addr, addrlen);
	if (clifd == -1) {
		perror("socket.h filerail_accept");
	}
	return clifd;
}

int filerail_close(int fd) {
	int ret;

	ret = -1;
	if (filerail_is_fd_valid(fd)) {
		ret = close(fd);
		if (ret == -1) {
			perror("socket.h filerail_close");
		}
	}
	return ret;
}

int filerail_create_tcp_server(char *ip, char *port) {
	int fd;
	const int optval = 1;
	socklen_t addrlen, optlen;
	struct sockaddr_in addr;

	optlen = sizeof(const int);
	addrlen = (socklen_t)sizeof(struct sockaddr_in);
	memset(&addr, 0, addrlen);

	addr.sin_addr.s_addr = inet_addr(ip);
	addr.sin_family = AF_INET;
	addr.sin_port = htons(atoi(port));

	if (
		(fd = filerail_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1 ||
		filerail_bind(fd, (const struct sockaddr*)&addr, addrlen) == -1 ||
		filerail_setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const void*)&optval, optlen) == -1 ||
		filerail_setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, (const void*)&optval, optlen) == -1 ||
		filerail_listen(fd, BACKLOG) == -1
	) {
		goto clean_up;
	}

	printf("[✔️ ] Listening for connections on %s:%s\n", ip, port);

	return fd;

	clean_up:
	filerail_close(fd);
	return -1;
}

int filerail_connect_to_tcp_server(char *ip, char *port) {
	int fd;
	socklen_t addrlen;
	struct sockaddr_in addr;

	addrlen = (socklen_t)sizeof(struct sockaddr_in);
	memset(&addr, 0, addrlen);

	addr.sin_addr.s_addr = inet_addr(ip);
	addr.sin_family = AF_INET;
	addr.sin_port = htons(atoi(port));

	if (
		(fd = filerail_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1 ||
		filerail_connect(fd, (const struct sockaddr*)&addr, addrlen) == -1
	) {
		goto clean_up;
	}

	printf("[✔️ ] Connected to server\n");

	return fd;

	clean_up:
	filerail_close(fd);
	return -1;
}

int filerail_send(int fd, void *buffer, size_t len, int flags) {
	ssize_t nbytes, cur;

	cur = 0;
	while (len != 0) {
		nbytes = send(fd, buffer + cur, len, flags);
		if (nbytes == -1) {
			perror("socket.h filerail_send");
			return -1;
		}
		len -= nbytes;
		cur += nbytes;
	}
	return 0;
}

int filerail_recv(int fd, void *buffer, size_t len, int flags) {
	ssize_t nbytes, cur;

	cur = 0;
	while (len != 0) {
		nbytes = recv(fd, buffer + cur, len, flags);
		if (nbytes == -1) {
			perror("socket.h filerail_recv");
			return -1;
		}
		len -= nbytes;
		cur += nbytes;
	}
	return 0;
}

int filerail_who(int fd, const char *action) {
	socklen_t addrlen;
	struct sockaddr_in addr;

	if (getpeername(fd, (struct sockaddr*)&addr, &addrlen) == -1) {
		perror("socket.h filerail_who_called_exit");
		return -1;
	}
	printf("%s:%d %s\n", inet_ntoa(addr.sin_addr), addr.sin_port, action);
	return 0;
}

int filerail_sendfile(int fd, const char *zip_filename) {
	int exit_status;
	off_t size;
	size_t nbytes;
	FILE *fp;
	struct stat stat_path;
	unsigned char buffer[BUFFER_SIZE];

	exit_status = 0;

	// open file
	fp = fopen(zip_filename, "rb");
	if (fp == NULL) {
		perror("socket.h filerail_sendfile");
		exit_status = -1;
		goto clean_up;
	}

	if (stat(zip_filename, &stat_path) == -1) {
		perror("socket.h filerail_sendfile");
		exit_status = -1;
		goto clean_up;
	}

	// advertize size of file
	if (filerail_send_resource_header(fd, "\0", "\0", stat_path.st_size) == -1) {
		perror("socket.h filerail_sendfile");
		exit_status = -1;
		goto clean_up;
	}

	size = stat_path.st_size;
  while (size != 0) {
  	nbytes = fread((void *)buffer, 1, min(BUFFER_SIZE, size), fp);
  	if (nbytes != min(BUFFER_SIZE, size)) {
			perror("socket.h filerail_sendfile");
			exit_status = -1;
			goto clean_up;
  	}
  	if (filerail_send(fd, (void *)buffer, nbytes, 0) == -1) {
  		exit_status = -1;
  		goto clean_up;
  	}
  	size -= nbytes;
  }
  fclose(fp);
	return 0;

	clean_up:
	if (fp != NULL) {
		fclose(fp);
	}
	return exit_status;
}

int filerail_recvfile(int fd, const char *zip_filename) {
	int exit_status;
	ssize_t nbytes;
	off_t size;
	FILE *fp;
	unsigned char buffer[BUFFER_SIZE];
	filerail_resource_header resource;

	exit_status = 0;

	fp = fopen(zip_filename, "wb");
	if (fp == NULL) {
		perror("socket.h filerail_recvfile");
		exit_status = -1;
		goto clean_up;
	}

  if (filerail_recv(fd, (void *)&resource, sizeof(resource), MSG_WAITALL) == -1) {
  	exit_status = -1;
  	goto clean_up;
  }

	size = resource.resource_size;
	while (size != 0) {
		nbytes = min(BUFFER_SIZE, size);
		if (filerail_recv(fd, (void *)buffer, nbytes, MSG_WAITALL) == -1) {
			goto clean_up;
		}
		if (fwrite((void *)buffer, 1, nbytes, fp) != nbytes) {
			perror("socket.h filerail_recvfile");
			exit_status = -1;
			goto clean_up;
		}
		size -= nbytes;
	}

	clean_up:
	if (fp != NULL) {
		fclose(fp);
	}
	return exit_status;
}

int filerail_send_command_header(int fd, int type) {
	filerail_command_header command = { type };

	if (filerail_send(fd, (void*)&command, sizeof(command), 0) == -1) {
		return -1;
	}
	return 0;
}

int filerail_send_response_header(int fd, int type) {
	filerail_response_header response = { type };

	if (filerail_send(fd, (void*)&response, sizeof(response), 0) == -1) {
		return -1;
	}
	return 0;
}

int filerail_send_resource_header(int fd, char *name, char *dir, off_t size) {
	filerail_resource_header resource;

	strcpy(resource.resource_name, name);
	strcpy(resource.resource_dir, dir);
	resource.resource_size = size;
	if (filerail_send(fd, (void*)&resource, sizeof(resource), 0) == -1) {
		return -1;
	}
	return 0;
}

#endif
