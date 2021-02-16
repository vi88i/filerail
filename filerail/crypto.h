#ifndef _CRYPTO_H
#define _CRYPTO_H

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <sys/stat.h>
#include <openssl/aes.h>
#include <openssl/md5.h>

#include "global.h"
#include "constants.h"

typedef struct _filerail_AES_keys {
	uint8_t iv[AES_KEY_SIZE];
	uint8_t key[AES_KEY_SIZE];
} filerail_AES_keys;

char filerail_dec_hex_to_char(uint8_t c);
void filerail_hex_to_str(const uint8_t *hash, char *hex_str);
int filerail_md5(uint8_t *hash, const char *zip_filename);
int filerail_read_AES_keys(char *key_path, filerail_AES_keys *K);
uint8_t filerail_char_to_hex(uint8_t c);
int filerail_encrypt(uint8_t *in, uint8_t *out, size_t nbytes, filerail_AES_keys *K);
int filerail_decrypt(uint8_t *in, uint8_t *out, size_t nbytes, filerail_AES_keys *K);

char filerail_dec_to_hex_char(uint8_t c) {
	switch(c) {
		case 0: return '0';
		case 1: return '1';
		case 2: return '2';
		case 3: return '3';
		case 4: return '4';
		case 5: return '5';
		case 6: return '6';
		case 7: return '7';
		case 8: return '8';
		case 9: return '9';
		case 10: return 'a';
		case 11: return 'b';
		case 12: return 'c';
		case 13: return 'd';
		case 14: return 'e';
		case 15: return 'f';
	}
	return '0';
}

void filerail_hash_to_str(const uint8_t *hash, char *hex_str) {
	int i;

	for (i = 0; i < MD5_DIGEST_LENGTH; i++) {
		hex_str[2 * i] = filerail_dec_to_hex_char((0xf0 & hash[i]) >> 4);
		hex_str[2 * i + 1] = filerail_dec_to_hex_char(0x0f & hash[i]);
	}
}

int filerail_md5(uint8_t *hash, const char *zip_filename) {
	int i, exit_status;
	size_t nbytes;
	off_t size;
	uint8_t buffer[BUFFER_SIZE];
	struct stat stat_path;
	FILE *fp;
	MD5_CTX mdContext;

	exit_status = 0;

	if ((fp = fopen(zip_filename, "rb")) == NULL) {
		LOG(LOG_USER | LOG_ERR, "crypto.h filerail_md5 fopen\n");
		exit_status = -1;
		goto clean_up;
	}

	if (stat(zip_filename, &stat_path) == -1) {
		LOG(LOG_USER | LOG_ERR, "crypto.h filerail_md5 stat\n");
		exit_status = -1;
		return exit_status;
	}

	size = stat_path.st_size;
  MD5_Init(&mdContext);
  while (size != 0) {
  	nbytes = fread((void *)buffer, 1, min(BUFFER_SIZE, size), fp);
  	if (nbytes != min(BUFFER_SIZE, size) && ferror(fp)) {
			LOG(LOG_USER | LOG_ERR, "crypto.h filerail_md5 fread\n");
			exit_status = -1;
			goto clean_up;
  	}
  	MD5_Update(&mdContext, buffer, nbytes);
  	size -= nbytes;
  }
  MD5_Final(hash, &mdContext);

  PRINT(printf("Hash: "));
  for(i = 0; i < MD5_DIGEST_LENGTH; i++) {
  	PRINT(printf("%02x", hash[i]));
  }
  PRINT(printf("\n"));

	clean_up:
	if (fp != NULL) {
		fclose(fp);
	}
	return exit_status;
}

uint8_t filerail_char_to_hex(uint8_t c) {
	switch(c) {
		case 'A':
		case 'a': return 10;
		case 'B':
		case 'b': return 11;
		case 'C':
		case 'c': return 12;
		case 'D':
		case 'd': return 13;
		case 'E':
		case 'e': return 14;
		case 'F':
		case 'f': return 15;
		case '0': return 0;
		case '1': return 1;
		case '2': return 2;
		case '3': return 3;
		case '4': return 4;
		case '5': return 5;
		case '6': return 6;
		case '7': return 7;
		case '8': return 8;
		case '9': return 9;
	}
	return 0;
}

int filerail_read_AES_keys(char *key_path, filerail_AES_keys *K) {
	int i, exit_status, idx;
	uint8_t pair[2];
	FILE *fp;
	uint8_t *ptr;
	struct stat stat_path;

	fp = NULL;
	exit_status = 0;
	for (i = 0; i < AES_KEY_SIZE; i++) {
		K->iv[i] = 0U;
		K->key[i] = 0U;
	}

	if (stat(key_path, &stat_path) == -1) {
		LOG(LOG_USER | LOG_ERR, "crypto.h filerail_read_AES_keys stat\n");
		exit_status = -1;
		goto clean_up;
	}

	if (stat_path.st_size != KEY_FILE_SIZE) {
		LOG(LOG_USER | LOG_INFO, "crypto.h filerail_read_AES_keys invalid key file\n");
		exit_status = -1;
		goto clean_up;
	}

	if ((fp = fopen(key_path, "r")) < 0) {
		LOG(LOG_USER | LOG_ERR, "crypto.h filerail_read_AES_keys fopen\n");
		exit_status = -1;
		goto clean_up;
	} else {
		idx = 0;
		ptr = K->iv;
		while (true) {
			if (fread((void *)pair, 1, 2, fp) == 2) {
				pair[0] = filerail_char_to_hex(pair[0]);
				pair[1] = filerail_char_to_hex(pair[1]);
				ptr[idx++] = (pair[0] << 4) | (pair[1]);
				fgetc(fp);
				if (idx == AES_KEY_SIZE) {
					idx = 0;
					ptr = K->key;
				}
			} else if (ferror(fp)) {
				LOG(LOG_USER | LOG_ERR, "crypto.h filerail_read_AES_keys ferror\n");
				exit_status = -1;
				goto clean_up;
			} else {
				break;
			}
		}
	}

	PRINT(printf("IV: "));
	for (i = 0; i < AES_KEY_SIZE; i++) {
		PRINT(printf("%02x ", K->iv[i]));
	}
	PRINT(printf("\n"));

	PRINT(printf("Key: "));
	for (i = 0; i < AES_KEY_SIZE; i++) {
		PRINT(printf("%02x ", K->key[i]));
	}
	PRINT(printf("\n"));

	clean_up:
	if (fp != NULL) {
		fclose(fp);
	}
	return exit_status;
}

int filerail_encrypt(uint8_t *in, uint8_t *out, size_t nbytes, filerail_AES_keys *K) {
	int ret, exit_status;
	AES_KEY enc_key;
	uint8_t iv[AES_KEY_SIZE];

	exit_status = 0;

	memcpy(iv, K->iv, AES_KEY_SIZE);
	ret = AES_set_encrypt_key(K->key, AES_KEY_SIZE * 8, &enc_key);
	if (ret < 0) {
		LOG(LOG_USER | LOG_INFO, "crypto.h filerail_encrypt AES_set_encrypt_key\n");
		exit_status = -1;
		goto clean_up;
	}

	AES_cbc_encrypt(in, out, nbytes, &enc_key, iv, AES_ENCRYPT);

	clean_up:
	return exit_status;
}

int filerail_decrypt(uint8_t *in, uint8_t *out, size_t nbytes, filerail_AES_keys *K) {
	int ret, exit_status;
	AES_KEY dec_key;
	uint8_t iv[AES_KEY_SIZE];

	exit_status = 0;

	memcpy(iv, K->iv, AES_KEY_SIZE);
	ret = AES_set_decrypt_key(K->key, AES_KEY_SIZE * 8, &dec_key);;
	if (ret < 0) {
		LOG(LOG_USER | LOG_INFO, "crypto.h filerail_decrypt AES_set_decrypt_key\n");
		exit_status = -1;
		goto clean_up;
	}

	AES_cbc_encrypt(in, out, nbytes, &dec_key, iv, AES_DECRYPT);

	clean_up:
	return exit_status;
}

void filerail_test(filerail_AES_keys *K) {
	size_t nbytes = 5;
	uint8_t data[5] = {0x00, 0x01, 0x02, 0x03, 0x04};
	uint8_t enc[5];
	uint8_t dec[5];
	filerail_encrypt(data, enc, nbytes, K);
	for (int i = 0; i < nbytes; i++) {
		printf("%02x", enc[i]);
	}
	printf("\n");
	// filerail_decrypt(enc, dec, nbytes, K);
	// for (int i = 0; i < nbytes; i++) {
	// 	printf("%02x", dec[i]);
	// }
	// printf("\n");
	// assert(memcmp(data, dec, nbytes) == 0);
}

#endif
