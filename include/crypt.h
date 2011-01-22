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
#include "rijndael.h"

#define KEYBITS     256     /**< key size */
#define BLOCKSIZE    16     /**< block size for encryption */

/**
 * initialize crypto module
 *
 * @param[in] password - encryption key
 *
 * @return nothing
 */
void mod_gm_aes_init(char * password);

/**
 * encrypt text
 *
 * @param[out] encrypted - pointer to encrypted text
 * @param[in] text       - text which should be encrypted
 *
 * @return size of encrypted text
 */
int mod_gm_aes_encrypt(unsigned char ** encrypted, char * text);

/**
 * decrypt text
 *
 * @param[out] decrypted - pointer to decrypted text
 * @param[in] encrypted  - text which should be decrypted
 * @param[in] size       - size of encrypted text
 *
 * @return nothing
 */
void mod_gm_aes_decrypt(char ** decrypted, unsigned char * encrypted, int size);

/*
 * @}
 */
