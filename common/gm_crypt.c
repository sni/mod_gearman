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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include <gm_crypt.h>
#include "common.h"

int encryption_initialized = 0;
unsigned char key[KEYBYTES];

/* initialize encryption */
EVP_CIPHER_CTX * mod_gm_aes_init(const char * password) {
    EVP_CIPHER_CTX * ctx;

    /* pad key till keysize */
    int i;
    for (i = 0; i < KEYBYTES; i++)
        key[i] = *password != 0 ? *password++ : 0;

    /* Create and initialise the context */
    if(!(ctx = EVP_CIPHER_CTX_new())) {
        fprintf(stderr, "EVP_CIPHER_CTX_new failed:\n");
        ERR_print_errors_fp(stderr);
        exit(1);
    }
    // disable padding, this has to be done manually. For historical reasons, mod-gearman uses zero padding which
    // is not supported by openssl
    EVP_CIPHER_CTX_set_padding(ctx, 0);

    encryption_initialized = 1;
    return(ctx);
}

/* deinitialize encryption */
void mod_gm_aes_deinit(EVP_CIPHER_CTX *ctx) {
    if(ctx != NULL)
        EVP_CIPHER_CTX_free(ctx);
    ctx = NULL;

    encryption_initialized = 0;
    return;
}


/* encrypt text with given key */
int mod_gm_aes_encrypt(EVP_CIPHER_CTX * ctx, unsigned char * ciphertext, const unsigned char * plaintext, int plaintext_len) {
    int len;
    int ciphertext_len;

    assert(encryption_initialized == 1);
    assert(ctx != NULL);

    if(1 != EVP_EncryptInit_ex(ctx, EVP_aes_256_ecb(), NULL, key, NULL)) {
        fprintf(stderr, "EVP_EncryptInit_ex failed:\n");
        ERR_print_errors_fp(stderr);
        return -1;
    }

    if(1 != EVP_EncryptUpdate(ctx, ciphertext, &len, plaintext, plaintext_len)) {
        fprintf(stderr, "EVP_EncryptUpdate failed\n");
        ERR_print_errors_fp(stderr);
        return -1;
    }

    ciphertext_len = len;

    // do zero padding
    if(BLOCKSIZE%plaintext_len != 0) {
        const char * zeros = "\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0";
        if(1 != EVP_EncryptUpdate(ctx, ciphertext+len, &len, (const unsigned char *)zeros, BLOCKSIZE - (plaintext_len % BLOCKSIZE))) {
            fprintf(stderr, "EVP_EncryptUpdate failed\n");
            ERR_print_errors_fp(stderr);
            return -1;
        }
        ciphertext_len += len;
    }

    if(1 != EVP_EncryptFinal_ex(ctx, ciphertext + ciphertext_len, &len)) {
        fprintf(stderr, "EVP_EncryptFinal_ex failed\n");
        ERR_print_errors_fp(stderr);
        return -1;
    }

    return ciphertext_len;
}


/* decrypt text with given key */
int mod_gm_aes_decrypt(EVP_CIPHER_CTX * ctx, unsigned char * plaintext, unsigned char * ciphertext, int ciphertext_len) {
    int len;

    assert(encryption_initialized == 1);
    assert(ctx != NULL);

    if(1 != EVP_DecryptInit_ex(ctx, EVP_aes_256_ecb(), NULL, key, NULL)) {
        fprintf(stderr, "EVP_DecryptInit_ex failed\n");
        ERR_print_errors_fp(stderr);
        return -1;
    }

    if(1 != EVP_DecryptUpdate(ctx, plaintext, &len, ciphertext, ciphertext_len)) {
        fprintf(stderr, "EVP_DecryptUpdate failed\n");
        ERR_print_errors_fp(stderr);
        return -1;
    }

    return 1;
}

/* create hex sum for char[] */
char *mod_gm_hexsum(const char *text) {
    unsigned char *result = NULL;
    unsigned int resultlen = -1;
    unsigned int i = 0;
    char *hex = gm_malloc(sizeof(char)*((KEYBYTES*2)+1));
    result = HMAC(EVP_sha256(), key, KEYBYTES, (const unsigned char*)text, strlen(text), result, &resultlen);
    for(i = 0; i < resultlen; i++){
        snprintf(hex+(i*2), 3, "%02hhX", result[i]);
    }
    return(hex);
}

int base64_decode(const char *source, int sourcelen, unsigned char * target) {
    int n = EVP_DecodeBlock(target, (const unsigned char*)source, sourcelen);
    if(n == -1) {
        fprintf(stderr, "base64 decode failed: ");
        ERR_print_errors_fp(stderr);
        fprintf(stderr, "\n");
        return(-1);
    }
    return(n);
}

unsigned char * base64_encode(const unsigned char *source, size_t sourcelen) {
    unsigned char * target = gm_malloc(sizeof(char) * ((sourcelen/3)*4)+5);
    if(!EVP_EncodeBlock(target, source, sourcelen)) {
        fprintf(stderr, "base64 encode failed: ");
        ERR_print_errors_fp(stderr);
        fprintf(stderr, "\n");
        return(NULL);
    }
    return(target);
}
