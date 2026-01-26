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

unsigned char key[KEYBYTES];

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#define TLS _Thread_local
#elif defined(__GNUC__)
#define TLS __thread
#else
#error "No thread-local storage support"
#endif

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

    return(ctx);
}

/* deinitialize encryption */
void mod_gm_aes_deinit(EVP_CIPHER_CTX *ctx) {
    if(ctx != NULL)
        EVP_CIPHER_CTX_free(ctx);
    ctx = NULL;

    return;
}


/* encrypt text with given key */
int mod_gm_aes_encrypt(EVP_CIPHER_CTX * ctx, unsigned char * ciphertext, const unsigned char * plaintext, int plaintext_len) {
    int len;
    int ciphertext_len;

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

    if(len < 0)  {
        fprintf(stderr, "EVP_DecryptUpdate return length: %d\n", len);
        return -1;
    }

    return 1;
}

static TLS EVP_MAC     *mac      = NULL;
static TLS EVP_MAC_CTX *mac_ctx  = NULL;
int hmac_sha256_init() {
    OSSL_PARAM params[] = {
        OSSL_PARAM_construct_utf8_string(OSSL_MAC_PARAM_DIGEST,
                                         (char *)"SHA256", 0),
        OSSL_PARAM_construct_end()
    };

    if (mac == NULL) {
        mac = EVP_MAC_fetch(NULL, "HMAC", NULL);
        if (!mac)
            return 0;
    }

    if (mac_ctx == NULL) {
        mac_ctx = EVP_MAC_CTX_new(mac);
        if (!mac_ctx)
            return 0;
    }

    return EVP_MAC_init(mac_ctx, key, KEYBYTES, params) == 1;
}

/* create hex sum for char[] */
void mod_gm_hexsum(char *dest, char *text) {
    uint8_t result[32] = {0};
    size_t resultlen = 0;
    unsigned int i = 0;

    hmac_sha256_init();
    if(mac_ctx == NULL) {
        fprintf(stderr, "failed to initialize HMAC context\n");
        exit(1);
    }

    if(EVP_MAC_update(mac_ctx, (const unsigned char*)text, strlen(text)) != 1 || EVP_MAC_final(mac_ctx, result, &resultlen, KEYBYTES) != 1) {
        fprintf(stderr, "HMAC computation failed\n");
        exit(1);
    }

    dest[0] = 0;
    for(i = 0; i < resultlen; i++){
        snprintf(dest+(i*2), 3, "%02hhX", result[i]);
    }

    return;
}

int base64_decode(const char *source, int sourcelen, unsigned char * target) {
    int n = EVP_DecodeBlock(target, (const unsigned char*)source, sourcelen);
    if(n == -1) {
        // try again and strip newlines, base64 decode fails if there are any newlines in the base64 string
        char *stripped = gm_malloc(sizeof(char) * sourcelen);
        int j = 0;
        int i = 0;
        for(i = 0; i < sourcelen; i++) {
            if(source[i] != '\n') {
                stripped[j++] = source[i];
            }
        }
        stripped[j] = '\0';
        n = EVP_DecodeBlock(target, (const unsigned char*)stripped, strlen(stripped));
        gm_free(stripped);
        if(n == -1) {
            fprintf(stderr, "base64 decode failed: ");
            ERR_print_errors_fp(stderr);
            fprintf(stderr, "\n");
            return(-1);
        }
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
