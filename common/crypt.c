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
unsigned char key[32];
MCRYPT td;
char * IV;

/* initialize encryption */
void mod_gm_aes_init(char * password) {
    int iv_size;

    /* pad key till keysize */
    int i;
    for (i = 0; i < 32; i++)
        key[i] = *password != 0 ? *password++ : 0;

    td=mcrypt_module_open(MCRYPT_RIJNDAEL_256,NULL,"cbc",NULL);
    iv_size=mcrypt_enc_get_iv_size(td);
    IV=(char *)malloc(iv_size);
    mcrypt_generic_init(td,key,32,IV);

    encryption_initialized = 1;
    return;
}


/* encrypt text with given key */
int mod_gm_aes_encrypt(char ** encrypted, char * text) {
    int totalsize = strlen(text);
    *encrypted = strdup(text);
    mcrypt_generic( td, *encrypted, totalsize);
    return totalsize;
}


/* decrypt text with given key */
void mod_gm_aes_decrypt(char ** text, char * encrypted, int size) {
    *text = strdup(encrypted);
    mdecrypt_generic( td, text, size);
    return;
}
