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
#include <netdb.h>
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
static int filerail_set_timeout(int fd, int level, int option, int s, int u);
int filerail_dns_resolve(char *hostname);
int firerail_accept(int fd, struct sockaddr *addr, socklen_t *addrlen);
int filerail_close(int fd);
int filerail_create_tcp_server(char *ip, char *port);
int filerail_connect_to_tcp_server(char *ip, char *port);
int filerail_who(int fd, const char *action);
int filerail_send_command_header(int fd, int type);
int filerail_send_response_header(int fd, int type);
int filerail_send_resource_header(int fd, char *name, char *dir, off_t size);
int filerail_sendfile(int fd, const char *zip_filename, AES_keys *K, off_t offset);
int filerail_recvfile(int fd, const char *zip_filename, AES_keys *K, off_t offset, const char *ckpt_resource_path, const char *resource_path);

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

/*
	check if fd is valid:
	if F_GETFD returns 0 (then fd is valid)
	else if fcntl() returned -1 and errno != EBADFD (bad file descriptor) then fd is valid
*/
static int filerail_is_fd_valid(int fd) {
	return fcntl(fd, F_GETFD) != -1 || errno != EBADFD;
}

// set timeout on receive or send
static int filerail_set_timeout(int fd, int level, int option, int s, int u) {
	struct timeval tv;

	tv.tv_sec = s;
	tv.tv_usec = u;

	if (filerail_setsockopt(fd, level, option, (const void*)&tv, sizeof(tv)) == -1) {
		return -1;
	}
	return 0;
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

// spins a server for 10 hours (why 10 hours, pretty random, or just to save CPU consumption if you are not using it)
int filerail_create_tcp_server(char *ip, char *port) {
	int fd;
	const int optval = 1;
	socklen_t addrlen;
	struct sockaddr_in addr;


	addrlen = (socklen_t)sizeof(struct sockaddr_in);
	memset(&addr, 0, addrlen);

	addr.sin_addr.s_addr = inet_addr(ip);
	addr.sin_family = AF_INET;
	addr.sin_port = htons(atoi(port));

	if (
		(fd = filerail_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1 ||
		filerail_bind(fd, (const struct sockaddr*)&addr, addrlen) == -1 ||
		filerail_setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const void*)&optval, sizeof(const int)) == -1 ||
		filerail_setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, (const void*)&optval, sizeof(const int)) == -1 ||
		filerail_set_timeout(fd, SOL_SOCKET, SO_RCVTIMEO, TIME_OUT, 0) ||
		filerail_set_timeout(fd, SOL_SOCKET, SO_SNDTIMEO, TIME_OUT, 0) ||
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
		if (nbytes <= 0) {
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
		// if nbytes == 0 => sender disconnected
		if (nbytes <= 0) {
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
	filerail_response_header response;

	fp = NULL;
	exit_status = 0;

	// open the resource
	fp = fopen(zip_filename, "rb");
	if (fp == NULL) {
		LOG(LOG_USER | LOG_ERR, "socket.h filerail_sendfile fopen");
		exit_status = -1;
		goto clean_up;
	}

	// stat the resource
	if (stat(zip_filename, &stat_path) == -1) {
		LOG(LOG_USER | LOG_ERR, "socket.h filerail_sendfile stat");
		exit_status = -1;
		goto clean_up;
	}

	// advertise the size of resource
	if (filerail_send_resource_header(fd, "\0", "\0", stat_path.st_size) == -1) {
		LOG(LOG_USER | LOG_ERR, "socket.h filerail_sendfile filerail_send_resource_header");
		exit_status = -1;
		goto clean_up;
	}

	// adjust the size using offset
	total = size = stat_path.st_size;
	size -= offset;

	// seek the file pointer
	rewind(fp);
	if (fseek(fp, offset, SEEK_CUR) == -1) {
		LOG(LOG_USER | LOG_ERR, "socket.h filerail_sendfile fseek");
		exit_status = -1;
		goto clean_up;
	}

	// set timeout of recv
	if (filerail_set_timeout(fd, SOL_SOCKET, SO_RCVTIMEO, MAX_IO_TIME_OUT, 0) == -1) {
		exit_status = -1;
		goto clean_up;
	}
  while (size != 0) {
  	// read from file
  	nbytes = fread((void *)data.data_payload, 1, min(BUFFER_SIZE, size), fp);
  	/*
  		usually fread(...nb) == nb
  		if nb != fread(...nb) and feof(fp) (no errors)
  		else nb != fread(...nb) and ferror(fp) (some error occured)
  	*/
  	if (nbytes != min(BUFFER_SIZE, size) && ferror(fp)) {
			LOG(LOG_USER | LOG_ERR, "socket.h filerail_sendfile fread");
			exit_status = -1;
			goto clean_up;
  	}
  	// encrypt
  	data.data_padding = AES_CTR(data.data_payload, nbytes, K);
  	data.data_size = nbytes;
  	// send the data packet
  	if (filerail_send(fd, (void *)&data, sizeof(data), 0) == -1) {
  		exit_status = -1;
  		goto clean_up;
  	}
  	// subtract the bytes sent
  	size -= nbytes;
  	// wait for OK
  	if (filerail_recv(fd, (void *)&response, sizeof(response), MSG_WAITALL) == -1) {
  		exit_status = -1;
  		goto clean_up;
  	}
  	if (response.response_type != OK) {
  		LOG(LOG_USER | LOG_INFO, "No ACK\n");
  		exit_status = -1;
  		goto clean_up;
  	}
  	PRINT(filerail_progress_bar(size / (1.0 * total)));
  }

	clean_up:
	PRINT(printf("\n"));
	if (fp != NULL) {
		fclose(fp);
	}
	// reset the timeout
	if (filerail_set_timeout(fd, SOL_SOCKET, SO_RCVTIMEO, TIME_OUT, 0) == -1) {
		exit_status = -1;
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
	char tmp_ckpt_resource_path[MAX_PATH_LENGTH];
	filerail_resource_header resource;
	filerail_data_packet data;
	filerail_checkpoint ckpt;

	fp = fckpt = NULL;
	exit_status = 0;
	ckpt.resource_path[0] = '\0';
	strcpy(ckpt.resource_path, resource_path);
	tmp_ckpt_resource_path[0] = '\0';
	strcpy(tmp_ckpt_resource_path, ckpt_resource_path);
	strcat(tmp_ckpt_resource_path, ".tmp");

	// open the resource
	fp = fopen(zip_filename, "ab");
	if (fp == NULL) {
		LOG(LOG_USER | LOG_ERR, "socket.h filerail_recvfile fopen");
		exit_status = -1;
		goto clean_up;
	}

	// receive the size
  if (filerail_recv(fd, (void *)&resource, sizeof(resource), MSG_WAITALL) == -1) {
  	exit_status = -1;
  	goto clean_up;
  }

  // adjust the size using offset read from checkpoint
	total = size = resource.resource_size;
	size -= offset;

	// initialize the checkpoint struct
	ckpt.offset = offset;
	// set the timeout
	if (filerail_set_timeout(fd, SOL_SOCKET, SO_RCVTIMEO, MAX_IO_TIME_OUT, 0) == -1) {
		exit_status = -1;
		goto clean_up;
	}
	while (size != 0) {
		// receive the data packet
		if (filerail_recv(fd, (void *)&data, sizeof(data), MSG_WAITALL) == -1) {
			exit_status = -1;
			goto clean_up;
		}
		// decrypt
		nbytes = data.data_size;
		// remember to include the data padding (else AES decryption fails)
		AES_CTR(data.data_payload, nbytes + data.data_padding, K);
		if (fwrite((void *)data.data_payload, 1, nbytes, fp) != nbytes && ferror(fp)) {
			LOG(LOG_USER | LOG_ERR, "socket.h filerail_recvfile fwrite");
			exit_status = -1;
			goto clean_up;
		}
		// flush anything in the stream, so that it writes immediately (best practice ;) )
		fflush(fp);
		// update the offset
		ckpt.offset += nbytes;
		// write the checkpoint to tmp file
		fckpt = fopen(tmp_ckpt_resource_path, "wb");
		if (fckpt == NULL) {
			LOG(LOG_USER | LOG_ERR, "socket.h filerail_recvfile fopen");
			exit_status = -1;
			goto clean_up;
		}
		if (fwrite((void *)&ckpt, 1, sizeof(ckpt), fckpt) != sizeof(ckpt)) {
			LOG(LOG_USER | LOG_ERR, "socket.h filerail_recvfile fwrite");
			exit_status = -1;
			goto clean_up;
		}
		fflush(fckpt);
		// it is important to close the file, before renaming
		fclose(fckpt);
		fckpt = NULL;
		/*
			Why rename?
			fwrite(checkpoint) is not atomic. If we directly attempt to write checkpoint using fwrite
			and there is a hardware/software failure, the checkpoint file will be corrupted.
			So we will write the checkpoint to tmp file, and rename tmp file to required name.
			rename() is atomic. So we can be completely sure that checkpoint is not corrupted
		*/
		if (rename(tmp_ckpt_resource_path, ckpt_resource_path) == -1) {
			LOG(LOG_USER | LOG_ERR, "socket.h filerail_recvfile rename");
			exit_status = -1;
			goto clean_up;
		}
  	size -= nbytes;
		if (filerail_send_response_header(fd, OK) == -1) {
			exit_status = -1;
			goto clean_up;
		}
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
	if (filerail_set_timeout(fd, SOL_SOCKET, SO_RCVTIMEO, TIME_OUT, 0) == -1) {
		exit_status = -1;
	}
	return exit_status;
}

// send command header
int filerail_send_command_header(int fd, int type) {
	filerail_command_header command = { type };

	if (filerail_send(fd, (void*)&command, sizeof(command), 0) == -1) {
		return -1;
	}
	return 0;
}

// send response header
int filerail_send_response_header(int fd, int type) {
	filerail_response_header response = { type };

	if (filerail_send(fd, (void*)&response, sizeof(response), 0) == -1) {
		return -1;
	}
	return 0;
}

// send resource header
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

// dns resolver
int filerail_dns_resolve(char *hostname) {
	struct hostent *info;

	info = gethostbyname(hostname);
	if (info == NULL) {
		LOG(LOG_USER | LOG_ERR, "DNS query failed\n");
		return -1;
	}
	if (info->h_addrtype != AF_INET) {
		LOG(LOG_USER | LOG_INFO, "Failed to get IPv4 address\n");
		return -1;
	}
	if (info->h_addr_list[0] == NULL) {
		LOG(LOG_USER | LOG_INFO, "Didn't find IPv4 address\n");
		return -1;
	}
	hostname = inet_ntoa(*(struct in_addr*)(info->h_addr_list[0]));
	return 0;
}

#endif
