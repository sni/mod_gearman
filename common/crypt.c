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
#include <crypt.h>
#include "common.h"

int encryption_initialized = 0;
char key[32];
MCRYPT td;
char * IV;
int blocksize;

/* initialize encryption */
void mod_gm_aes_init(char * password) {
    int keysize = 32;

    if(encryption_initialized == 1) {
        mcrypt_module_close(td);
    }

    /* pad key till keysize */
    int i;
    for (i = 0; i < keysize; i++)
        key[i] = *password != 0 ? *password++ : 0;

    td = mcrypt_module_open("rijndael-256", NULL, "cbc", NULL);
    if (td == MCRYPT_FAILED)
        return;
    blocksize = mcrypt_enc_get_block_size(td);

    IV = malloc(mcrypt_enc_get_iv_size(td));
    for (i=0; i< mcrypt_enc_get_iv_size( td); i++)
        IV[i]=rand();
    i = mcrypt_generic_init( td, key, keysize, IV);
    if (i<0) {
        mcrypt_perror(i);
        return;
    }

    encryption_initialized = 1;
    return;
}


/* encrypt text with given key */
int mod_gm_aes_encrypt(char ** encrypted, char * text) {
    int i = 0;
    int k = 0;
    char *enc;
    int size;
    int totalsize;

    assert(encryption_initialized == 1);

    size      = strlen(text);
    totalsize = size + blocksize-size%blocksize;
    enc       = (char *) malloc(sizeof(char)*totalsize);
    while(size > 0) {
        char plaintext[blocksize];
        int j;
        for (j = 0; j < blocksize; j++) {
            int c = text[i];
            if(c == 0)
                break;
            plaintext[j] = c;
            i++;
        }

        for (; j < blocksize; j++)
            plaintext[j] = '\x0';
printf("a1: '%s'\n", plaintext);
        mcrypt_generic( td, plaintext, blocksize);
printf("a2: '%s'\n", plaintext);
        for (j = 0; j < blocksize; j++)
            enc[k++] = plaintext[j];
        size -=blocksize;
    }

    mcrypt_generic_deinit( td );

    *encrypted = enc;
    return totalsize;
}

/* decrypt text with given key */
void mod_gm_aes_decrypt(char ** text, char * encrypted, int size) {
    char decr[GM_BUFFERSIZE];
    int i = 0;
printf("b0: '%s'\n", encrypted);

    assert(encryption_initialized == 1);

    while(1) {
        char ciphertext[blocksize];
        int j;
        for (j = 0; j < blocksize; j++) {
            int c = encrypted[i];
            ciphertext[j] = c;
//printf("b0: %d '%s'\n", j, ciphertext);
            i++;
        }
printf("b1: '%s'\n", ciphertext);
        mdecrypt_generic (td, ciphertext, blocksize);
printf("b2: '%s'\n", ciphertext);
        strncat(decr, (char*)ciphertext, blocksize);
        size -= blocksize;
        if(size < blocksize)
            break;
    }

    mcrypt_generic_deinit( td );

    strcpy(*text, decr);
}