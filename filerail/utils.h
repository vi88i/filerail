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
#include <sys/types.h>
#include <zip.c>

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
bool filerail_zip_folder(struct zip_t *zip, const char *resource_path);
bool zip_file(struct zip_t *zip, const char *resource_name);
int zip_on_extract_entry(const char *resource_name, void *arg);
bool zip_extract_resource(const char *source_path, const char *destination_path);
int filerail_rm(const char *resource_path);
void filerail_progress_bar(double fraction);
int filerail_mkdir(const char *dir_path);

// check if there is enough storage size (resource size is sent by client)
bool filerail_check_storage_size(off_t resource_size) {
	struct statvfs buf;

	if (statvfs("/", &buf) == -1) {
		LOG(LOG_USER | LOG_ERR, "utils.h filerail_find_available_size statvfs\n");
		return false;
	}
	// man statvfs, you will get it. (Basically checking if there exists enough memory blocks)
	return (resource_size / buf.f_frsize) < buf.f_bavail;
}

// checks if resource if file
bool filerail_is_file(struct stat *stat_resource) {
  return S_ISREG(stat_resource->st_mode) || S_ISLNK(stat_resource->st_mode);
}

// checks if resource is dir
bool filerail_is_dir(struct stat *stat_resource) {
  return S_ISDIR(stat_resource->st_mode);
}

// check if resource exists
bool filerail_is_exists(const char *resource_path, struct stat *stat_resource) {
  if (lstat(resource_path, stat_resource) == -1) {
  	LOG(LOG_USER | LOG_ERR, "utils.h filerail_is_exists lstat\n");
  	return false;
  }
  return true;
}

// check is resource is readable
bool filerail_is_readable(const char *resource_path) {
  return access(resource_path, R_OK) == 0;
}

// check is resource is writeable
bool filerail_is_writeable(const char *resource_path) {
  return access(resource_path, W_OK) == 0;
}

// recursively zip the folder
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
		LOG(LOG_USER | LOG_ERR, "utils.h filerail_zip_folder opendir\n");
		exit_status = false;
		goto clean_up;
	}

	while ((entry = readdir(dir)) != NULL) {
		if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) {
			continue;
		}

		snprintf(path, sizeof(path), "%s/%s", resource_path, entry->d_name);
		if (lstat(path, &s) == -1) {
			exit_status = false;
			goto clean_up;
		}
		// if resource is folder, recursively zip it
		if (S_ISDIR(s.st_mode)) {
			if (!filerail_zip_folder(zip, path)) {
				LOG(LOG_USER | LOG_ERR, "utils.h filerail_zip_folder\n");
				exit_status = false;
				goto clean_up;
			}
		} else {
			if (
				zip_entry_open(zip, path) == -1 ||
				zip_entry_fwrite(zip, path) == -1 ||
				zip_entry_close(zip) == -1
			) {
				LOG(LOG_USER | LOG_ERR, "utils.h filerail_zip_folder\n");
				exit_status = false;
				goto clean_up;
			}
		}
	}

	clean_up:
	if (dir != NULL) {
		closedir(dir);
	}
	return exit_status;
}

// zip single file
bool filerail_zip_file(struct zip_t *zip, const char *resource_path) {
	if (
		zip_entry_open(zip, resource_path) == -1 ||
		zip_entry_fwrite(zip, resource_path) == -1 ||
		zip_entry_close(zip) == -1
	) {
		LOG(LOG_USER | LOG_ERR, "utils.h filerail_zip_file\n");
		return false;
	}
	return true;
}

// call back for zip extract (check deps)
int zip_on_extract_entry(const char *resource_name, void *arg) {
	struct stat stat_resource;

  if (lstat(resource_name, &stat_resource) == -1) {
  	LOG(LOG_USER | LOG_ERR, "utils.h zip_on_extract_entry lstat\n");
  	return -1;
  }
  return stat_resource.st_size;
}

// unzip
bool zip_extract_resource(const char *source_path, const char *destination_path) {
	if (zip_extract(source_path, destination_path, zip_on_extract_entry, NULL) == -1) {
		LOG(LOG_USER | LOG_ERR, "utils.h zip_extract_resource zip_extract\n");
		return false;
	}
	return true;
}

// remove the file/dir
int filerail_rm(const char *resource_path) {
  int exit_status;
  DIR *dir;
  char path[MAX_PATH_LENGTH];
  struct stat stat_path, stat_entry;
  struct dirent *entry;

  dir = NULL;
  exit_status = 0;
  if (lstat(resource_path, &stat_path) == -1) {
  	LOG(LOG_USER | LOG_ERR, "utils.h filerail_rm lstat\n");
  	exit_status = -1;
  	goto clean_up;
  }

  if (S_ISREG(stat_path.st_mode) || S_ISLNK(stat_path.st_mode)) {
	  if (unlink(resource_path) == -1) {
	  	LOG(LOG_USER | LOG_ERR, "utils.h filerail_rm unlink\n");
	  	exit_status = -1;
	  	goto clean_up;
	  }
  } else if (S_ISDIR(stat_path.st_mode)) {
  	// if directory, recursively remove all sub-dir and files
	  if ((dir = opendir(resource_path)) == NULL) {
	  	LOG(LOG_USER | LOG_ERR, "utils.h filerail_rm opendir\n");
	  	exit_status = -1;
	    goto clean_up;
	  }

	  while ((entry = readdir(dir)) != NULL) {
	    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
	    	continue;
	    }

	    strcpy(path, resource_path);
	    strcat(path, "/");
	    strcat(path, entry->d_name);

	    if (lstat(path, &stat_entry) == -1) {
		  	LOG(LOG_USER | LOG_ERR, "utils.h filerail_rm lstat\n");
		  	exit_status = -1;
		  	goto clean_up;
	    }

	    if (S_ISDIR(stat_entry.st_mode)) {
	      if (filerail_rm(path) == -1) {
	      	exit_status = -1;
	      	goto clean_up;
	      }
	      continue;
	    }

	    if (unlink(path) == -1) {
		  	LOG(LOG_USER | LOG_ERR, "operations.h filerail_rm unlink\n");
		  	exit_status = -1;
		    goto clean_up;
	    }
	  }

	  if (rmdir(resource_path) == -1) {
	  	LOG(LOG_USER | LOG_ERR, "operations.h filerail_rm unlink\n");
	  	exit_status = -1;
	    goto clean_up;
	  }
  }

  clean_up:
  if (dir != NULL) {
  	closedir(dir);
  }
  return exit_status;
}

// parse the resource path (gets resource dir and resource name)
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

// get current dir
int filerail_getcwd(char *dir) {
	char *ret;

	ret = getcwd(dir, MAX_PATH_LENGTH);
	if (ret == NULL) {
		LOG(LOG_USER | LOG_ERR, "utils.h filerail_getcwd getcwd\n");
		return -1;
	}
	return 0;
}

// change current dir
int filerail_cd(const char *path) {
	int ret;

	ret = chdir(path);
	if (ret == -1) {
		LOG(LOG_USER | LOG_ERR, "operations.h filerail_cd chdir\n");
		return -1;
	}
	return 0;
}

// progress bar
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
	if ((int)((1.0 - fraction) * 100) == 100) {
		printf("\n");
	}
}

// create a file
int filerail_mkdir(const char *dir_path) {
	if (mkdir(dir_path, 0777) == -1) {
		LOG(LOG_USER | LOG_ERR, "utils.h filerail_mkdir mkdir\n");
		return -1;
	}
	return 0;
}

#endif
