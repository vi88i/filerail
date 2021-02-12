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

#include "global.h"
#include "constants.h"
#include "protocol.h"
#include "utils.h"
#include "aes128.h"

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
int filerail_sendfile(int fd, const char *zip_filename, AES_keys *K, off_t offset);
int filerail_recvfile(
	int fd,
	const char *zip_filename,
	AES_keys *K,
	off_t offset,
	const char *ckpt_resource_path,
	const char *resource_path
);

static int filerail_socket(int domain, int type, int protocol) {
	int fd;

	fd = socket(domain, type, protocol);
	if (fd == -1) {
		LOG(LOG_USER | LOG_ERR, "socket.h filerail_socket socket");
	}
	return fd;
}

static int filerail_bind(int fd, const struct sockaddr *addr, socklen_t addrlen) {
	int ret;

	ret = bind(fd, addr, addrlen);
	if (ret == -1) {
		LOG(LOG_USER | LOG_ERR, "socket.h filerail_bind bind");
	}
	return ret;
}

static int filerail_connect(int fd, const struct sockaddr *addr, socklen_t addrlen) {
	int ret;

	ret = connect(fd, addr, addrlen);
	if (ret == -1) {
		LOG(LOG_USER | LOG_ERR, "socket.h filerail_connect connect");
	}
	return ret;
}

static int filerail_listen(int fd, int backlog) {
	int ret;

	ret = listen(fd, backlog);
	if (ret == -1) {
		LOG(LOG_USER | LOG_ERR, "socket.h filerail_listen listen");
	}
	return ret;
}

static int filerail_setsockopt(int fd, int level, int option, const void *optval, socklen_t optlen) {
	int ret;

	ret = setsockopt(fd, level, option, optval, optlen);
	if (ret == -1) {
		LOG(LOG_USER | LOG_ERR, "socket.h filerail_setsockopt setsockopt");
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
		LOG(LOG_USER | LOG_ERR, "socket.h filerail_accept accept");
	}
	return clifd;
}

int filerail_close(int fd) {
	int ret;

	ret = -1;
	if (filerail_is_fd_valid(fd)) {
		ret = close(fd);
		if (ret == -1) {
			LOG(LOG_USER | LOG_ERR, "socket.h filerail_close");
		}
	}
	return ret;
}

int filerail_create_tcp_server(char *ip, char *port) {
	int fd;
	const int optval = 1;
	socklen_t addrlen;
	struct sockaddr_in addr;
	struct timeval tv;

	addrlen = (socklen_t)sizeof(struct sockaddr_in);
	memset(&addr, 0, addrlen);

	addr.sin_addr.s_addr = inet_addr(ip);
	addr.sin_family = AF_INET;
	addr.sin_port = htons(atoi(port));

	tv.tv_sec = TIME_OUT;
	tv.tv_usec = 0;

	if (
		(fd = filerail_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1 ||
		filerail_bind(fd, (const struct sockaddr*)&addr, addrlen) == -1 ||
		filerail_setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const void*)&optval, sizeof(const int)) == -1 ||
		filerail_setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, (const void*)&optval, sizeof(const int)) == -1 ||
		filerail_setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (const void*)&tv, sizeof(tv)) == -1 ||
		filerail_setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, (const void*)&tv, sizeof(tv)) == -1 ||
		filerail_listen(fd, BACKLOG) == -1
	) {
		goto clean_up;
	}

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

	PRINT(printf("[✔️ ] Connected to server\n"));

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
			LOG(LOG_USER | LOG_ERR, "socket.h filerail_send send");
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
			LOG(LOG_USER | LOG_ERR, "socket.h filerail_recv recv");
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
		LOG(LOG_USER | LOG_ERR, "socket.h filerail_who_called_exit getpeername");
		return -1;
	}
	PRINT(printf("%s:%d %s\n", inet_ntoa(addr.sin_addr), addr.sin_port, action));
	return 0;
}

int filerail_sendfile(int fd, const char *zip_filename, AES_keys *K, off_t offset) {
	int exit_status;
	off_t size, total;
	size_t nbytes;
	FILE *fp;
	struct stat stat_path;
	filerail_data_packet data;

	fp = NULL;
	exit_status = 0;

	// open file
	fp = fopen(zip_filename, "rb");
	if (fp == NULL) {
		LOG(LOG_USER | LOG_ERR, "socket.h filerail_sendfile fopen");
		exit_status = -1;
		goto clean_up;
	}

	if (stat(zip_filename, &stat_path) == -1) {
		LOG(LOG_USER | LOG_ERR, "socket.h filerail_sendfile stat");
		exit_status = -1;
		goto clean_up;
	}

	// advertize size of file
	if (filerail_send_resource_header(fd, "\0", "\0", stat_path.st_size) == -1) {
		LOG(LOG_USER | LOG_ERR, "socket.h filerail_sendfile filerail_send_resource_header");
		exit_status = -1;
		goto clean_up;
	}

	total = size = stat_path.st_size;
	size -= offset;

	rewind(fp);
	if (fseek(fp, offset, SEEK_CUR) == -1) {
		LOG(LOG_USER | LOG_ERR, "socket.h filerail_sendfile fseek");
		exit_status = -1;
		goto clean_up;
	}

  while (size != 0) {
  	nbytes = fread((void *)data.data_payload, 1, min(BUFFER_SIZE, size), fp);
  	if (nbytes != min(BUFFER_SIZE, size) && ferror(fp)) {
			LOG(LOG_USER | LOG_ERR, "socket.h filerail_sendfile fread");
			exit_status = -1;
			goto clean_up;
  	}
  	data.data_padding = AES_CTR(data.data_payload, nbytes, K);
  	data.data_size = nbytes;
  	if (filerail_send(fd, (void *)&data, sizeof(data), 0) == -1) {
  		exit_status = -1;
  		goto clean_up;
  	}
  	size -= nbytes;
  	PRINT(filerail_progress_bar(size / (1.0 * total)));
  }

	clean_up:
	PRINT(printf("\n"));
	if (fp != NULL) {
		fclose(fp);
	}
	return exit_status;
}

int filerail_recvfile(
	int fd,
	const char *zip_filename,
	AES_keys *K,
	off_t offset,
	const char *ckpt_resource_path,
	const char *resource_path
	)
{
	int exit_status;
	ssize_t nbytes;
	off_t size, total;
	FILE *fp, *fckpt;
	filerail_resource_header resource;
	filerail_data_packet data;
	filerail_checkpoint ckpt;

	fp = fckpt = NULL;
	exit_status = 0;
	strcpy(ckpt.resource_path, resource_path);

	fp = fopen(zip_filename, "wb");
	if (fp == NULL) {
		LOG(LOG_USER | LOG_ERR, "socket.h filerail_recvfile fopen");
		exit_status = -1;
		goto clean_up;
	}

	fckpt = fopen(ckpt_resource_path, "wb");
	if (fckpt == NULL) {
		LOG(LOG_USER | LOG_ERR, "socket.h filerail_recvfile fopen");
		exit_status = -1;
		goto clean_up;
	}

  if (filerail_recv(fd, (void *)&resource, sizeof(resource), MSG_WAITALL) == -1) {
  	exit_status = -1;
  	goto clean_up;
  }

	total = size = resource.resource_size;
	size -= offset;

	rewind(fp);
	if (fseek(fp, offset, SEEK_CUR) == -1) {
		LOG(LOG_USER | LOG_ERR, "socket.h filerail_sendfile fseek");
		exit_status = -1;
		goto clean_up;
	}

	ckpt.offset = offset;
	while (size != 0) {
		if (filerail_recv(fd, (void *)&data, sizeof(data), MSG_WAITALL) == -1) {
			goto clean_up;
		}
		nbytes = data.data_size;
		AES_CTR(data.data_payload, nbytes + data.data_padding, K);
		if (fwrite((void *)data.data_payload, 1, nbytes, fp) != nbytes && ferror(fp)) {
			LOG(LOG_USER | LOG_ERR, "socket.h filerail_recvfile fwrite");
			exit_status = -1;
			goto clean_up;
		}
		fflush(fp);
		ckpt.offset += nbytes;
		rewind(fckpt);
		if (fwrite((void *)&ckpt, 1, sizeof(ckpt), fckpt) != sizeof(ckpt)) {
			LOG(LOG_USER | LOG_ERR, "socket.h filerail_recvfile fwrite");
			exit_status = -1;
			goto clean_up;
		}
		fflush(fckpt);
  	size -= nbytes;
  	PRINT(filerail_progress_bar(size / (1.0 * total)););
	}

	clean_up:
	PRINT(printf("\n"));
	if (fp != NULL) {
		fclose(fp);
	}
	if (fckpt != NULL) {
		fclose(fckpt);
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
