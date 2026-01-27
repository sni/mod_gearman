/******************************************************************************
 *
 * mod_gearman - distribute checks with gearman
 *
 * Copyright (c) 2010 Sven Nierlein - sven.nierlein@consol.de
 *
 * This file is part of mod_gearman.
 *
 *  mod_gearman is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  mod_gearman is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with mod_gearman.  If not, see <http://www.gnu.org/licenses/>.
 *
 *****************************************************************************/

/** @file
 *  @brief crypt module
 *
 * contains the utility functions for en/decryption
 *
 * @{
 */

#include <stdio.h>
#include <string.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
#include <openssl/core_names.h>
#endif
#include <openssl/err.h>

#define KEYBITS     256     /**< key size */
#define KEYBYTES     32     /* char size */
#define BLOCKSIZE    16     /**< block size for encryption */

/**
 * initialize crypto module
 *
 * @param[in] password - encryption key
 *
 * @return openssl ctx
 */
EVP_CIPHER_CTX * mod_gm_aes_init(const char * password);

/**
 * deinitialize crypto module
 *
 * @return nothing
 */
void mod_gm_aes_deinit(EVP_CIPHER_CTX *);

/**
 * encrypt text
 *
 * @param[in] ctx           - openssl context (from mod_gm_aes_init())
 * @param[out] ciphertext   - pointer to encrypted text
 * @param[in] plaintext     - text which should be encrypted
 * @param[in] plaintext_len - length of plain text
 *
 * @return size of encrypted text
 */
int mod_gm_aes_encrypt(EVP_CIPHER_CTX * ctx, unsigned char * ciphertext, const unsigned char * plaintext, int plaintext_len);

/**
 * decrypt text
 *
 * @param[in] ctx        - openssl context (from mod_gm_aes_init())
 * @param[out] decrypted - pointer to decrypted text
 * @param[in] encrypted  - text which should be decrypted
 * @param[in] size       - size of encrypted text
 *
 * @return 1 on success
 */
int mod_gm_aes_decrypt(EVP_CIPHER_CTX * ctx, unsigned char * plaintext, unsigned char * ciphertext, int ciphertext_len);

/**
 * create hex sum of text
 *
 * @param[out] dest - pointer to hex sum
 * @param[in] text  - source text for check sum
 *
 * @return nothing
 */
void mod_gm_hexsum(char *dest, char *text);

/**
 * decode base64 encoded data
 *
 * @param source the encoded data (zero terminated)
 * @param sourcelen the size of the input text
 * @param target the target char array
 * @return number of bytes decoded or -1 on error
 */
int base64_decode(const char *source, int sourcelen, unsigned char * target);

/**
 * encode an array of bytes using Base64
 *
 * @param source the source buffer
 * @param sourcelen the length of the source buffer
 * @return encodede bytes or NULL on error
 */
unsigned char * base64_encode(const unsigned char *source, size_t sourcelen);

int md5sum_init(void);

/*
 * @}
 */
