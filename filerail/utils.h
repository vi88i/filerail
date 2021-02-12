#ifndef _UTIL_H
#define _UTIL_H

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include "../deps/kuba__zip/zip.c"

#include "global.h"
#include "constants.h"
#include "protocol.h"

bool filerail_check_storage_size(off_t resource_size);
bool filerail_is_file(struct stat *stat_resource);
bool filerail_is_dir(struct stat *stat_resource);
bool filerail_is_exists(const char *resource_path, struct stat *stat_resource);
bool filerail_is_resource_readable(const char *resource_path);
bool filerail_is_resource_writeable(const char *resource_path);
bool filerail_parse_resource_path(const char *resource_path, char *resource_name, char *resource_dir);
int filerail_getcwd(char *dir);
int filerail_cd(const char *path);
bool filerail_zip_create(const char *resource_path);
bool filerail_zip_folder(struct zip_t *zip, const char *resource_path);
bool zip_file(struct zip_t *zip, const char *resource_name);
int zip_on_extract_entry(const char *resource_name, void *arg);
bool zip_extract_resource(const char *source_path, const char *destination_path);
int filerail_rm(const char *resource_path);
void filerail_progress_bar(double fraction);

bool filerail_check_storage_size(off_t resource_size) {
	struct statvfs buf;

	if (statvfs("/", &buf) == -1) {
		LOG(LOG_USER | LOG_ERR, "utils.h filerail_find_available_size statvfs");
		return false;
	}
	return (resource_size / buf.f_frsize) < buf.f_bavail;
}

bool filerail_is_file(struct stat *stat_resource) {
  return S_ISREG(stat_resource->st_mode) || S_ISLNK(stat_resource->st_mode);
}

bool filerail_is_dir(struct stat *stat_resource) {
  return S_ISDIR(stat_resource->st_mode);
}

bool filerail_is_exists(const char *resource_path, struct stat *stat_resource) {
  if (lstat(resource_path, stat_resource) == -1) {
  	LOG(LOG_USER | LOG_ERR, "utils.h filerail_is_exists lstat");
  	return false;
  }
  return true;
}

bool filerail_is_readable(const char *resource_path) {
  return access(resource_path, R_OK) == 0;
}

bool filerail_is_writeable(const char *resource_path) {
  return access(resource_path, W_OK) == 0;
}

bool filerail_zip_folder(struct zip_t *zip, const char *resource_path) {
	bool exit_status;
	DIR *dir;
	struct dirent *entry;
	char path[MAX_PATH_LENGTH];
	struct stat s;

	exit_status = true;
	memset(path, 0, MAX_PATH_LENGTH);
	dir = opendir(resource_path);
	if (dir == NULL) {
		LOG(LOG_USER | LOG_ERR, "utils.h filerail_zip_folder opendir");
		exit_status = false;
		goto cleanup;
	}

	while ((entry = readdir(dir)) != NULL) {
		if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) {
			continue;
		}

		snprintf(path, sizeof(path), "%s/%s", resource_path, entry->d_name);
		if (lstat(path, &s) == -1) {
			exit_status = false;
			goto cleanup;
		}
		if (S_ISDIR(s.st_mode)) {
			if (filerail_zip_folder(zip, path) == -1) {
				LOG(LOG_USER | LOG_ERR, "utils.h filerail_zip_folder");
				exit_status = false;
				goto cleanup;
			}
		} else {
			if (
				zip_entry_open(zip, path) == -1 ||
				zip_entry_fwrite(zip, path) == -1 ||
				zip_entry_close(zip) == -1
			) {
				LOG(LOG_USER | LOG_ERR, "utils.h filerail_zip_folder");
				exit_status = false;
				goto cleanup;
			}
		}
	}

	cleanup:
	if (dir != NULL) {
		closedir(dir);
	}
	return exit_status;
}

bool filerail_zip_file(struct zip_t *zip, const char *resource_path) {
	if (
		zip_entry_open(zip, resource_path) == -1 ||
		zip_entry_fwrite(zip, resource_path) == -1 ||
		zip_entry_close(zip) == -1
	) {
		LOG(LOG_USER | LOG_ERR, "utils.h filerail_zip_file");
		return false;
	}
	return true;
}

bool filerail_zip_create(const char *resource_path) {
  bool exit_status;
  size_t zip_filename_len;
  char zip_filename[MAX_RESOURCE_LENGTH];
  struct zip_t *zip;

  exit_status = true;
	strcpy(zip_filename, resource_path);
	strcat(zip_filename, ".zip");

  if ((zip = zip_open(zip_filename, ZIP_DEFAULT_COMPRESSION_LEVEL, 'w')) == NULL) {
		LOG(LOG_USER | LOG_ERR, "utils.h zip_create zip_open");
		exit_status = false;
		goto cleanup;
  }

  cleanup:
  zip_close(zip);
  return exit_status;
}

int zip_on_extract_entry(const char *resource_name, void *arg) {
	struct stat stat_resource;

  if (lstat(resource_name, &stat_resource) == -1) {
  	LOG(LOG_USER | LOG_ERR, "utils.h zip_on_extract_entry lstat");
  	return -1;
  }
  return stat_resource.st_size;
}

bool zip_extract_resource(const char *source_path, const char *destination_path) {
	if (zip_extract(source_path, destination_path, zip_on_extract_entry, NULL) == -1) {
		LOG(LOG_USER | LOG_ERR, "utils.h zip_extract_resource zip_extract");
		return false;
	}
	return true;
}

int filerail_rm(const char *resource_path) {
  int exit_status;
  size_t path_len;
  DIR *dir;
  char path[MAX_PATH_LENGTH];
  struct stat stat_path, stat_entry;
  struct dirent *entry;

  exit_status = 0;
  if (lstat(resource_path, &stat_path) == -1) {
  	LOG(LOG_USER | LOG_ERR, "utils.h filerail_rm lstat");
  	exit_status = -1;
  	goto cleanup;
  }

  if (S_ISREG(stat_path.st_mode) || S_ISLNK(stat_path.st_mode)) {
	  if (unlink(resource_path) == -1) {
	  	LOG(LOG_USER | LOG_ERR, "utils.h filerail_rm unlink");
	  	exit_status = -1;
	  	goto cleanup;
	  }
  } else if (S_ISDIR(stat_path.st_mode)) {
	  if ((dir = opendir(resource_path)) == NULL) {
	  	LOG(LOG_USER | LOG_ERR, "utils.h filerail_rm opendir");
	  	exit_status = -1;
	    goto cleanup;
	  }

	  path_len = strlen(resource_path);
	  while ((entry = readdir(dir)) != NULL) {
	    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
	    	continue;
	    }

	    strcpy(path, resource_path);
	    strcat(path, "/");
	    strcat(path, entry->d_name);

	    if (lstat(path, &stat_entry) == -1) {
		  	LOG(LOG_USER | LOG_ERR, "utils.h filerail_rm lstat");
		  	exit_status = -1;
		  	goto cleanup;
	    }

	    if (S_ISDIR(stat_entry.st_mode)) {
	      if (filerail_rm(path) == -1) {
	      	exit_status = -1;
	      	goto cleanup;
	      }
	      continue;
	    }

	    if (unlink(path) == -1) {
		  	LOG(LOG_USER | LOG_ERR, "operations.h filerail_rm unlink");
		  	exit_status = -1;
		    goto cleanup;
	    }
	  }

	  if (rmdir(resource_path) == -1) {
	  	LOG(LOG_USER | LOG_ERR, "operations.h filerail_rm unlink");
	  	exit_status = -1;
	    goto cleanup;
	  }
  }

  cleanup:
  if (dir != NULL) {
  	closedir(dir);
  }
  return exit_status;
}

bool filerail_parse_resource_path(const char *resource_path, char *resource_name, char *resource_dir) {
	int i;
	size_t path_len = strlen(resource_path);

	if (path_len > MAX_PATH_LENGTH - 1) {
		printf("Length of resource path exceeded MAX_PATH_LENGTH\n");
		return false;
	}

	for (i = path_len - 1; i > 0; i--) {
		if (resource_path[i] == '/') {
			break;
		}
	}

	resource_name[0] = '\0';
	resource_dir[0] = '\0';
	if (i == -1) {
		printf("Resource path must be absolute\n");
		return false;
	}
	if (path_len - i - 1 > MAX_RESOURCE_LENGTH - 1) {
		printf("Length of resource name exceeded MAX_RESOURCE_LENGTH\n");
		return false;
	}
	memcpy(resource_name, resource_path + i + 1, path_len - i - 1);
	if (i == 0) {
		strcpy(resource_dir, "/");
	} else {
		memcpy(resource_dir, resource_path, i);
	}
	return true;
}

int filerail_getcwd(char *dir) {
	char *ret;

	ret = getcwd(dir, MAX_PATH_LENGTH);
	if (ret == NULL) {
		LOG(LOG_USER | LOG_ERR, "utils.h filerail_getcwd getcwd");
		return -1;
	}
	return 0;
}

int filerail_cd(const char *path) {
	int ret;

	ret = chdir(path);
	if (ret == -1) {
		LOG(LOG_USER | LOG_ERR, "operations.h filerail_cd chdir");
		return -1;
	}
	return 0;
}

void filerail_progress_bar(double fraction) {
	int x, cur;

	cur = PROGRESS_BAR_WIDTH * (1 - fraction);
	printf("\r\t[");
	for (x = 1; x <= cur; x++) {
		printf("#");
	}
	for (x = 1; x <= PROGRESS_BAR_WIDTH - cur; x++) {
		printf(" ");
	}
	printf("] (%d%%)", (int)((1.0 - fraction) * 100));
}

#endif
