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
unsigned char key[KEYLENGTH(KEYBITS)];


/* initialize encryption */
void mod_gm_aes_init(char * password) {

    /* pad key till keysize */
    int i;
    for (i = 0; i < 32; i++)
        key[i] = *password != 0 ? *password++ : 0;

    encryption_initialized = 1;
    return;
}


/* encrypt text with given key */
int mod_gm_aes_encrypt(unsigned char ** encrypted, char * text) {

    assert(encryption_initialized == 1);

    unsigned long rk[RKLENGTH(KEYBITS)];
    int nrounds;
    nrounds = rijndaelSetupEncrypt(rk, key, KEYBITS);
    int size = strlen(text);
    int totalsize = size + BLOCKSIZE-size%BLOCKSIZE;
    int i = 0;
    int k = 0;
    unsigned char *enc;
    enc = (unsigned char *) malloc(sizeof(unsigned char)*totalsize);
    while(size > 0) {
        unsigned char plaintext[BLOCKSIZE];
        unsigned char ciphertext[BLOCKSIZE];
        int j;
        for (j = 0; j < BLOCKSIZE; j++) {
            int c = text[i];
            if(c == 0)
                break;
            plaintext[j] = c;
            i++;
        }

        for (; j < BLOCKSIZE; j++)
            plaintext[j] = ' ';
        rijndaelEncrypt(rk, nrounds, plaintext, ciphertext);
        for (j = 0; j < BLOCKSIZE; j++)
            enc[k++] = ciphertext[j];
        size -=BLOCKSIZE;
    }

    *encrypted = enc;
    return totalsize;
}


/* decrypt text with given key */
void mod_gm_aes_decrypt(char ** text, unsigned char * encrypted, int size) {

    assert(encryption_initialized == 1);

    char decr[GM_BUFFERSIZE];
    decr[0] = '\0';
    unsigned long rk[RKLENGTH(KEYBITS)];
    int nrounds;
    nrounds = rijndaelSetupDecrypt(rk, key, KEYBITS);
    int i = 0;
    while(1) {
        unsigned char plaintext[BLOCKSIZE];
        unsigned char ciphertext[BLOCKSIZE];
        int j;
        for (j = 0; j < BLOCKSIZE; j++) {
            int c = encrypted[i];
            ciphertext[j] = c;
            i++;
        }
        rijndaelDecrypt(rk, nrounds, ciphertext, plaintext);
        strncat(decr, (char*)plaintext, BLOCKSIZE);
        size -= BLOCKSIZE;
        if(size < BLOCKSIZE)
            break;
    }

    strcpy(*text, decr);
    return;
}
